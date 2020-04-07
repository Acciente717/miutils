/* Copyright [2020] Zhiyao Ma */
#include "macros.hpp"
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// This function extracts and prints PDCP PDU sizes from
/// LTE_PDCP_UL_Cipher_Data_PDU or LTE_PDCP_DL_Cipher_Data_PDU packets.
/// For downlink packets, it looks like below. (XXX is the extracted field.)
/// <dm_log_packet>
///     <pair key="type_id">LTE_PDCP_DL_Cipher_Data_PDU</pair>
///         ...
///         <pair key="PDU Size">XXX</pair>
///         ...
/// </dm_log_packet>
void extract_pdcp_cipher_data_pdu_packet(pt::ptree &&tree, Job &&job) {
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
