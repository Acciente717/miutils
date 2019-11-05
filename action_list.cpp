/**
 * Copyright [2019] Zhiyao Ma
 * 
 * This module defines all actions that need to be taken by the extractor.
 * 
 * Each `ConditionalAction` contains a predicate and an action function.
 * The predicate function is called first, and if it yields true, the action
 * function will then be called.
 * 
 * All `CondiitionalAction`s are stored in a list. Note that ONLY THE FIRST
 * action function in the list whose corresponding predicate function yields
 * true will be called. All predicate and action functions after it will be
 * skipped.
 * 
 * HOW TO ADD A NEW ACTION:
 * 1. Write a predicate function with signature
 *    (const pt::ptree &, const Job &) -> bool
 * 2. Write a action function with signature
 *    (pt::ptree &&, Job &&) -> void
 * 3. Push them to `g_action_list` in the function `initialize_action_list`.
 *    Note that the last predicate function MUST yield true.
 * 4. Make sure that if you want to output anything in the action function,
 *    wrap it in the lambda and pass it to the in-order executor by calling
 *    `insert_ordered_task`. See `print_timestamp` for example.
 */
#include "action_list.hpp"
#include "in_order_executor.hpp"
#include "global_states.hpp"
#include "exceptions.hpp"
#include "extractor.hpp"
#include "macros.hpp"
#include <string>
#include <iostream>

static bool recursive_find_mobility_control_info(const pt::ptree &tree);
static void print_time_of_mobility_control_info(pt::ptree &&tree, Job &&job);
static void print_timestamp(pt::ptree &&tree, long seq_num);

static bool is_rrc_ota_packet(const pt::ptree &tree, const Job &job);
static void extract_rrc_ota_packet(pt::ptree &&tree, Job &&job);

static bool is_rrc_serv_cell_info_packet(const pt::ptree &tree, const Job &job);
static void extract_rrc_serv_cell_info_packet(pt::ptree &&tree, Job &&job);

static bool is_pdcp_cipher_data_pdu_packet(
    const pt::ptree &tree, const Job &job);
static void extract_pdcp_cipher_data_pdu_packet(pt::ptree &&tree, Job &&job);

/// The list storing all `ConditionalAction`s.
ActionList g_action_list;


/// Initialize the `g_action_list`. It pushes `ConditionalActions` to
/// the list. Note that we need to push a guard predicate function which
/// always yields true at the end of the list. This is because each action
/// function acts as the producer to the in-order executor module. That module
/// executes all output tasks in sequence (by looking at the sequence number).
/// Even if we have no output on the current input XML tree, we should
/// produce a dummy output task, which essentially output nothing, to the
/// in-order executor module.
void initialize_action_list() {
    // Below is an example.
    // Predicate: find the "mobilityControlInfo is present" string recursively.
    // Action: print the timestamp of this packet.
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, const Job &job) {
                return recursive_find_mobility_control_info(tree);
            },
            print_time_of_mobility_control_info
        }
    );

    g_action_list.push_back(
        {
            is_rrc_ota_packet, extract_rrc_ota_packet
        }
    );

    g_action_list.push_back(
        {
            is_rrc_serv_cell_info_packet, extract_rrc_serv_cell_info_packet
        }
    );

    // g_action_list.push_back(
    //   {
    //       is_pdcp_cipher_data_pdu_packet, extract_pdcp_cipher_data_pdu_packet
    //   }
    // );

    // Predicate: always true
    // Action: do nothing
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, const Job &job) { return true; },
            [](pt::ptree &&tree, Job &&job) {
                insert_ordered_task(job.job_num, []{});
            }
        }
    );
}

// Recursively scan through the input XML tree. Yield true if it sees
// a field that contains "mobilityControlInfo is present" as a substring.
static bool recursive_find_mobility_control_info(const pt::ptree &tree) {
    if (tree.data().find("mobilityControlInfo is present")
        != std::string::npos) {
        return true;
    }
    for (const auto &i : tree) {
        if (recursive_find_mobility_control_info(i.second)) {
            return true;
        }
    }
    return false;
}

