/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// Print the timestamp that located at path
/// "dm_log_packet.pair.<xmlattr>.key.timestamp". It throws an exception if
/// the path is invalid.
void print_timestamp(pt::ptree &&tree, long seq_num) {
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
