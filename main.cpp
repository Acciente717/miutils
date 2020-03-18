/**
 * Copyright [2019] Zhiyao Ma
 * 
 * This is a XML parser. It defines a sequence of actions associated
 * with predicates. For each tree in the input XML files, it goes through
 * the action list. The first action whose predicate function yields true
 * will be taken.
 * 
 * The action list is defined in action_list.cpp.
 * 
 * The XML parser contains three modules. The first is a splitter, which
 * runs a finite state machine to split the input files into strings. The
 * second module contains a thread pool of extractors, which scan through
 * the action list. The third module is an in-order executor, which execute
 * output functions in the same order as the input. As the second module
 * is multi-threaded, the later part of the input file may be finished
 * processing prior to the some former part, thus we introduce the third
 * module to guarantee the order of the output.
 */

#include "action_list.hpp"
#include "extractor.hpp"
#include "splitter.hpp"
#include "in_order_executor.hpp"
#include "global_states.hpp"
#include "exceptions.hpp"
#include "parameters.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <boost/type_index.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>

/// Sub main function running a state machine monitoring the global state.
/// During normal execution, the state should move following the path:
///
/// Initializing -> AllRunning -> SplitterFinished -> ExtractorFinished
/// -> InOrderExecutorFinished
///
/// Any state may transfer to Error, if any sub thread throws an
/// exception. In Error state, it will kill and join all sub thread and
/// then rethrows that exception to main function.
static void smain() {
    std::unique_lock<std::mutex> main_state_lck(g_main_state_mtx);
    g_main_state = MainState::Initializing;

    // Run the state machine. It will exit on InOrderExecutorFinished state.
    while (true) {
        switch (g_main_state) {
        // Start all sub threads.
        case MainState::Initializing:
            g_main_state = MainState::AllRunning;
            main_state_lck.unlock();
            start_splitter();
            start_extractor();
            start_in_order_executor();
            main_state_lck.lock();
            break;
        // All sub threads are running. Nothing to do. Just wait.
        case MainState::AllRunning:
            g_main_state_change_cv.wait(
                main_state_lck,
                []{ return g_main_state != MainState::AllRunning; }
            );
            break;
        // The splitter has finished. Join the spitter thread and notify
        // the extractors that the splitter has finished.
        case MainState::SplitterFinished:
            main_state_lck.unlock();
            join_splitter();
            notify_splitter_finished();
            main_state_lck.lock();
            g_main_state_change_cv.wait(
                main_state_lck,
                []{ return g_main_state != MainState::SplitterFinished; }
            );
            break;
        // All the extractor have finished. Join the extractor threads and
        // notify the in-ordr executor that all the extractors have finished.
        case MainState::ExtractorFinished:
            main_state_lck.unlock();
            join_extractor();
            notify_extractor_finished();
            main_state_lck.lock();
            g_main_state_change_cv.wait(
                main_state_lck,
                []{ return g_main_state != MainState::ExtractorFinished; }
            );
            break;
        // The in-order executor has finished. Join the in-order executor
        // thread and return to main.
        case MainState::InOrderExecutorFinished:
            main_state_lck.unlock();
            join_in_order_executor();
            return;
        // One of the sub thread has caught an exception. Kill all sub threads
        // and rethrow the exception to main.
        case MainState::Error:
            main_state_lck.unlock();
            kill_splitter();
            kill_extractor();
            kill_in_order_executor();
            join_splitter();
            join_extractor();
            join_in_order_executor();
            if (g_pexcept != nullptr) {
                std::rethrow_exception(g_pexcept);
            } else {
                throw ProgramBug(
                    "Main state was changed to Error, but the global exception "
                    "pointer is still nullptr!"
                );
            }
            break;
        }
    }
}

/// Print the type of the exception and its message.
template <typename T>
void show_exception_message(T &e) {
    auto exception_type = boost::typeindex::type_id<T>().pretty_name();
    std::cerr << "Caught an exception of type ["
              << exception_type << "]" << std::endl;
    std::cerr << "Exception message:" << std::endl;
    std::cerr << e.what() << std::endl;
}

