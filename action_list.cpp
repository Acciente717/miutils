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
#include <ctime>
#include <string>
#include <iostream>
#include <unordered_map>

enum class ExtractorEnum {
    RRC_OTA,
    RRC_SERV_CELL_INFO,
    PDCP_CIPHER_DATA_PDU,
    NAS_EMM_OTA_INCOMING,
    NAS_EMM_OTA_OUTGOING,
    MAC_RACH_ATTEMPT,
    MAC_RACH_TRIGGER,
    PHY_PDSCH_STAT,
    PHY_PDSCH,
    PHY_SERV_CELL_MEAS,
    RLC_DL_AM_ALL_PDU,
    RLC_UL_AM_ALL_PDU,
    ALL_PACKET_TYPE,
    ACTION_PDCP_CIPHER_DATA_PDU,
    NOP
};

/// Map the extractor name string to the corresponding enum.
static const std::unordered_map<std::string,
                                ExtractorEnum>
    extractor_name_to_enum = {
        {"rrc_ota", ExtractorEnum::RRC_OTA},
        {"rrc_serv_cell_info", ExtractorEnum::RRC_SERV_CELL_INFO},
        {"pdcp_cipher_data_pdu", ExtractorEnum::PDCP_CIPHER_DATA_PDU},
        {"nas_emm_ota_incoming", ExtractorEnum::NAS_EMM_OTA_INCOMING},
        {"nas_emm_ota_outgoing", ExtractorEnum::NAS_EMM_OTA_OUTGOING},
        {"mac_rach_attempt", ExtractorEnum::MAC_RACH_ATTEMPT},
        {"mac_rach_trigger", ExtractorEnum::MAC_RACH_TRIGGER},
        {"phy_pdsch_stat", ExtractorEnum::PHY_PDSCH_STAT},
        {"phy_pdsch", ExtractorEnum::PHY_PDSCH},
        {"phy_serv_cell_meas", ExtractorEnum::PHY_SERV_CELL_MEAS},
        {"rlc_dl_am_all_pdu", ExtractorEnum::RLC_DL_AM_ALL_PDU},
        {"rlc_ul_am_all_pdu", ExtractorEnum::RLC_UL_AM_ALL_PDU},
        {"all_packet_type", ExtractorEnum::ALL_PACKET_TYPE},
        {"action_pdcp_cipher_data_pdu",
         ExtractorEnum::ACTION_PDCP_CIPHER_DATA_PDU}
    };

static bool is_packet_having_type(
    const pt::ptree &tree, const std::string type_id);

static bool recursive_find_mobility_control_info(const pt::ptree &tree);
static void print_time_of_mobility_control_info(pt::ptree &&tree, Job &&job);
static void print_timestamp(pt::ptree &&tree, long seq_num);

static void extract_rrc_ota_packet(pt::ptree &&tree, Job &&job);
static void extract_rrc_serv_cell_info_packet(pt::ptree &&tree, Job &&job);
static void update_pdcp_cipher_data_pdu_packet_timestamp(
    pt::ptree &&tree, Job &&job);
static void extract_pdcp_cipher_data_pdu_packet(pt::ptree &&tree, Job &&job);
static void extract_lte_nas_emm_ota_incoming_packet(
    pt::ptree &&tree, Job &&job);
static void extract_lte_nas_emm_ota_outgoing_packet(
    pt::ptree &&tree, Job &&job);
static void extract_lte_mac_rach_attempt_packet(
    pt::ptree &&tree, Job &&job);
static void extract_lte_mac_rach_trigger_packet(
    pt::ptree &&tree, Job &&job);
static void extract_lte_phy_pdsch_stat_packet(
    pt::ptree &&tree, Job &&job);
static void extract_lte_phy_pdsch_packet(
    pt::ptree &&tree, Job &&job);
static void extract_lte_phy_serv_cell_measurement(
    pt::ptree &&tree, Job &&job);
static void echo_packet_within_time_range(
    pt::ptree &&tree, Job &&job);
static void extract_packet_type(
    pt::ptree &&tree, Job &&job);
static void extract_rlc_dl_am_all_pdu(
    pt::ptree &&tree, Job &&job);
static void extract_rlc_ul_am_all_pdu(
    pt::ptree &&tree, Job &&job);
static void echo_packet_if_new(
    pt::ptree &&tree, Job &&job);