/// Print the timestamp that located at path
/// "dm_log_packet.pair.<xmlattr>.key.timestamp". It throws an exception if
/// the path is invalid.
static void print_timestamp(pt::ptree &&tree, long seq_num) {
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get<std::string>("<xmlattr>.key") == "timestamp") {
                auto timestamp = i.second.data();
                insert_ordered_task(seq_num, [timestamp] {
                    (*g_output) << timestamp << std::endl;
                });
                break;
            }
        }
    }
}


static void print_time_of_mobility_control_info(pt::ptree &&tree, Job &&job) {
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get<std::string>("<xmlattr>.key") == "timestamp") {
                auto timestamp = i.second.data();
                insert_ordered_task(job.job_num, [timestamp] {
                    (*g_output) << "[" << timestamp
                                << "] [mobilityControlInfo] $ " << std::endl;
                });
                break;
            }
        }
    }
}

/// Return true if and only if the tree has the following structure:
/// <dm_log_packet>
///     ...
///     <pair key="type_id">LTE_RRC_OTA_Packet</pair>
///     ...
/// <dm_log_packet>
static bool is_rrc_ota_packet(const pt::ptree &tree, const Job &job) {
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get("<xmlattr>.key", std::string()) == "type_id"
                && i.second.data() == "LTE_RRC_OTA_Packet") {
                return true;
            }
        }
    }
    return false;
}

/// Start from the root `tree`, recursively find the following subtrees:
/// <some_tag attribute_name=attribute_value ... >
///     ...
/// </some_tag>
/// Return the pointers to the roots of the subtrees, i.e. all the subtrees
/// starting at `some_tag` with the attribute name `attribute_name` and
/// attribute value `attribute_value`.
static std::vector<pt::ptree*> locate_subtree_with_attribute(
    pt::ptree &tree,
    const std::string &attribute_name,
    const std::string &attribute_value
) {
    std::vector<pt::ptree*> subtrees;
    for (auto &i : tree) {
        if (i.first == "<xmlattr>") {
            for (auto &j : i.second) {
                if (j.first == attribute_name
                    && j.second.data() == attribute_value) {
                    subtrees.push_back(&tree);
                }
            }
        } else {
            auto &&ret = locate_subtree_with_attribute(
                i.second, attribute_name, attribute_value
            );
            subtrees.insert(subtrees.end(), ret.begin(), ret.end());
        }
    }
    return subtrees;
}

static void throw_vector_size_unequal(
    const std::string vec1_name,
    const std::string vec2_name,
    std::size_t vec1_size,
    std::size_t vec2_size,
    const Job &job) {
    throw ProgramBug(
        vec1_name + " and " + vec2_name + " have unequal size.\n"
        + vec1_name + " has size " + std::to_string(vec1_size)
        + ", whlie " + vec2_name + " has size " + std::to_string(vec2_size)
        + ".\nInput file \"" + job.file_name + "\" at line "
        + std::to_string(job.start_line_number)
        + "-" + std::to_string(job.end_line_number)
    );
}

static std::string generate_vector_size_unexpected_message(
    const std::string &timestamp,
    const std::string &vec_name,
    std::size_t vec_size,
    std::size_t lower_limit,
    std::size_t upper_limit,
    const Job &job) {
    return
        "Warning (packet timestamp = " + timestamp + "): \n"
        + vec_name + " has unexpected size " + std::to_string(vec_size)
        + "\nExpected range: [" + std::to_string(lower_limit)
        + "," + std::to_string(upper_limit) + "] (inclusive)"
        + ".\nInput file \"" + job.file_name + "\" at line "
        + std::to_string(job.start_line_number)
        + "-" + std::to_string(job.end_line_number) + "\n";
}

