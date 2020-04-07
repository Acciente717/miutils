/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

void extract_phy_pdsch_packet(pt::ptree &&tree, Job &&job) {
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
