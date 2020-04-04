/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// This function extracts and prints random access results
/// from LTE_MAC_Rach_Attempt packets. It looks for the
/// pattern shown below.
/// <dm_log_packet>
///     ...
///         <pair key="Rach result"> XXX </pair>
///     ...
/// </dm_log_packet>
void extract_mac_rach_attempt_packet(
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
void extract_lte_mac_rach_trigger_packet(
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