/// This function extracts several kinds of information from RRC_OTA
/// packets. Currently 5 kinds of information are extracted.
/// 1. adding mapping between measurement event types to report config IDs
/// 2. removing mapping between measurement event types to report config IDs
/// 3. adding mapping between report config IDs to measurement IDs
/// 4. removing mapping betwee report config IDs to measurement IDs
/// 5. sending measurement report with triggering measurement ID
static void extract_rrc_ota_packet(pt::ptree &&tree, Job &&job) {
    // Warning message to be printed to stderr.
    std::string warning_message;

    // Extract the timestamp.
    std::string timestamp = "[unknown time]";
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get<std::string>("<xmlattr>.key") == "timestamp") {
                timestamp = i.second.data();
                break;
            }
        }
    }

    // Extract new mapping between measurement event types to report config IDs.
    // <field name="lte-rrc.ReportConfigToAddMod_element" ... >
    //     ...
    //         ... <field name="lte-rrc.reportConfigId"
    //                    showname="reportConfigId: X" .../>
    //         ... <field name="lte-rrc.eventId"
    //                    showname="eventId: eventAX (X)" ... >
    //                 ...
    //             </field>
    //     ...
    // </field>
    std::vector<std::string> added_config_ids, added_event_types;
    {
        auto &&report_config_nodes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.ReportConfigToAddMod_element"
        );
        std::vector<pt::ptree*> report_config_id_nodes, event_id_nodes;
        for (auto ptr : report_config_nodes) {
            auto &&ret = locate_subtree_with_attribute(
                *ptr, "name", "lte-rrc.reportConfigId"
            );
            if_likely (ret.size() == 1) {
                for (auto iter = ret.begin(); iter != ret.end(); ++iter) {
                    auto &&subret = locate_subtree_with_attribute(
                        *ptr, "name", "lte-rrc.eventId"
                    );
                    if_likely (subret.size() == 1) {
                        event_id_nodes.push_back(*iter);
                        report_config_id_nodes.push_back(subret[0]);
                    } else {
                        warning_message +=
                            generate_vector_size_unexpected_message(
                                timestamp,
                                "vector containing lte-rrc.eventId",
                                subret.size(),
                                1, 1, job
                            );
                    }
                }
            } else {
                warning_message +=
                    generate_vector_size_unexpected_message(
                        timestamp,
                        "vector containing lte-rrc.reportConfigId",
                        ret.size(),
                        1, 1, job
                    );
            }
        }

        if_unlikely (report_config_id_nodes.size() != event_id_nodes.size()) {
            throw_vector_size_unequal(
                "report_config_nodes", "event_id_nodes",
                report_config_id_nodes.size(),
                event_id_nodes.size(),
                job
            );
        }
        for (auto i = 0; i < report_config_id_nodes.size(); ++i) {
            added_event_types.push_back(
                event_id_nodes[i]->get<std::string>("<xmlattr>.showname")
            );
            added_config_ids.push_back(
                report_config_id_nodes[i]
                    ->get<std::string>("<xmlattr>.showname")
            );
        }
    }

    // Extract removal of mapping between measurement event types to
    // report config IDs.
    // <field name="lte-rrc.reportConfigToRemoveList" ... >
    //     ...
    //         ... <field name="lte-rrc.ReportConfigId"
    //                    showname="ReportConfigId: X" ... />
    //         ...
    //     ...
    // </field>
    std::vector<std::string> removed_config_ids;
    {
        auto &&report_config_remove_nodes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.reportConfigToRemoveList"
        );
        std::vector<pt::ptree*> remove_config_id_nodes;
        for (auto ptr : report_config_remove_nodes) {
            auto &&ret = locate_subtree_with_attribute(
                *ptr, "name", "lte-rrc.ReportConfigId"
            );
            remove_config_id_nodes.insert(
                remove_config_id_nodes.end(), ret.begin(), ret.end()
            );
        }
        for (auto i = 0; i < remove_config_id_nodes.size(); ++i) {
            removed_config_ids.push_back(
                remove_config_id_nodes[i]
                    ->get<std::string>("<xmlattr>.showname")
            );
        }
    }

    // Extract new mapping between report config IDs to measurement IDs.
    // <field name="lte-rrc.MeasIdToAddMod_element" ... >
    //     ...
    //         ... <field name="lte-rrc.reportConfigId"
    //                    showname="reportConfigId: X" .../>
    //         ... <field name="lte-rrc.measId"
    //                    showname="measId: 5" .../>
    //     ...
    // </field>
    std::vector<std::string> added_measure_ids, report_to_measure_ids;
    {
        auto &&measure_id_to_add_nodes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.MeasIdToAddMod_element"
        );
        std::vector<pt::ptree*> added_measure_id_nodes, measure_id_nodes;
        for (auto ptr : measure_id_to_add_nodes) {
            auto &&ret = locate_subtree_with_attribute(
                *ptr, "name", "lte-rrc.reportConfigId"
            );
            if_likely (ret.size() == 1) {
                for (auto iter = ret.begin(); iter != ret.end(); ++iter) {
                    auto &&subret = locate_subtree_with_attribute(
                        *ptr, "name", "lte-rrc.measId"
                    );
                    if_likely (subret.size() == 1) {
                        added_measure_id_nodes.push_back(*iter);
                        measure_id_nodes.push_back(subret[0]);
                    } else {
                        warning_message +=
                            generate_vector_size_unexpected_message(
                                timestamp,
                                "vector containing lte-rrc.measId",
                                subret.size(),
                                1, 1, job
                            );
                    }
                }
            } else {
                warning_message +=
                    generate_vector_size_unexpected_message(
                        timestamp,
                        "vector containing lte-rrc.reportConfigId",
                        ret.size(),
                        1, 1, job
                    );
            }
        }

        if_unlikely (added_measure_id_nodes.size() != measure_id_nodes.size()) {
            throw_vector_size_unequal(
                "added_measure_id_nodes", "measure_id_nodes",
                added_measure_id_nodes.size(),
                measure_id_nodes.size(),
                job
            );
        }
        for (auto i = 0; i < added_measure_id_nodes.size(); ++i) {
            added_measure_ids.push_back(
                measure_id_nodes[i]->get<std::string>("<xmlattr>.showname")
            );
            report_to_measure_ids.push_back(
                added_measure_id_nodes[i]
                    ->get<std::string>("<xmlattr>.showname")
            );
        }
    }

    // Extract removal of mapping betwee report config IDs to measurement IDs.
    // <field name="lte-rrc.measIdToRemoveList" ... >
    //     ...
    //         ... <field name="lte-rrc.MeasId"
    //                    showname="MeasId: X" ... />
    //         ...
    //     ...
    // </field>
    std::vector<std::string> removed_measure_ids;
    {
        auto &&measure_id_to_remove_nodes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.measIdToRemoveList"
        );
        std::vector<pt::ptree*> removed_measure_id_nodes;
        for (auto ptr : measure_id_to_remove_nodes) {
            auto &&ret = locate_subtree_with_attribute(
                *ptr, "name", "lte-rrc.MeasId"
            );
            removed_measure_id_nodes.insert(
                removed_measure_id_nodes.end(), ret.begin(), ret.end()
            );
        }
        for (auto i = 0; i < removed_measure_id_nodes.size(); ++i) {
            removed_measure_ids.push_back(
                removed_measure_id_nodes[i]
                    ->get<std::string>("<xmlattr>.showname")
            );
        }
    }

    // Extract the triggering measurement ID of the measurement report.
    // <field name="lte-rrc.measResults_element" ... >
    //     <field name="lte-rrc.measId"
    //            showname="measId: X" ... />
    //     ...
    // </field>
    std::vector<std::string> measurement_reports;
    {
        auto &&measurement_result_nodes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.measResults_element"
        );
        std::vector<pt::ptree*> measurement_id_nodes;
        for (auto ptr : measurement_result_nodes) {
            auto &&ret = locate_subtree_with_attribute(
                *ptr, "name", "lte-rrc.measId"
            );
            measurement_id_nodes.insert(
                measurement_id_nodes.end(), ret.begin(), ret.end()
            );
        }
        for (auto ptr : measurement_id_nodes) {
            measurement_reports.push_back(
                ptr->get<std::string>("<xmlattr>.showname")
            );
        }
    }

    // Send the closure to ordered task executor to print extracted
    // information out.
    insert_ordered_task(
        job.job_num,
        [timestamp = std::move(timestamp),
         added_config_ids = std::move(added_config_ids),
         added_event_types = std::move(added_event_types),
         removed_config_ids = std::move(removed_config_ids),
         added_measure_ids = std::move(added_measure_ids),
         report_to_measure_ids = std::move(report_to_measure_ids),
         removed_measure_ids = std::move(removed_measure_ids),
         measurement_reports = std::move(measurement_reports),
         warning_message = std::move(warning_message)] {
            std::cerr << warning_message;
            for (auto &i : removed_config_ids) {
                (*g_output) << "[" << timestamp
                << "] [reportConfigToRemoveList] $ " << i << std::endl;
            }
            for (auto &i : removed_measure_ids) {
                (*g_output) << "[" << timestamp
                << "] [measIdToRemoveList] $ " << i << std::endl;
            }
            for (auto i = 0; i < added_config_ids.size(); ++i) {
                (*g_output) << "[" << timestamp
                << "] [ReportConfigToAddMod] $ " << added_config_ids[i]
                << ", " << added_event_types[i] << std::endl;
            }
            for (auto i = 0; i < added_measure_ids.size(); ++i) {
                (*g_output) << "[" << timestamp
                << "] [MeasIdToAddMod] $ " << added_measure_ids[i] << ", "
                << report_to_measure_ids[i] << std::endl;
            }
            for (auto &i : measurement_reports) {
                (*g_output) << "[" << timestamp
                << "] [measResults] $ " << i << std::endl;
        }
    });
}

