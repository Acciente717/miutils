/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// Extract RLCUL/RLCDL PDUs fields in
/// LTE_RLC_UL_AM_All_PDU/LTE_RLC_DL_AM_All_PDU packets.
static void extract_rlc_am_all_pdu(
    pt::ptree &&tree, Job &&job, bool uplink) {
    auto &&timestamp = get_packet_time_stamp(tree);

    const char *rlc_lists_tag;
    const char *result_tag;
    if (uplink) {
        rlc_lists_tag = "RLCUL PDUs";
        result_tag = " $ LTE_RLC_UL_AM_All_PDU $ ";
    } else {
        rlc_lists_tag = "RLCDL PDUs";
        result_tag = " $ LTE_RLC_DL_AM_All_PDU $ ";
    }

    std::string result;
    auto &&rlcdl_pdu_lists = locate_disjoint_subtree_with_attribute(
        tree, "key", rlc_lists_tag
    );

    for (auto rlcdl_pdu_list : rlcdl_pdu_lists) {
        auto &&rlcdl_pdus = locate_disjoint_subtree_with_attribute(
            *rlcdl_pdu_list, "type", "dict"
        );
        for (auto rlcdl_pdu : rlcdl_pdus) {
            result += timestamp;
            result += result_tag;
            auto &&fields = rlcdl_pdu->get_child("dict");
            bool first = true;
            for (auto &&field : fields) {
                auto &&value = field.second.get_value<std::string>();
                auto &&key = field.second.get<std::string>("<xmlattr>.key");
                if (!first) {
                    result += ", ";
                } else {
                    first = false;
                }
                result += key;
                result += ": ";
                if (key == "RLC CTRL NACK") {
                    auto &&nack_sns = locate_disjoint_subtree_with_attribute(
                        field.second, "key", "NACK_SN"
                    );
                    std::string sns_str;
                    for (auto &&sns : nack_sns) {
                        if (!sns_str.empty()) {
                            sns_str += '/';
                        }
                        sns_str += sns->get_value<std::string>();
                    }
                    result += sns_str;
                } else if (key != "RLC DATA LI") {
                    result += value;
                } else {
                    result += "OMITTED";
                }
            }
            result += '\n';
        }
    }

    insert_ordered_task(
        job.job_num,
        [result = std::move(result)] {
            (*g_output) << result;
        }
    );
}

void extract_rlc_dl_am_all_pdu(
    pt::ptree &&tree, Job &&job) {
    extract_rlc_am_all_pdu(std::move(tree), std::move(job), false);
}

void extract_rlc_ul_am_all_pdu(
    pt::ptree &&tree, Job &&job) {
    extract_rlc_am_all_pdu(std::move(tree), std::move(job), true);
}
