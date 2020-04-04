/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// Print the type of the packet.
void extract_packet_type(pt::ptree &&tree, Job &&job) {
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
