/* Copyright [2020] Zhiyao Ma */
#include "macros.hpp"
#include "actions.hpp"
#include "exceptions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// Extract RLC_DL/UL_CONFIG_LOG_PACKET for the `Added/Modified RBs`
/// field, the `Released RBs` field, the `Active RBs` field and the
/// `Reason` field.
static void extract_rlc_config_log_packet(
    pt::ptree &&tree, Job &&job, const char *pkt_name) {
    auto &&timestamp = get_packet_time_stamp(tree);
    std::string result;

    auto &&config_reasons = locate_disjoint_subtree_with_attribute(
        tree, "key", "Reason"
    );
    if_unlikely (config_reasons.size() != 1UL) {
        throw InputError(
            "RLC_CONFIG_LOG_PACKET does not have a \"Reason\" field."
        );
    }
    auto &&reason = config_reasons[0]->get_value<std::string>();
    reason = "Reason: " + reason;

    auto &&collect_rb_config_result
        = [&tree, &result, &timestamp, pkt_name, &reason](const char *type) {
        auto &&add_mod_rb_lists = locate_disjoint_subtree_with_attribute(
            tree, "key", type
        );
        for (auto &&lists : add_mod_rb_lists) {
            auto &&dicts = locate_disjoint_subtree_with_attribute(
                *lists, "type", "dict"
            );
            for (auto &&dict : dicts) {
                result += timestamp;
                result += " $ ";
                result += pkt_name;
                result += " $ ";
                result += reason;
                result += ", Category: ";
                result += type;
                for (auto &&pair : dict->get_child("dict")) {
                    result += ", ";
                    result += pair.second.get<std::string>("<xmlattr>.key");
                    result += ": ";
                    result += pair.second.get_value<std::string>();
                }
                result += '\n';
            }
        }
    };

    collect_rb_config_result("Added/Modified RBs");
    collect_rb_config_result("Released RBs");
    collect_rb_config_result("Active RBs");

    insert_ordered_task(
        job.job_num,
        [result = std::move(result)] {
            (*g_output) << result;
        }
    );
}

void extract_rlc_dl_config_log_packet(pt::ptree &&tree, Job &&job) {
    extract_rlc_config_log_packet(
        std::move(tree), std::move(job), "LTE_RLC_DL_Config_Log_Packet"
    );
}

void extract_rlc_ul_config_log_packet(pt::ptree &&tree, Job &&job) {
    extract_rlc_config_log_packet(
        std::move(tree), std::move(job), "LTE_RLC_UL_Config_Log_Packet"
    );
}