static void update_reorder_window(
    pt::ptree &&tree, Job &&job);


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
void initialize_action_list_with_extractors() {
    // Below is an example.
    // Predicate: find the "mobilityControlInfo is present" string recursively.
    // Action: print the timestamp of this packet.
    // g_action_list.push_back(
    //     {
    //         [](const pt::ptree &tree, const Job &job) {
    //             return recursive_find_mobility_control_info(tree);
    //         },
    //         print_time_of_mobility_control_info
    //     }
    // );

    // Enable the extractors given by the program option.
    for (auto i : g_enabled_extractors) {
        auto pextractor = extractor_name_to_enum.find(i);
        ExtractorEnum extractor;
        if (pextractor == extractor_name_to_enum.end()) {
            extractor = ExtractorEnum::NOP;
        } else {
            extractor = pextractor->second;
        }
        switch (extractor) {
        case ExtractorEnum::RRC_OTA:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_RRC_OTA_Packet"
                        );
                    },
                    extract_rrc_ota_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_RRC_OTA_Packet" << std::endl;
            break;
        case ExtractorEnum::RRC_SERV_CELL_INFO:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_RRC_Serv_Cell_Info"
                        );
                    },
                    extract_rrc_serv_cell_info_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_RRC_Serv_Cell_Info" << std::endl;
            break;
        case ExtractorEnum::PDCP_CIPHER_DATA_PDU:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return
                            is_packet_having_type(
                                tree, "LTE_PDCP_UL_Cipher_Data_PDU"
                            )
                            ||
                            is_packet_having_type(
                                tree, "LTE_PDCP_DL_Cipher_Data_PDU"
                            );
                    },
                    extract_pdcp_cipher_data_pdu_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_PDCP_UL_Cipher_Data_PDU "
                      << "and LTE_PDCP_DL_Cipher_Data_PDU"
                      << std::endl;
            break;
        case ExtractorEnum::ACTION_PDCP_CIPHER_DATA_PDU:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return
                            is_packet_having_type(
                                tree, "LTE_PDCP_UL_Cipher_Data_PDU"
                            )
                            ||
                            is_packet_having_type(
                                tree, "LTE_PDCP_DL_Cipher_Data_PDU"
                            );
                    },
                    update_pdcp_cipher_data_pdu_packet_timestamp
                }
            );
            std::cerr << "Compound extractor enabled: "
                      << "act on LTE_PDCP_UL_Cipher_Data_PDU "
                      << "and LTE_PDCP_DL_Cipher_Data_PDU"
                      << std::endl;
            break;
        case ExtractorEnum::NAS_EMM_OTA_INCOMING:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_NAS_EMM_OTA_Incoming_Packet"
                        );
                    },
                    extract_lte_nas_emm_ota_incoming_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_NAS_EMM_OTA_Incoming_Packet" << std::endl;
            break;
        case ExtractorEnum::NAS_EMM_OTA_OUTGOING:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_NAS_EMM_OTA_Outgoing_Packet"
                        );
                    },
                    extract_lte_nas_emm_ota_outgoing_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_NAS_EMM_OTA_Outgoing_Packet" << std::endl;
            break;
        case ExtractorEnum::MAC_RACH_ATTEMPT:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_MAC_Rach_Attempt"
                        );
                    },
                    extract_lte_mac_rach_attempt_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_MAC_Rach_Attempt" << std::endl;
            break;
        case ExtractorEnum::MAC_RACH_TRIGGER:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_MAC_Rach_Trigger"
                        );
                    },
                    extract_lte_mac_rach_trigger_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_MAC_Rach_Trigger" << std::endl;
            break;
        case ExtractorEnum::PHY_PDSCH_STAT:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_PHY_PDSCH_Stat_Indication"
                        );
                    },
                    extract_lte_phy_pdsch_stat_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_PHY_PDSCH_Stat_Indication" << std::endl;
            break;
        case ExtractorEnum::PHY_PDSCH:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_PHY_PDSCH_Packet"
                        );
                    },
                    extract_lte_phy_pdsch_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_PHY_PDSCH_Packet" << std::endl;
            break;
        case ExtractorEnum::PHY_SERV_CELL_MEAS:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_PHY_Serv_Cell_Measurement"
                        );
                    },
                    extract_lte_phy_serv_cell_measurement
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_PHY_Serv_Cell_Measurement" << std::endl;
            break;
        case ExtractorEnum::RLC_DL_AM_ALL_PDU:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_RLC_DL_AM_All_PDU"
                        );
                    },
                    extract_rlc_dl_am_all_pdu
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_RLC_DL_AM_All_PDU" << std::endl;
            break;
        case ExtractorEnum::RLC_UL_AM_ALL_PDU:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_RLC_UL_AM_All_PDU"
                        );
                    },
                    extract_rlc_ul_am_all_pdu
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_RLC_UL_AM_All_PDU" << std::endl;
            break;
        case ExtractorEnum::ALL_PACKET_TYPE:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return true;
                    },
                    extract_packet_type
                }
            );
            std::cerr << "Extractor enabled: "
                      << "ALL_PACKET_TYPE" << std::endl;
            break;
        case ExtractorEnum::NOP:
            std::cerr << "Warning: encountered unknown extractor "
                      << "(" << i << ")" << std::endl;
            break;
        default:
            throw ProgramBug(
                "Switch case list in initialize_action_list() "
                "is not exhaustive. Ran into default branch."
            );
        }
    }

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

/// Initialize the `g_action_list` to do the filter work. Due to the same
/// reason as `initialize_action_list_with_extractors()`, we must put
/// a dummy function at the end of the list.
void initialize_action_list_with_filter() {
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, const Job &job) { return true; },
            echo_packet_within_time_range
        }
    );
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

/// Initialize the `g_action_list` to do the deduplicate work. Due to the
/// same reason as `initialize_action_list_with_extractors()`, we must put
/// a dummy function at the end of the list.
void initialize_action_list_to_dedup() {
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, const Job &job) { return true; },
            echo_packet_if_new
        }
    );
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

/// Initialize the `g_action_list` to do the reorder work .Due to the
/// same reason as `initialize_action_list_with_extractors()`, we must put
/// a dummy function at the end of the list.
void initialize_action_list_to_reorder() {
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, const Job &job) { return true; },
            update_reorder_window
        }
    );
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

/// Return true if and only if the tree has the following structure:
/// <dm_log_packet>
///     ...
///     <pair key="type_id">$type_id</pair>
///     ...
/// <dm_log_packet>
static bool is_packet_having_type(
    const pt::ptree &tree, const std::string type_id) {
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get("<xmlattr>.key", std::string()) == "type_id"
                && i.second.data() == type_id) {
                return true;
            }
        }
    }
    return false;
}

/// Find and return the timestamp locating at
/// <dm_log_packet>
///     ...
///     <pair key="timestamp"> TIMESTAMP </pair>
///     ...
/// </dm_log_packet>
static std::string get_packet_time_stamp(const pt::ptree &tree) {
    std::string timestamp = "timestamp N/A";
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get<std::string>("<xmlattr>.key") == "timestamp") {
                timestamp = i.second.data();
                break;
            }
        }
    }
    return timestamp;
}

/// Convert the timestamp string to a long integer under
/// timezone UTC+8.
static time_t timestamp_str2long(const std::string &timestamp) {
    tm s;
    memset(&s, 0, sizeof(s));
    auto cnt = sscanf(timestamp.c_str(), "%d-%d-%d %d:%d:%d.%*d",
                      &s.tm_year, &s.tm_mon, &s.tm_mday,
                      &s.tm_hour, &s.tm_min, &s.tm_sec);
    if (cnt != 6) {
        return static_cast<time_t>(-1);
    }
    s.tm_year -= 1900;
    s.tm_mon -= 1;
    return mktime(&s) + 28800;
}

