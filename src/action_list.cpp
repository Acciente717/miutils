/**
 * Copyright [2019] Zhiyao Ma
 * 
 * This module defines all actions that need to be taken by the extractor.
 * 
 * Each `ConditionalAction` contains a predicate and an action function.
 * The predicate function is called first, and if it yields true, the action
 * function will then be called.
 * 
 * All `ConditionalAction`s are stored in a list. Note that ONLY THE FIRST
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
    RLC_DL_CONFIG_LOG,
    RLC_UL_CONFIG_LOG,
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
        {"rlc_dl_config_log", ExtractorEnum::RLC_DL_CONFIG_LOG},
        {"rlc_ul_config_log", ExtractorEnum::RLC_UL_CONFIG_LOG},
        {"all_packet_type", ExtractorEnum::ALL_PACKET_TYPE},
        {"action_pdcp_cipher_data_pdu",
         ExtractorEnum::ACTION_PDCP_CIPHER_DATA_PDU}
    };

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
                    extract_nas_emm_ota_incoming_packet
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
                    extract_nas_emm_ota_outgoing_packet
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
                    extract_mac_rach_attempt_packet
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
                    extract_phy_pdsch_stat_packet
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
                    extract_phy_pdsch_packet
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
                    extract_phy_serv_cell_measurement
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
        case ExtractorEnum::RLC_DL_CONFIG_LOG:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_RLC_DL_Config_Log_Packet"
                        );
                    },
                    extract_rlc_dl_config_log_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_RLC_DL_Config_Log_Packet" << std::endl;
            break;
        case ExtractorEnum::RLC_UL_CONFIG_LOG:
            g_action_list.push_back(
                {
                    [] (const pt::ptree &tree, const Job &job) {
                        return is_packet_having_type(
                            tree, "LTE_RLC_UL_Config_Log_Packet"
                        );
                    },
                    extract_rlc_ul_config_log_packet
                }
            );
            std::cerr << "Extractor enabled: "
                      << "LTE_RLC_UL_Config_Log_Packet" << std::endl;
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

/// Initialize the `g_action_list` to do the range filter work. Since the
/// predicate function always return true, we do not need another dummy
/// function at the end of the list.
void initialize_action_list_with_range() {
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, const Job &job) { return true; },
            echo_packet_within_time_range
        }
    );
}

/// Initialize the `g_action_list` to do the deduplicate work. Since the
/// predicate function always return true, we do not need another dummy
/// function at the end of the list.
void initialize_action_list_to_dedup() {
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, const Job &job) { return true; },
            echo_packet_if_new
        }
    );
}

/// Initialize the `g_action_list` to do the reorder work. Since the
/// predicate function always return true, we do not need another dummy
/// function at the end of the list.
void initialize_action_list_to_reorder() {
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, const Job &job) { return true; },
            update_reorder_window
        }
    );
}

/// Initialize the `g_action_list` to do the filter work. Since the
/// predicate function always return true, we do not need another dummy
/// function at the end of the list.
void initialize_action_list_to_filter() {
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, const Job &job) { return true; },
            echo_packet_if_match
        }
    );
}
