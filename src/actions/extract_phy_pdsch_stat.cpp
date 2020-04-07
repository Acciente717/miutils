/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

void extract_phy_pdsch_stat_packet(pt::ptree &&tree, Job &&job) {
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