/// Convert the timestamp string to a long integer under
/// timezone UTC+8. The returned value contains the microsecond part.
static time_t timestamp_str2long_microsec_hack(const std::string &timestamp) {
    tm s;
    int mircosec;
    memset(&s, 0, sizeof(s));
    auto cnt = sscanf(timestamp.c_str(), "%d-%d-%d %d:%d:%d.%d",
                      &s.tm_year, &s.tm_mon, &s.tm_mday,
                      &s.tm_hour, &s.tm_min, &s.tm_sec, &mircosec);
    if (cnt == 6) {
        cnt = sscanf(timestamp.c_str(), "%d-%d-%d %d:%d:%d",
                     &s.tm_year, &s.tm_mon, &s.tm_mday,
                     &s.tm_hour, &s.tm_min, &s.tm_sec);
        mircosec = 0;
    } else if (cnt != 7) {
        return static_cast<time_t>(-1);
    }
    s.tm_year -= 1900;
    s.tm_mon -= 1;
    return ((mktime(&s) + 28800) * 1000000) + mircosec;
}

static bool is_tree_having_attribute(
    const pt::ptree &tree, const std::string &key, const std::string &val) {
    auto &&attributes = tree.get_child_optional("<xmlattr>");

    // If the root XML tree has no attribute.
    if (!attributes) {
        return false;
    }

    // Scan through the attributes to see if there is any key:val pair.
    for (auto attribute : *attributes) {
        if (attribute.first == key
            && attribute.second.get_value(std::string()) == val) {
            return true;
        }
    }

    return false;
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
                                << "] [mobilityControlInfo] $ "
                                << "LastPDCPPacketTimestamp: "
                                << g_last_pdcp_packet_timestamp
                                << std::endl;
                });
                break;
            }
        }
    }
}

