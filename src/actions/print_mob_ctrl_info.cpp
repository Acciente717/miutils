/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

// Recursively scan through the input XML tree. Yield true if it sees
// a field that contains "mobilityControlInfo is present" as a substring.
bool recursive_find_mobility_control_info(const pt::ptree &tree) {
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

void print_time_of_mobility_control_info(pt::ptree &&tree, Job &&job) {
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
