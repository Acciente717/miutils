/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

void echo_packet_if_match(pt::ptree &&tree, Job &&job) {
    auto &&type = get_packet_type(tree);
    if (std::regex_match(type, g_packet_type_regex)) {
        insert_ordered_task(
            job.job_num,
            [result = std::move(job.xml_string)] {
                (*g_output) << result << std::endl;
            }
        );
    } else {
        insert_ordered_task(
            job.job_num, []{}
        );
    }
}