/// Start from the root `tree`, recursively find the following subtrees:
/// <some_tag attribute_name=attribute_value ... >
///     ...
/// </some_tag>
/// Return the pointers to the roots of the subtrees, i.e. all the subtrees
/// starting at `some_tag` with the attribute name `attribute_name` and
/// attribute value `attribute_value`.
///
/// This function *does not* guarantee that all returned trees are disjoint,
/// i.e. a returned node might be the decendent of another returned node.
static std::vector<const pt::ptree*> locate_subtree_with_attribute(
    const pt::ptree &tree,
    const std::string &attribute_name,
    const std::string &attribute_value
) {
    std::vector<const pt::ptree*> subtrees;
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

/// Start from the root `tree`, recursively find the following subtrees:
/// <some_tag attribute_name=attribute_value ... >
///     ...
/// </some_tag>
/// Return the pointers to the roots of the subtrees, i.e. all the subtrees
/// starting at `some_tag` with the attribute name `attribute_name` and
/// attribute value `attribute_value`.
///
/// This function guarantees that all returned trees are disjoint, i.e.
/// no node is the decendent of any other returned nodes.
static std::vector<const pt::ptree*> locate_disjoint_subtree_with_attribute(
    const pt::ptree &tree,
    const std::string &attribute_name,
    const std::string &attribute_value
) {
    std::vector<const pt::ptree*> subtrees;
    bool hit = false;
    for (auto &i : tree) {
        if (i.first == "<xmlattr>") {
            for (auto &j : i.second) {
                if (j.first == attribute_name
                    && j.second.data() == attribute_value) {
                    subtrees.push_back(&tree);
                    hit = true;
                    break;
                }
            }
            if (hit) break;
        }
    }
    if (!hit) {
        for (auto &i : tree) {
            auto &&ret = locate_disjoint_subtree_with_attribute(
                i.second, attribute_name, attribute_value);
            subtrees.insert(subtrees.end(), ret.begin(), ret.end());
        }
    }
    return subtrees;
}

/// Start from the root `tree`, recursively find the following subtrees:
/// <some_tag attribute_name=attribute_value ... >
///     ...
/// </some_tag>
/// Return true if at least one such subtree exists, otherwise false.
static bool is_subtree_with_attribute_present(
    const pt::ptree &tree,
    const std::string &attribute_name,
    const std::string &attribute_value
) {
    for (auto &i : tree) {
        if (i.first == "<xmlattr>") {
            for (auto &j : i.second) {
                if (j.first == attribute_name
                    && j.second.data() == attribute_value) {
                    return true;
                }
            }
        } else {
            if (is_subtree_with_attribute_present(
                i.second, attribute_name, attribute_value
            )) {
                return true;
            }
        }
    }
    return false;
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
/// packets. Currently 14 kinds of information are extracted.
/// 1. adding mapping between measurement event types to report config IDs
/// 2. removing mapping between measurement event types to report config IDs
/// 3. adding mapping between report config IDs to measurement IDs
/// 4. removing mapping betwee report config IDs to measurement IDs
/// 5. sending measurement report with triggering measurement ID
/// 6. sending RRC connection reestablishment request
/// 7. receiving RRC connection reestablishment complete
/// 8. receiving RRC connection reestablishment reject
/// 9. sending RRC connection reconfiguration
/// 10. sending RRC connection reconfiguration complete
/// 11. sending RRC connection release
/// 12. sending RRC connection request
/// 13. receiving RRC connection setup
/// 14. receiving RRC connection reject
static void extract_rrc_ota_packet(pt::ptree &&tree, Job &&job) {
    // Warning message to be printed to stderr.
    std::string warning_message;

    // Extract the timestamp.
    std::string timestamp = get_packet_time_stamp(tree);

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
        std::vector<const pt::ptree*> report_config_id_nodes, event_id_nodes;
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
        std::vector<const pt::ptree*> remove_config_id_nodes;
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
        std::vector<const pt::ptree*> added_measure_id_nodes, measure_id_nodes;
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
        std::vector<const pt::ptree*> removed_measure_id_nodes;
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
        std::vector<const pt::ptree*> measurement_id_nodes;
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

    bool rrc_connection_reestablishment_request_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionReestablishmentRequest"
    );

    bool rrc_connection_reestablishment_complete_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionReestablishmentComplete"
    );
    std::string connection_reestablishment_cause;
    {
        auto &&causes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.reestablishmentCause"
        );
        for (auto p_cause : causes) {
            if (!connection_reestablishment_cause.empty()) {
                connection_reestablishment_cause += ", ";
            }
            connection_reestablishment_cause +=
                p_cause->get("<xmlattr>.showname", std::string());
        }
    }

    bool rrc_connection_reestablishment_reject_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionReestablishmentReject"
    );

    bool rrc_connection_reconfiguration_present = false;
    bool mobility_control_info_present = false;
    std::string target_cells;
    {
        auto &&reconf_nodes = locate_subtree_with_attribute(
            tree, "showname", "rrcConnectionReconfiguration"
        );
        if (!reconf_nodes.empty()) {
            rrc_connection_reconfiguration_present = true;
            for (auto p_reconf_node : reconf_nodes) {
                if (is_subtree_with_attribute_present(
                    *p_reconf_node, "showname", "mobilityControlInfo"
                )) {
                    mobility_control_info_present = true;
                    break;
                }
            }
        }
    }
    if (mobility_control_info_present) {
        auto &&target_physic_cells = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.targetPhysCellId"
        );
        for (auto ptr : target_physic_cells) {
            if (!target_cells.empty()) {
                target_cells += ", ";
            }
            target_cells += ptr->get<std::string>("<xmlattr>.showname");
        }
    }

    bool rrc_connection_reconfiguration_complete_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionReconfigurationComplete"
    );

    bool rrc_connection_release_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionRelease"
    );

    bool rrc_connection_request_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionRequest"
    );

    bool rrc_connection_setup_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionSetup"
    );

    bool rrc_connection_reject_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionReject"
    );

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
         warning_message = std::move(warning_message),
         rrc_connection_reestablishment_request_present,
         rrc_connection_reestablishment_complete_present,
         rrc_connection_reestablishment_reject_present,
         rrc_connection_reconfiguration_present,
         mobility_control_info_present,
         target_cells = std::move(target_cells),
         connection_reestablishment_cause
            = std::move(connection_reestablishment_cause),
         rrc_connection_reconfiguration_complete_present,
         rrc_connection_release_present,
         rrc_connection_request_present,
         rrc_connection_setup_present,
         rrc_connection_reject_present] {
            auto print_last_data_pdcp_packet_timestamp = [] {
            (*g_output) << "LastPDCPPacketTimestamp: "
                        << g_last_pdcp_packet_timestamp
                        << ", Direction: ";
                if (g_last_pdcp_packet_direction == PDCPDirection::Downlink) {
                    (*g_output) << "downlink";
                } else if (g_last_pdcp_packet_direction
                            == PDCPDirection::Uplink) {
                    (*g_output) << "uplink";
                } else {
                    (*g_output) << "unknown";
                }
            };

            auto set_connection_disruption =
                [] (DisruptionEventEnum event_type) {
                g_distuption_events.is_being_disrupted = true;
                g_distuption_events.disruptions[
                    static_cast<int>(event_type)
                ] = true;
            };

            std::cerr << warning_message;
            for (auto &i : removed_config_ids) {
                (*g_output) << timestamp << " $ reportConfigToRemoveList $ "
                            << i << std::endl;
            }
            for (auto &i : removed_measure_ids) {
                (*g_output) << timestamp << " $ measIdToRemoveList $ "
                            << i << std::endl;
            }
            for (auto i = 0; i < added_config_ids.size(); ++i) {
                (*g_output) << timestamp << " $ ReportConfigToAddMod $ "
                            << added_config_ids[i]
                            << ", " << added_event_types[i] << std::endl;
            }
            for (auto i = 0; i < added_measure_ids.size(); ++i) {
                (*g_output) << timestamp << " $ MeasIdToAddMod $ "
                            << added_measure_ids[i] << ", "
                            << report_to_measure_ids[i] << std::endl;
            }
            for (auto &i : measurement_reports) {
                (*g_output) << timestamp << " $ measResults $ "
                            << i << std::endl;
            }
            if (rrc_connection_reestablishment_request_present) {
                (*g_output) << timestamp
                            << " $ rrcConnectionReestablishmentRequest $ ";
                print_last_data_pdcp_packet_timestamp();
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionReestablishmentRequest
                );;
                if (!connection_reestablishment_cause.empty()) {
                    (*g_output) << ", " << connection_reestablishment_cause;
                }
                (*g_output) << std::endl;
            }
            if (rrc_connection_reestablishment_complete_present) {
                (*g_output) << timestamp
                            << " $ rrcConnectionReestablishmentComplete $"
                            << std::endl;
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionReestablishmentComplete
                );
            }
            if (rrc_connection_reestablishment_reject_present) {
                (*g_output) << timestamp
                            << " $ rrcConnectionReestablishmentReject $"
                            << std::endl;
            }
            if (rrc_connection_reconfiguration_present) {
                (*g_output) << timestamp << " $ rrcConnectionReconfiguration $"
                            << " mobilityControlInfo: ";
                if (mobility_control_info_present) {
                    (*g_output) << "1, "
                                << target_cells;
                } else {
                    (*g_output) << '0';
                }
                (*g_output) << ", ";
                print_last_data_pdcp_packet_timestamp();
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionReconfiguration
                );
                (*g_output) << std::endl;
            }
            if (rrc_connection_reconfiguration_complete_present) {
                (*g_output) << timestamp
                            << " $ rrcConnectionReconfigurationComplete $"
                            << std::endl;
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionReconfigurationComplete
                );
            }
            if (rrc_connection_release_present) {
                (*g_output) << timestamp << " $ rrcConnectionRelease $"
                            << std::endl;
            }
            if (rrc_connection_request_present) {
                (*g_output) << timestamp << " $ rrcConnectionRequest $ ";
                print_last_data_pdcp_packet_timestamp();
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionRequest
                );
                (*g_output) << std::endl;
            }
            if (rrc_connection_setup_present) {
                (*g_output) << timestamp << " $ rrcConnectionSetup $"
                            << std::endl;
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionSetup
                );
            }
            if (rrc_connection_reject_present) {
                (*g_output) << timestamp << " $ rrcConnectionReject $"
                            << std::endl;
            }
    });
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
    std::string timestamp = "timestamp N/A";
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
             (*g_output) << timestamp << " $ LTE_RRC_Serv_Cell_Info $ "
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

