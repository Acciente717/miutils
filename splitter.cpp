/**
 * Copyright [2019] Zhiyao Ma
 * 
 * This module implements a finite state machine. It searches for four
 * patterns: "<", "</", ">" and "/>" to split the input file.
 * The input XML file is assumed to be in the following format:
 * 
 * <$top_level_tag> ... </$top_level_tag>
 * <$top_level_tag> ... </$top_level_tag>
 * ...
 * <$top_level_tag> ... </$top_level_tag>
 * 
 * The finite state machine uses the above four patterns to split
 * the input file into several std::string. Each std::string will contain
 * one subtree begining with "<$top_level_tag>", i.e. each std::string will
 * look like "<$top_level_tag> ... </$top_level_tag>".
 * 
 * This module is designed to run as fast as possible. It splits the input
 * file lexically, rather than gramatically. In other words, it does not
 * verify the input file to have correct XML format. It makes decisions
 * on seeing "<", "</", ">" and "/>".
 */

#include "splitter.hpp"
#include "extractor.hpp"
#include "global_states.hpp"
#include "exceptions.hpp"
#include "parameters.hpp"
#include "macros.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>

/// The states of the finite state machine.
enum class MachineState {
    /// The starting state. It means that we are not in the middle of a
    /// tag. If it reads ">", it will return to AngleClosed state.
    AngleClosed,

    /// AngleOpen means it has just read in a "<". Depending on the following
    /// character, which may be "/" or not, the state may be transfered to
    /// ClosingSubtree or CreatingSubtree. If the following character is "/",
    /// it means we have just read "</". It is closing a subtree. Otherwise,
    /// it is creating a new subtree beginning with "<".
    AngleOpen,

    /// CreatingField means we are in the middle of a tag, and it MIGHT be
    /// creating a field, but not a subtree, i.e. <some_tag attr=x />. This
    /// state is entered from CreatingSubtree upon seeing "/". However, if the
    /// next character is not ">", it means our GUESS is wrong and we are not
    /// reading "/>", and should return to CreatingSubtree.
    CreatingField,

    /// CreatingSubtree means we are in the middle of a tag beginning with "<",
    /// but not "</". If we see ">" in this state, it means the tag finishes,
    /// and we are now in its subtree. We should return to AngleClosed and
    /// increase the depth counter. If we see "/" in this state, we MIGHT be
    /// reading the first character of "/>". We should then transfer to
    /// CreatingField state.
    CreatingSubtree,

    /// CLosingSubtree means we are in the middle of a tag beginning with "/<".
    /// If we see ">" in this state, it means the tag finishes, and we are
    /// exiting a subtree. We should then return to AngleClosed state and
    /// decrease the depth counter.
    ClosingSubtree
};

/// The entrance function of the (sub)thread running the lexical splitter.
static void smain_splitter();

/// The thread object that runs the lexical splitter.
static std::thread g_splitter_thread;

/// The flag indicating we are terminating the lexical splitter prematurely,
/// probably because of some error.
static std::atomic<bool> g_early_terminating(false);

/// The index number of the current file being worked on, i.e. the index
/// to the `g_inputs` vector.
static int g_current_file_idx = 0;

/// Current line number of the file being processed.
static long g_current_line_number = 1;

/// The line number that corresponds to the start of the XML string
/// currently being processed.
static long g_start_line_number = 0;

/// Provide the input file name and start running the lexical splitter.
void start_splitter() {
    g_splitter_thread = std::thread(smain_splitter);
    g_early_terminating = false;
    g_current_file_idx = 0;
}

/// Join the thread running the lexical splitter.
void join_splitter() {
    if (g_splitter_thread.joinable()) {
        g_splitter_thread.join();
    }
}

/// Prematurely stop the lexical splitter. It does NOT join the thread.
/// One should call join_splitter() after calling this function.
void kill_splitter() {
    g_early_terminating.store(false);
}

/// When the splitter has finished execution, it calls this funcion
/// to notify the main thread about this.
static void notify_main_thread() {
    std::lock_guard<std::mutex> guard(g_main_state_mtx);

    // If the main state is AllRunning, we should change it to
    // SplitterFinished and notify the main thread.
    if (g_main_state == MainState::AllRunning) {
        g_main_state = MainState::SplitterFinished;
        g_main_state_change_cv.notify_one();

    // If the main state is Error, we should keep it and do nothing.
    // Otherwise, we are in the wrong state and it is bug.
    } else if (g_main_state != MainState::Error) {
        throw ProgramBug(
            "All extractors have just finished execution. "
            "The main state should be either SplitterFinished "
            "or Error, but is neither."
        );
    }
}

/// The entrance function of the (sub)thread running the lexical splitter.
static void smain_splitter() {
    try {
        // The string that contains "<$top_level_tag> ... </$top_level_tag>".
        std::string xml_subtree;

        // The counter of read strings.
        long job_num = 0;

        // Continue to loop unless exiting prematurely.
        while (!g_early_terminating.load()) {
            // Get a piece of the file in the form
            // "<$top_level_tag> ... </$top_level_tag>".
            xml_subtree = next_ptree_string();

            // If the returned string is empty, it means we have reached the
            // end of file. Break the loop.
            if (xml_subtree.empty()) {
                break;
            }

            // Otherwize, send it to the exetractors.
            produce_job_to_extractor({
                job_num++,
                std::move(xml_subtree),
                g_input_file_names[g_current_file_idx],
                g_start_line_number,
                g_current_line_number
            });
        }

        // If we are not exiting prematurely, we should notify the main thread
        // that the splitter has finished all its work.
        if_unlikely (!g_early_terminating) {
            notify_main_thread();
        }
    } catch (...) {
        propagate_exeption_to_main();
    }
}