/// Parse command line options and arguments, and set the global variables
/// accordingly.
static void parse_option(int argc, char **argv) {
    namespace po = boost::program_options;

    /// All visible options (those can be seen by --help).
    po::options_description visible_opts("Options");
    visible_opts.add_options()
        ("help,h", "Produce help message.\n")
        ("thread,j", po::value<int>()->default_value(THREAD_DEFAULT),
            "Set the thread number of the extractors.\n")
        ("output,o", po::value<std::string>(),
            "Set the output file name (default to stdout).\n")
        ("range", po::value<std::string>(),
            "Enable range mode. "
            "Set the timestamp range file path. Each line in the file "
            "should contains two unix timestamps separated by a space. "
            "The output only keeps packets that lie in the "
            "time intervals provided by the range file. "
            "This option is mutually exclusive "
            "with the \"extract\" mode.\n")
        ("extract", po::value<std::string>(),
            "Enable extractor mode.\n"
            "Example: \"--enable rrc_ota,lte_phy_pdsch\".\n\n"
            "Available extractors:\n"
            "[rrc_ota, rrc_serv_cell_info, pdcp_cipher_data_pdu, "
            "nas_emm_ota_incoming, nas_emm_ota_outgoing, "
            "mac_rach_attempt, mac_rach_trigger, "
            "phy_pdsch_stat, phy_pdsch, "
            "phy_serv_cell_meas, action_pdcp_cipher_data_pdu, "
            "rlc_dl_am_all_pdu, rlc_ul_am_all_pdu, "
            "all_packet_type].\n\n"
            "Note that each packet goes through the enabled "
            "extractors list. Only the first matched extractor "
            "will be executed. That means, if `all_packet_type` "
            "is set as the first extractor, then only this will "
            "be effective, which shadows all subsequent extractors.\n\n"
            "Extractors preceeding with \"action_\" is a compound "
            "function that operate across packets, and might "
            "interfere with other extractors. DO NOT simultaneously "
            "enable those which have conflict.\n"
            "Known conflicts:\n"
            "1. \"action_pdcp_cipher_data_pdu\" against "
            "\"pdcp_cipher_data_pdu.\"\n\n"
            "This option is mutially exclusive "
            "with the \"range\" mode.\n")
        ("dedup",
            "Enable deduplicate mode.\n\n"
            "For each packet, it will be printed to the output if "
            "and only if its timestamp is no less than all previously "
            "seen packets.\n")
        ("reorder", po::value<long>(),
            "Enable reorder mode. "
            "Specify the size of reorder window in microseconds.\n\n"
            "For each pair of packets P and Q, if P occurs before "
            "Q in the file but the timestamp of P is greater than Q, "
            "then it is a reverse pair. If the difference of the "
            "timestamp between P and Q is less than the given "
            "reorder window size, then Q is guaranteed to precede "
            "P in the output.");

    /// All internal options. (Arguments are automatically transformed to
    /// the --input option.)
    po::options_description all_opts("All Options");
    all_opts.add(visible_opts);
    all_opts.add_options()
        ("input,i", po::value< std::vector<std::string> >()->composing());
    po::positional_options_description p;
    p.add("input", -1);

    /// Build the option map and parse options.
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
        options(all_opts).positional(p).run(), vm);
    po::notify(vm);

    /// If --help or -h is set, show the help message and exit.
    if (vm.count("help")) {
        std::cout << "Usage: " + std::string(argv[0])
                  << " [options] [input_file ...]" << std::endl;
        std::cout << "If no input file is provided, it reads from stdin."
                  << std::endl;
        std::cout << visible_opts << std::endl;
        exit(0);
    }

    /// If the --thread or -f option is set, set to the global variable
    /// accordingly. Otherwise set it to the default value.
    if (vm.count("thread")) {
        g_thread_num = vm["thread"].as<int>();
        /// If the thread number exceeds the limit, throw an exception.
        if (g_thread_num <= 0 || g_thread_num > THREAD_LIMIT) {
            throw ArgumentError(
                "Invalid thread number. It should be between 1 and 256."
            );
        }
    } else {
        g_thread_num = THREAD_DEFAULT;
    }

    /// If we have any input argument, open the file and store it to the
    /// global vector.
    if (vm.count("input")) {
        const auto &inputs = vm["input"].as< std::vector<std::string> >();
        for (const auto &i : inputs) {
            auto file = std::unique_ptr<std::istream,
                                        std::function<void(std::istream*)>>(
                new std::ifstream(i),
                std::default_delete<std::istream>()
            );
            if (file->fail()) {
                throw ArgumentError(
                    "Failed to open input file: "
                    + ("\"" + i + "\"")
                );
            }
            g_inputs.emplace_back(std::move(file));
            g_input_file_names.emplace_back(i);
        }
    /// If we have no input argument, set the stdin as the only input file.
    } else {
        auto file = std::unique_ptr<std::istream,
                                    std::function<void(std::istream*)>>(
            &std::cin,
            [](std::istream *p) {}
        );
        g_inputs.emplace_back(std::move(file));
        g_input_file_names.emplace_back("stdin");
    }

    /// If we have an output argument, open the file and store it to the
    /// global variable.
    if (vm.count("output")) {
        const auto &output = vm["output"].as<std::string>();
        auto file = std::unique_ptr<std::ostream,
                                    std::function<void(std::ostream*)>>(
            new std::ofstream(output),
            std::default_delete<std::ostream>()
        );
        if (file->fail()) {
            throw ArgumentError(
                    "Failed to open output file: "
                    + ("\"" + output + "\"")
            );
        }
        g_output = std::move(file);
    /// Otherwise, use stdout as the output file.
    } else {
        auto file = std::unique_ptr<std::ostream,
                                    std::function<void(std::ostream*)>>(
            &std::cout,
            [](std::ostream *p) {}
        );
        g_output = std::move(file);
    }

    // One and only one of the --range, --extract or --dedup must be set.
    auto mode_cnt = vm.count("range") + vm.count("extract")
                  + vm.count("dedup") + vm.count("reorder");
    if(mode_cnt == 0) {
        throw ArgumentError(
            "None of the \"extract\", \"range\",  \"dedup\" "
            "and \"reorder\" mode is enabled."
        );
    } else if (mode_cnt > 1) {
        throw ArgumentError(
            "Only one of the \"extract\", \"range\", \"dedup\" "
            "and \"reorder\" mode can be enabled at a time."
        );
    }

    // If the range file is provided, read and store them to
    // the global vector.
    if (vm.count("range")) {
        const auto &filename = vm["range"].as<std::string>();
        auto file = std::ifstream(filename);
        if (file.fail()) {
            throw ArgumentError(
                "Failed to open range file: "
                + ("\"" + filename + "\"")
            );
        }
        time_t left, right;
        while (file >> left >> right) {
            g_valid_time_range.emplace_back(left, right);
        }
        initialize_action_list_with_range();

    // If the extract mode is enabled, perpare the extractor list.
    } else if (vm.count("extract")) {
        // Split the string by ",", and stores them to the global
        // vector.
        auto extractors_str = vm["extract"].as<std::string>();
        size_t start = 0, end;
        while (true) {
            end = extractors_str.find(',', start);
            auto len = end == std::string::npos
                     ? std::string::npos
                     : end - start;
            g_enabled_extractors.emplace_back(
                extractors_str.substr(start, len)
            );
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
        initialize_action_list_with_extractors();
    // If the dedup mode is enabled, setup the action list correspondingly.
    } else if (vm.count("dedup")) {
        initialize_action_list_to_dedup();
    // If the reorder mode is enabled, setup the reorder window
    // and initialize the action list correspondingly.
    } else if (vm.count("reorder")) {
        auto size = vm["reorder"].as<long>();
        g_reorder_window.reset(new ReorderWindow(size));
        initialize_action_list_to_reorder();
    }
}

/// Do clean up work before exiting.
static void cleanup() {
    /// If we are in the reorder mode, print out everything
    /// left in the reorder window.
    if (g_reorder_window != nullptr) {
        g_reorder_window->flush();
    }
}

int main(int argc, char **argv) {
    try {
        parse_option(argc, argv);
        smain();
        cleanup();
    } catch (UnexpectedCase &e) {
        show_exception_message(e);
        return 1;
    } catch (ArgumentError &e) {
        show_exception_message(e);
        return 1;
    } catch (boost::program_options::error &e) {
        show_exception_message(e);
        return 1;
    } catch (boost::property_tree::ptree_bad_path &e) {
        show_exception_message(e);
        return 1;
    } catch (boost::property_tree::ptree_bad_data &e) {
        show_exception_message(e);
        return 1;
    } catch (boost::property_tree::ptree_error &e) {
        show_exception_message(e);
        return 1;
    } catch (ProgramBug &e) {
        show_exception_message(e);
        return 1;
    } catch (std::exception& e) {
        show_exception_message(e);
        return 1;
    } catch (...) {
        std::cerr << "Caught an unknown exception!" << std::endl;
    }

    return 0;
}