/// This function extracts and update the global string containing the
/// timestamp of the last LTE_PDCP_UL_Cipher_Data_PDU or
/// LTE_PDCP_DL_Cipher_Data_PDU packet.
/// Note that this function itself does NOT check whether the packet is
/// one of the two above. It MUST be used together with the predicate
/// function.
/// Note that the update is done by the in-order executor.
static void update_pdcp_cipher_data_pdu_packet_timestamp(
    pt::ptree &&tree, Job &&job) {
    std::string timestamp = get_packet_time_stamp(tree);

    // Get the direction of the PDCP packets, should be uplink or downlink.
    PDCPDirection direction = PDCPDirection::Unknown;
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get("<xmlattr>.key", std::string()) == "type_id") {
                auto &&packet_type = i.second.data();
                if (packet_type == "LTE_PDCP_UL_Cipher_Data_PDU") {
                    direction = PDCPDirection::Uplink;
                } else if (packet_type == "LTE_PDCP_DL_Cipher_Data_PDU") {
                    direction = PDCPDirection::Downlink;
                }
                break;
            }
        }
    }

    auto print_timestamp_of_first_pdcp_packet_after_disruption =
        [timestamp, direction] {
        if (g_distuption_events.is_being_disrupted) {
            for (int i = 0;
                i < static_cast<int>(DisruptionEventEnum::NumberOfDisruptions);
                ++i) {
                if (g_distuption_events.disruptions[i]) {
                    (*g_output) << timestamp
                                << " $ FirstPDCPPacketAfterDisruption $ "
                                << "Disruption Type: "
                                << DisruptionEventNames[i]
                                << ", Direction: ";
                    switch (direction) {
                    case PDCPDirection::Unknown:
                        (*g_output) << "unknown";
                        break;
                    case PDCPDirection::Uplink:
                        (*g_output) << "uplink";
                        break;
                    case PDCPDirection::Downlink:
                        (*g_output) << "downlink";
                        break;
                    }
                    (*g_output) << std::endl;
                    g_distuption_events.disruptions[i] = false;
                }
            }
            g_distuption_events.is_being_disrupted = false;
        }
    };

    switch (direction) {
    case PDCPDirection::Unknown:
        throw ProgramBug(
            "Function `update_pdcp_cipher_data_pdu_packet_timestamp`"
            " was invoked with a packet of type neither"
            " LTE_PDCP_UL_Cipher_Data_PDU nor"
            " LTE_PDCP_DL_Cipher_Data_PDU."
        );
    case PDCPDirection::Uplink:
        // Search if there is any uplink pdcp data packets.
        // Note that we only treat packets with size=1412 as data packets,
        // since upper TCP connection is sending at full speed.
        {
            bool uplink_pdcp_data_packet_present = false;
            auto &&pdu_packet_list = locate_subtree_with_attribute(
                tree, "key", "PDCPUL CIPH DATA"
            );
            for (auto pdu_packets : pdu_packet_list) {
                auto &&sizes = locate_subtree_with_attribute(
                    *pdu_packets, "key", "PDU Size"
                );
                for (auto size : sizes) {
                    if (size->data() == "1412") {
                        uplink_pdcp_data_packet_present = true;
                        break;
                    }
                }
                if (uplink_pdcp_data_packet_present) {
                    break;
                }
            }
            if (uplink_pdcp_data_packet_present) {
                insert_ordered_task(
                    job.job_num,
                    [timestamp = std::move(timestamp),
                     print_timestamp_of_first_pdcp_packet_after_disruption] {
                        print_timestamp_of_first_pdcp_packet_after_disruption();
                        g_last_pdcp_packet_timestamp = std::move(timestamp);
                        g_last_pdcp_packet_direction = PDCPDirection::Uplink;
                    }
                );
                return;
            }
        }
        break;
    case PDCPDirection::Downlink:
        // Search if there is any downlink pdcp data packets.
        // Note that we only treat packets with size=1412 as data packets,
        // since upper TCP connection is sending at full speed.
        {
            bool downlink_pdcp_data_packet_present = false;
            auto &&pdu_packet_list = locate_subtree_with_attribute(
                tree, "key", "PDCPDL CIPH DATA"
            );
            for (auto pdu_packets : pdu_packet_list) {
                auto &&sizes = locate_subtree_with_attribute(
                    *pdu_packets, "key", "PDU Size"
                );
                for (auto size : sizes) {
                    if (size->data() == "1412") {
                        downlink_pdcp_data_packet_present = true;
                        break;
                    }
                }
                if (downlink_pdcp_data_packet_present) {
                    break;
                }
            }
            if (downlink_pdcp_data_packet_present) {
                insert_ordered_task(
                    job.job_num,
                    [timestamp = std::move(timestamp),
                     print_timestamp_of_first_pdcp_packet_after_disruption] {
                        print_timestamp_of_first_pdcp_packet_after_disruption();
                        g_last_pdcp_packet_timestamp = std::move(timestamp);
                        g_last_pdcp_packet_direction = PDCPDirection::Downlink;
                    }
                );
                return;
            }
        }
        break;
    default:
        throw ProgramBug(
            "PDCPDirection should be one of the following: "
            "Unknown, Uplink, Downlink."
        );
    }

    insert_ordered_task(
        job.job_num,
        [] {}
    );
}