/// Return true if and only if the tree has the following structure:
/// <dm_log_packet>
///     ...
///     <pair key="type_id">LTE_RRC_Serv_Cell_Info</pair>
///     ...
/// <dm_log_packet>
static bool is_rrc_serv_cell_info_packet(
    const pt::ptree &tree, const Job &job) {
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get("<xmlattr>.key", std::string()) == "type_id"
                && i.second.data() == "LTE_RRC_Serv_Cell_Info") {
                return true;
            }
        }
    }
    return false;
}

/// Extract the following field from an LTE_RRC_Serv_Cell_Info packet.
/// <dm_log_packet>
///     <pair key="type_id">LTE_RRC_Serv_Cell_Info</pair>
///     <pair key="timestamp">XXX</pair>
///     <pair key="Cell ID">XXX</pair>
///     <pair key="Downlink frequency">XXX</pair>
///     <pair key="Uplink frequency">XXX</pair>
///     <pair key="Downlink bandwidth">XXX MHz</pair>
///     <pair key="Uplink bandwidth">XXX MHz</pair>
///     <pair key="Cell Identity">XXX</pair>
///     <pair key="TAC">XXX</pair>
///     ...
/// </dm_log_packet>
static void extract_rrc_serv_cell_info_packet(
    pt::ptree &&tree, Job &&job) {
    std::string timestamp = "[unknown time]";
    std::string cell_id, dl_freq, ul_freq;
    std::string dl_bandwidth, ul_bandwidth;
    std::string cell_identity, tracking_area_code;

    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first != "pair") {
            continue;
        }
        if (i.second.get("<xmlattr>.key", std::string()) == "timestamp") {
            timestamp = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string()) == "Cell ID") {
            cell_id = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string())
                   == "Downlink frequency") {
            dl_freq = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string())
                   == "Uplink frequency") {
            ul_freq = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string())
                   == "Downlink bandwidth") {
            dl_bandwidth = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string())
                   == "Uplink bandwidth") {
            ul_bandwidth = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string())
                   == "Cell Identity") {
            cell_identity = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string()) == "TAC") {
            tracking_area_code = i.second.data();
        }
    }

    std::string err_msg;
    if_unlikely (timestamp.empty() || cell_id.empty() || dl_freq.empty()
                 || ul_freq.empty() || dl_bandwidth.empty()
                 || ul_bandwidth.empty() || cell_identity.empty()
                 || tracking_area_code.empty()) {
        err_msg += "Warning (packet timestamp = " + timestamp + "): \n";
        err_msg += "The following field in the rrc_serv_cell_info_packet"
                   " is empty\n";
        if (timestamp.empty()) {
            err_msg += "timestamp, ";
        }
        if (cell_id.empty()) {
            err_msg += "Cell ID, ";
        }
        if (dl_freq.empty()) {
            err_msg += "Downlink frequency, ";
        }
        if (ul_freq.empty()) {
            err_msg += "Uplink frequency, ";
        }
        if (dl_bandwidth.empty()) {
            err_msg += "Downlink bandwidth, ";
        }
        if (ul_bandwidth.empty()) {
            err_msg += "Uplink bandwidth, ";
        }
        if (cell_identity.empty()) {
            err_msg += "Cell Identity, ";
        }
        if (tracking_area_code.empty()) {
            err_msg += "TAC, ";
        }
        err_msg += "\n";
        err_msg += "Input file " + job.file_name + " at line "
                   + std::to_string(job.start_line_number) + "-"
                   + std::to_string(job.end_line_number) + "\n";
    }

    insert_ordered_task(
        job.job_num,
        [timestamp = std::move(timestamp), cell_id = std::move(cell_id),
         dl_freq = std::move(dl_freq), ul_freq = std::move(ul_freq),
         dl_bandwidth = std::move(dl_bandwidth),
         ul_bandwidth = std::move(ul_bandwidth),
         cell_identity = std::move(cell_identity),
         tracking_area_code = std::move(tracking_area_code),
         err_msg = std::move(err_msg)] {
             std::cerr << err_msg;
             (*g_output) << "[" << timestamp
                         << "] [LTE_RRC_Serv_Cell_Info] $ "
                         << "Cell ID: " << cell_id << ", "
                         << "Downlink frequency: " << dl_freq << ", "
                         << "Uplink frequency: " << ul_freq << ", "
                         << "Downlink bandwidth: " << dl_bandwidth << ", "
                         << "Uplink bandwidth: " << ul_bandwidth << ", "
                         << "Cell Identity: " << cell_identity << ", "
                         << "TAC: " << tracking_area_code << std::endl;
        }
    );
}

