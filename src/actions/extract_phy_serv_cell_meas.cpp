/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

void extract_phy_serv_cell_measurement(pt::ptree &&tree, Job &&job) {
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