/// This function extracts and prints PDCP PDU sizes from
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
    std::string timestamp = get_packet_time_stamp(tree);

    std::string err_msg;

    auto extract_size_and_bearer_id = [&tree, &err_msg, &timestamp] (
        const std::string &packet_type,
        std::vector<std::string> &pdu_size_vec,
        std::vector<std::string> &bearer_id_vec) {
        auto &&pdu_packet_lists = locate_subtree_with_attribute(
            tree, "key", packet_type
        );
        for (auto pdu_packet_list : pdu_packet_lists) {
            auto &&pdu_packets = locate_subtree_with_attribute(
                *pdu_packet_list, "type", "dict"
            );
            std::string size, bearer_id;
            for (auto pdu_packet : pdu_packets) {
                for (auto packet_info : pdu_packet->get_child("dict")) {
                    if (is_tree_having_attribute(
                        packet_info.second, "key", "Bearer ID")) {
                        bearer_id
                            = packet_info.second.get_value(std::string());
                    } else if (is_tree_having_attribute(
                        packet_info.second, "key", "PDU Size")) {
                        size = packet_info.second.get_value(std::string());
                    }
                }
                if_unlikely (size.empty()) {
                    err_msg += "Warning (packet timestamp = " + timestamp
                            + "):\n" + "Found an " + packet_type
                            + " packet with size = 0."
                            + " Skipping...\n";
                    continue;
                }
                if_unlikely (bearer_id.empty()) {
                    err_msg += "Warning (packet timestamp = " + timestamp
                            + "):\n" + "Found an " + packet_type
                            + " packet with no bearer id."
                            + " Skipping...\n";
                    continue;
                }
                pdu_size_vec.emplace_back(std::move(size));
                bearer_id_vec.emplace_back(std::move(bearer_id));
            }
        }
    };

    // Extract uplink PDCP PDU size and bearer id.
    std::vector<std::string> ul_pdu_sizes, ul_bearer_id;
    extract_size_and_bearer_id("PDCPUL CIPH DATA", ul_pdu_sizes, ul_bearer_id);

    // Extract downlink PDCP PDU size and bearer id.
    std::vector<std::string> dl_pdu_sizes, dl_bearer_id;
    extract_size_and_bearer_id("PDCPDL CIPH DATA", dl_pdu_sizes, dl_bearer_id);

    insert_ordered_task(
        job.job_num,
        [timestamp = std::move(timestamp),
         err_msg = std::move(err_msg),
         ul_pdu_sizes = std::move(ul_pdu_sizes),
         ul_bearer_id = std::move(ul_bearer_id),
         dl_pdu_sizes = std::move(dl_pdu_sizes),
         dl_bearer_id = std::move(dl_bearer_id)] {
             std::cerr << err_msg;
             for (auto i = 0; i < ul_pdu_sizes.size(); ++i) {
                 (*g_output) << timestamp
                             << " $ LTE_PDCP_UL_Cipher_Data_PDU $ "
                             << "PDU Size: " << ul_pdu_sizes[i]
                             << ", Bearer ID: " << ul_bearer_id[i]
                             << std::endl;
             }
             for (auto i = 0; i < dl_pdu_sizes.size(); ++i) {
                 (*g_output) << timestamp
                             << " $ LTE_PDCP_DL_Cipher_Data_PDU $ "
                             << "PDU Size: " << dl_pdu_sizes[i]
                             << ", Bearer ID: " << dl_bearer_id[i]
                             << std::endl;
             }
        }
    );
}

/// This function extracts and prints tracking area update accept or reject
/// from LTE_NAS_EMM_OTA_Incoming_Packet packets. For update accept, it
/// looks for the pattern shown below.
/// <dm_log_packet>
///     ...
///         <field name="nas_eps.nas_msg_emm_type"
///             showname="NAS EPS Mobility Management Message Type:
///                       Tracking area update accept (0x49)" ... />
///     ...
/// </dm_log_packet>
static void extract_lte_nas_emm_ota_incoming_packet(
    pt::ptree &&tree, Job &&job) {
    std::string timestamp = get_packet_time_stamp(tree);

    bool tracking_area_update_accept = false;
    bool tracking_area_update_reject = false;
    auto &&nas_msg_emm_type_fields = locate_subtree_with_attribute(
        tree, "name", "nas_eps.nas_msg_emm_type"
    );
    for (auto ptr : nas_msg_emm_type_fields) {
        auto &&showname = ptr->get("<xmlattr>.showname", std::string());
        if (showname.find("Tracking area update accept") != std::string::npos) {
            tracking_area_update_accept = true;
            break;
        }
        if (showname.find("Tracking area update reject") != std::string::npos) {
            tracking_area_update_reject = true;
            break;
        }
    }

    // If tracking area update request is neither accecpted or rejected,
    // we have nothing to print. Simply do nothing.
    if (!tracking_area_update_accept && !tracking_area_update_reject) {
        insert_ordered_task(
            job.job_num, [] {}
        );
        return;
    }

    std::string message;
    message += timestamp + " $ LTE_NAS_EMM_OTA_Incoming_Packet $ "
               + "Tracking area update accept: ";
    if (tracking_area_update_accept) {
        message += "1";
    } else {
        message += "0";
    }
    message += ", Tracking area update reject: ";
    if (tracking_area_update_reject) {
        message += "1";
    } else {
        message += "0";
    }

    insert_ordered_task(
        job.job_num,
        [message = std::move(message)] {
            (*g_output) << message << std::endl;
        }
    );
}

/// This function extracts and prints tracking area update request
/// from LTE_NAS_EMM_OTA_Outgoing_Packet packets. It looks for the
/// pattern shown below.
/// <dm_log_packet>
///     ...
///         <field name="nas_eps.nas_msg_emm_type"
///             showname="NAS EPS Mobility Management Message Type:
///                       Tracking area update request (0x48)" ... />
///     ...
/// </dm_log_packet>
static void extract_lte_nas_emm_ota_outgoing_packet(
    pt::ptree &&tree, Job &&job) {
    std::string timestamp = get_packet_time_stamp(tree);

    bool tracking_area_update_request = false;
    auto &&nas_msg_emm_type_fields = locate_subtree_with_attribute(
        tree, "name", "nas_eps.nas_msg_emm_type"
    );
    for (auto ptr : nas_msg_emm_type_fields) {
        auto &&showname = ptr->get("<xmlattr>.showname", std::string());
        if (showname.find("Tracking area update request")
            != std::string::npos) {
            tracking_area_update_request = true;
            break;
        }
    }

    // If the packet does not contain tracking area update request,
    // we have nothing to print. Simply do nothing.
    if (!tracking_area_update_request) {
        insert_ordered_task(
            job.job_num, [] {}
        );
        return;
    }

    std::string message;
    message += timestamp + " $ LTE_NAS_EMM_OTA_Outgoing_Packet $ "
               + "Tracking area update request: ";
    if (tracking_area_update_request) {
        message += "1";
    } else {
        message += "0";
    }

    insert_ordered_task(
        job.job_num,
        [message = std::move(message)] {
            (*g_output) << message << std::endl;
        }
    );
}