/// Return true if and only if the tree has the following structure:
/// <dm_log_packet>
///     ...
///     <pair key="type_id">LTE_PDCP_UL_Cipher_Data_PDU</pair>
///     ...
/// <dm_log_packet>
/// or
/// <dm_log_packet>
///     ...
///     <pair key="type_id">LTE_PDCP_DL_Cipher_Data_PDU</pair>
///     ...
/// <dm_log_packet>
static bool is_pdcp_cipher_data_pdu_packet(
    const pt::ptree &tree, const Job &job) {
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get("<xmlattr>.key", std::string()) == "type_id"
                && (i.second.data() == "LTE_PDCP_UL_Cipher_Data_PDU"
                    || i.second.data() == "LTE_PDCP_DL_Cipher_Data_PDU")) {
                return true;
            }
        }
    }
    return false;
}

/// This function extracts PDU sizes from
/// LTE_PDCP_UL_Cipher_Data_PDU or LTE_PDCP_DL_Cipher_Data_PDU packets.
/// For downlink packets, it looks like below. (XXX is the extracted field.)
/// <dm_log_packet>
///     <pair key="type_id">LTE_PDCP_DL_Cipher_Data_PDU</pair>
///         ...
///         <pair key="PDU Size">XXX</pair>
///         ...
/// </dm_log_packet>
static void extract_pdcp_cipher_data_pdu_packet(
    pt::ptree &&tree, Job &&job) {
    std::string timestamp = "[unknown time]";
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get<std::string>("<xmlattr>.key") == "timestamp") {
                timestamp = i.second.data();
                break;
            }
        }
    }

    // Extract uplink PDCP PDU size.
    std::vector<std::string> ul_pdu_sizes;
    {
        auto &&pdu_packet_list = locate_subtree_with_attribute(
            tree, "key", "PDCPUL CIPH DATA"
        );
        for (auto pdu_packets : pdu_packet_list) {
            auto &&sizes = locate_subtree_with_attribute(
                *pdu_packets, "key", "PDU Size"
            );
            for (auto size : sizes) {
                ul_pdu_sizes.emplace_back(size->data());
            }
        }
    }

    // Extract downlink PDCP PDU size.
    std::vector<std::string> dl_pdu_sizes;
    {
        auto &&pdu_packet_list = locate_subtree_with_attribute(
            tree, "key", "PDCPUL CIPH DATA"
        );
        for (auto pdu_packets : pdu_packet_list) {
            auto &&sizes = locate_subtree_with_attribute(
                *pdu_packets, "key", "PDU Size"
            );
            for (auto size : sizes) {
                dl_pdu_sizes.emplace_back(size->data());
            }
        }
    }

    insert_ordered_task(
        job.job_num,
        [timestamp = std::move(timestamp),
         ul_pdu_sizes = std::move(ul_pdu_sizes),
         dl_pdu_sizes = std::move(dl_pdu_sizes)] {
             for (const auto &i : ul_pdu_sizes) {
                    (*g_output) << "[" << timestamp
                         << "] [LTE_PDCP_UL_Cipher_Data_PDU] $ "
                         << "PDU Size: " << i << std::endl;
             }
             for (const auto &i : dl_pdu_sizes) {
                    (*g_output) << "[" << timestamp
                         << "] [LTE_PDCP_DL_Cipher_Data_PDU] $ "
                         << "PDU Size: " << i << std::endl;
             }
        }
    );
}