/// Actions that should be taken on AngleClosed state.
inline static void machine_action_angle_closed(MachineState &state,
                                        int &depth,
                                        char c) {
    switch (c) {
    case '<':
        state = MachineState::AngleOpen;
        break;
    default:
        break;
    }
}

/// Actions that should be taken on AngleOpen state.
inline static void machine_action_angle_open(MachineState &state,
                                      int &depth,
                                      char c) {
    switch (c) {
    case '/':
        state = MachineState::ClosingSubtree;
        break;
    default:
        state = MachineState::CreatingSubtree;
        break;
    }
}

/// Actions that should be taken on CreatingField state.
inline static void machine_action_creating_field(MachineState &state,
                                          int &depth,
                                          char c) {
    switch (c) {
    case '>':
        state = MachineState::AngleClosed;
        break;
    default:
        state = MachineState::CreatingSubtree;
        break;
    }
}

/// Actions that should be taken on CreatingSubtree state.
inline static void machine_action_creating_subtree(MachineState &state,
                                            int &depth,
                                            char c) {
    switch (c) {
    case '>':
        state = MachineState::AngleClosed;
        ++depth;
        break;
    case '/':
        state = MachineState::CreatingField;
        break;
    default:
        break;
    }
}

/// Actions that should be taken on ClosingSubtree state.
inline static void machine_action_closing_subtree(MachineState &state,
                                           int &depth,
                                           char c) {
    switch (c) {
    case '>':
        state = MachineState::AngleClosed;
        --depth;
        break;
    default:
        break;
    }
}

/// Read a char from the buffer. Fill the buffer by reading from the
/// input if the buffer runs out. Return true if successfully get a new
/// character. It returns false to indicate we have reach the end of file.
static bool buffered_getchar(char &c, std::istream &input) {
    static char buf[READ_BUFF_SIZE];
    // The index of the next character to be read.
    static int idx = 0;
    // The length of the buffered data.
    static int end = 0;

    // Read more from the input file if the buffer runs out.
    if (idx == end) {
        input.read(buf, READ_BUFF_SIZE);
        idx = 0;
        end = input.gcount();

        // Return false if we reach EOF.
        if (end == 0) {
            return false;
        }
    }

    c = buf[idx++];
    return true;
}

/// Get the next subtree in the opened XML file. The returned string is a
/// slice of the input XML file in the form like
/// "<$top_level_tag> ... </$top_level_tag>". Note that since the splitter
/// is only running on the lexical level, rather than the grammar level,
/// it assumes that the input file is in valid XML format. It defers the
/// validation of the format to the following modules, where an exception
/// will be raised if the returned string is malformated because the input
/// is not a valid XML file.
std::string next_ptree_string() {
    // Current input character.
    char c;

    // The string to be returned.
    std::string tree;

    // The depth in the grammar tree.
    int depth = 0;

    // If we have finished processing all the input files, return an empty
    // string to indicate that.
    if (g_current_file_idx == g_inputs.size()) {
        return tree;
    }

    // The input stream we are currently working on.
    auto &file = g_inputs[g_current_file_idx];

    // Skip characters until we see an "<".
    while (buffered_getchar(c, *file) && c != '<') {
        if (c == '\n') ++g_current_line_number;
    }

    // If the previous loop stopped because we saw "<", search for the
    // matching ending tag and returns the string containing everyting between
    // the starting tag and ending tag. Otherwize, the previous loop stopped
    // because there was no more input character. Return an empty string.
    // Note that `tree` is now an empty string.
    if (c == '<') {
        // Set the state to the starting state.
        MachineState state = MachineState::AngleClosed;

        // Update starting line number of current XML string.
        g_start_line_number = g_current_line_number;

        // Run the finite state machine.
        while (true) {
            // Store each character we see.
            tree.push_back(c);

            // Take actions according to the current state and the input.
            switch (state) {
            case MachineState::AngleClosed:
                machine_action_angle_closed(state, depth, c);
                break;
            case MachineState::AngleOpen:
                machine_action_angle_open(state, depth, c);
                break;
            case MachineState::CreatingField:
                machine_action_creating_field(state, depth, c);
                break;
            case MachineState::CreatingSubtree:
                machine_action_creating_subtree(state, depth, c);
                break;
            case MachineState::ClosingSubtree:
                machine_action_closing_subtree(state, depth, c);
                break;
            }

            // If we have found the ending tag, stop the finite state machine.
            if (depth == 0 && state == MachineState::AngleClosed) {
                break;
            }

            // Read the next character in the file. If we fail to read the next
            // character, the file is corrupted. We defer to the extractor to
            // throw an exception.
            if_unlikely (!buffered_getchar(c, *file)) {
                break;
            }

            if (c == '\n') {
                ++g_current_line_number;
            }
        }
        return tree;

    // Otherwise, we have finished this file. Go to the next.
    } else {
        ++g_current_file_idx;
        g_current_line_number = 1;
        return next_ptree_string();
    }
}