/// This function extracts and prints random access results
/// from LTE_MAC_Rach_Attempt packets. It looks for the
/// pattern shown below.
/// <dm_log_packet>
///     ...
///         <pair key="Rach result"> XXX </pair>
///     ...
/// </dm_log_packet>
static void extract_lte_mac_rach_attempt_packet(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    std::string results;
    {
        auto &&rach_results = locate_subtree_with_attribute(
            tree, "key", "Rach result"
        );
        for (auto ptr : rach_results) {
            if (!results.empty()) {
                results += ", ";
            }
            results += "Result: " + ptr->data();
        }
    }

    insert_ordered_task(
        job.job_num,
        [timestamp = std::move(timestamp),
         results = std::move(results)] {
            (*g_output) << timestamp << " $ LTE_MAC_Rach_Attempt $ "
                        << results << std::endl;
        }
    );
}

/// This function extracts and prints triggering reason of ramdom access
/// from LTE_MAC_Rach_Trigger packets. It looks for the
/// pattern shown below.
/// <dm_log_packet>
///     ...
///         <pair key="Rach reason"> XXX </pair>
///     ...
/// </dm_log_packet>
static void extract_lte_mac_rach_trigger_packet(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    std::string reasons;
    {
        auto &&rach_results = locate_subtree_with_attribute(
            tree, "key", "Rach reason"
        );
        for (auto ptr : rach_results) {
            if (!reasons.empty()) {
                reasons += ", ";
            }
            reasons += "Reason: " + ptr->data();
        }
    }

    insert_ordered_task(
        job.job_num,
        [timestamp = std::move(timestamp),
         reasons = std::move(reasons)] {
            (*g_output) << timestamp << " $ LTE_MAC_Rach_Trigger $ "
                        << reasons
                        << ", LastPDCPPacketTimestamp: "
                        << g_last_pdcp_packet_timestamp
                        << std::endl;
        }
    );
}

static void extract_lte_phy_pdsch_stat_packet(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    auto extract_key_value = [](const pt::ptree &tree) {
        std::vector<std::string> trans_block_info_lst;
        auto &&trans_block_items = locate_disjoint_subtree_with_attribute(
            tree, "type", "dict"
        );
        for (const auto &trans_block_item : trans_block_items) {
            std::string single_result;
            for (const auto &pair : trans_block_item->get_child("dict")) {
                if (!single_result.empty()) {
                    single_result += ", ";
                }
                single_result += pair.second.get<std::string>("<xmlattr>.key");
                single_result += ": ";
                single_result += pair.second.get_value<std::string>();
            }
            trans_block_info_lst.emplace_back(std::move(single_result));
        }
        return trans_block_info_lst;
    };

    std::string final_result;
    auto &&record_lists = locate_disjoint_subtree_with_attribute(
        tree, "key", "Records"
    );
    for (const auto &record_list : record_lists) {
        auto &&records = locate_disjoint_subtree_with_attribute(
            *record_list, "type", "dict"
        );
        for (const auto &record : records) {
            std::string single_result;
            std::vector<std::string> trans_block_info_lst;
            for (const auto &item : record->get_child("dict")) {
                auto &&key = item.second.get<std::string>("<xmlattr>.key");
                if (key == "Transport Blocks") {
                    trans_block_info_lst = extract_key_value(item.second);
                } else {
                    if (!single_result.empty()) single_result += ", ";
                    single_result += key;
                    single_result += ": "
                                   + item.second.get_value<std::string>();
                }
            }
            for (const auto &trans_block_info : trans_block_info_lst) {
                final_result += timestamp;
                final_result += " $ LTE_PHY_PDSCH_Stat_Indication $ ";
                final_result += single_result;
                if (!single_result.empty()) final_result += ", ";
                final_result += trans_block_info;
                final_result += '\n';
            }
        }
    }

    insert_ordered_task(
        job.job_num,
        [final_result = std::move(final_result)] {
            (*g_output) << final_result;
        }
    );
}

static void extract_lte_phy_pdsch_packet(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    const char * const target_keys[] = {
        "System Frame Number",
        "Subframe Number",
        "Number of Tx Antennas(M)",
        "Number of Rx Antennas(N)",
        "TBS 0",
        "MCS 0",
        "TBS 1",
        "MCS 1"
    };

    std::string result;
    for (const auto &pair : tree.get_child("dm_log_packet")) {
        char const *match_key = nullptr;
        for (auto key : target_keys) {
            if (pair.second.get<std::string>("<xmlattr>.key") == key) {
                match_key = key;
                break;
            }
        }
        if (match_key == nullptr) continue;
        if (!result.empty()) {
            result += ", ";
        }
        result += match_key;
        result += ": ",
        result += pair.second.get_value<std::string>();
    }

    insert_ordered_task(
        job.job_num,
        [timestamp = std::move(timestamp),
         result = std::move(result)] {
            (*g_output) << timestamp
                        << " $ LTE_PHY_PDSCH_Packet $ "
                        << result << std::endl;
        }
    );
}

static void extract_lte_phy_serv_cell_measurement(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    std::string result;
    auto &&subpacket_lists = locate_subtree_with_attribute(
        tree, "key", "Subpackets"
    );
    for (const auto subpacket_list : subpacket_lists) {
        for (const auto &subpacket : subpacket_list->get_child("list")) {
            enum class Status {unknown, primary, non_primary};
            Status status = Status::unknown;
            std::string rsrp;
            for (const auto pair : subpacket.second.get_child("dict")) {
                if (pair.second.get<std::string>("<xmlattr>.key")
                    == "Serving Cell Index") {
                    if (pair.second.get_value<std::string>() == "PCell") {
                        status = Status::primary;
                    } else {
                        status = Status::non_primary;
                    }
                } else if (pair.second.get<std::string>("<xmlattr>.key")
                    == "RSRP") {
                    rsrp = pair.second.get_value<std::string>();
                }
                if (status != Status::unknown && !rsrp.empty()) {
                    break;
                }
            }
            if (status == Status::primary && !rsrp.empty()) {
                result += timestamp;
                result += " $ LTE_PHY_Serv_Cell_Measurement $ RSRP: ";
                result += rsrp;
                result += '\n';
            }
        }
    }

    insert_ordered_task(
        job.job_num,
        [result = std::move(result)] {
            (*g_output) << result;
        }
    );
}

/// This function compares the timestamp of the current packet and
/// the ranges provided by the --range argument. If the current timestamp
/// falls in any one of the time range, then the XML string is printed to
/// the output file without any modification, otherwise silently do nothing.
static void echo_packet_within_time_range(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    auto rawtime = timestamp_str2long(timestamp);
    if (rawtime == static_cast<time_t>(-1)) {
        insert_ordered_task(
            job.job_num,
            [timestamp = std::move(timestamp)] {
                std::cerr << "Warning (packet timestamp = "
                          + timestamp + "): \n"
                          << "Timestamp is not in the format "
                          << "\"%d-%d-%d %d:%d:%d.%*d\"\n";
            }
        );
        return;
    }

    bool within_range = false;
    for (auto range : g_valid_time_range) {
        if (range.first <= rawtime && rawtime <= range.second) {
            within_range = true;
            break;
        }
    }

    std::string content;
    if (within_range) {
        content = std::move(job.xml_string);
        content += '\n';
    }
    insert_ordered_task(
        job.job_num,
        [content = std::move(content)] {
            (*g_output) << content;
        }
    );
}

/// Print the type of the packet.
static void extract_packet_type(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    std::string packet_type;
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair"
            && i.second.get("<xmlattr>.key", std::string()) == "type_id") {
            packet_type = i.second.data();
            break;
        }
    }

    insert_ordered_task(
        job.job_num,
        [timestamp = std::move(timestamp),
         packet_type = std::move(packet_type)] {
            (*g_output) << timestamp << " $ "
                        << packet_type << std::endl;
        }
    );
}

/// Extract RLCUL/RLCDL PDUs fields in
/// LTE_RLC_UL_AM_All_PDU/LTE_RLC_DL_AM_All_PDU packets.
static void extract_rlc_am_all_pdu(
    pt::ptree &&tree, Job &&job, bool uplink) {
    auto &&timestamp = get_packet_time_stamp(tree);

    const char *rlc_lists_tag;
    const char *result_tag;
    if (uplink) {
        rlc_lists_tag = "RLCUL PDUs";
        result_tag = " $ LTE_RLC_UL_AM_All_PDU $ ";
    } else {
        rlc_lists_tag = "RLCDL PDUs";
        result_tag = " $ LTE_RLC_DL_AM_All_PDU $ ";
    }

    std::string result;
    auto &&rlcdl_pdu_lists = locate_disjoint_subtree_with_attribute(
        tree, "key", rlc_lists_tag
    );

    for (auto rlcdl_pdu_list : rlcdl_pdu_lists) {
        auto &&rlcdl_pdus = locate_disjoint_subtree_with_attribute(
            *rlcdl_pdu_list, "type", "dict"
        );
        for (auto rlcdl_pdu : rlcdl_pdus) {
            result += timestamp;
            result += result_tag;
            auto &&fields = rlcdl_pdu->get_child("dict");
            bool first = true;
            for (auto &&field : fields) {
                auto &&value = field.second.get_value<std::string>();
                auto &&key = field.second.get<std::string>("<xmlattr>.key");
                if (!first) {
                    result += ", ";
                } else {
                    first = false;
                }
                result += key;
                result += ": ";
                if (key == "RLC CTRL NACK") {
                    auto &&nack_sns = locate_disjoint_subtree_with_attribute(
                        field.second, "key", "NACK_SN"
                    );
                    std::string sns_str;
                    for (auto &&sns : nack_sns) {
                        if (!sns_str.empty()) {
                            sns_str += '/';
                        }
                        sns_str += sns->get_value<std::string>();
                    }
                    result += sns_str;
                } else if (key != "RLC DATA LI") {
                    result += value;
                } else {
                    result += "OMITTED";
                }
            }
            result += '\n';
        }
    }

    insert_ordered_task(
        job.job_num,
        [result = std::move(result)] {
            (*g_output) << result;
        }
    );
}

static void extract_rlc_dl_am_all_pdu(
    pt::ptree &&tree, Job &&job) {
    extract_rlc_am_all_pdu(std::move(tree), std::move(job), false);
}

static void extract_rlc_ul_am_all_pdu(
    pt::ptree &&tree, Job &&job) {
    extract_rlc_am_all_pdu(std::move(tree), std::move(job), true);
}

/// Print the packet to the output file if the timestamp
/// is greater or equal than that of the latest packet we have ever seen.
static void echo_packet_if_new(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    auto rawtime = timestamp_str2long_microsec_hack(timestamp);
    if_unlikely (rawtime == static_cast<time_t>(-1)) {
        insert_ordered_task(
            job.job_num,
            [timestamp = std::move(timestamp)] {
                std::cerr << "Warning (packet timestamp = "
                          + timestamp + "): \n"
                          << "Timestamp does not match the pattern "
                          << "\"%d-%d-%d %d:%d:%d.%d\" "
                          << "or \"%d-%d-%d %d:%d:%d\". "
                          << "Dropped." << std::endl;
            }
        );
        return;
    }

    insert_ordered_task(
        job.job_num,
        [content = std::move(job.xml_string), rawtime, timestamp] {
            if (rawtime >= g_latest_seen_timestamp) {
                (*g_output) << content << std::endl;
                g_latest_seen_timestamp = rawtime;
                g_latest_seen_ts_string = timestamp;
            } else {
                std::cerr << "Dropping packet: "
                          << timestamp << " < "
                          << g_latest_seen_ts_string << std::endl;
            }
        }
    );
}

static void update_reorder_window(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    auto rawtime = timestamp_str2long_microsec_hack(timestamp);
    if_unlikely (rawtime == static_cast<time_t>(-1)) {
        insert_ordered_task(
            job.job_num,
            [timestamp = std::move(timestamp)] {
                std::cerr << "Warning (packet timestamp = "
                          + timestamp + "): \n"
                          << "Timestamp does not match the pattern "
                          << "\"%d-%d-%d %d:%d:%d.%d\" "
                          << "or \"%d-%d-%d %d:%d:%d\". "
                          << "Dropped." << std::endl;
            }
        );
        return;
    }

    auto &&content = job.xml_string;
    insert_ordered_task(
        job.job_num,
        [content, rawtime]() mutable {
            g_reorder_window->update(
                rawtime, std::move(content)
            );
        }
    );
}
