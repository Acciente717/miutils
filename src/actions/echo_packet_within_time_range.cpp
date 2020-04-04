/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// This function compares the timestamp of the current packet and
/// the ranges provided by the --range argument. If the current timestamp
/// falls in any one of the time range, then the XML string is printed to
/// the output file without any modification, otherwise silently do nothing.
void echo_packet_within_time_range(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    auto rawtime = timestamp_str2long(timestamp);
    if (rawtime == static_cast<time_t>(-1)) {
        insert_ordered_task(
            job.job_num,
            [timestamp = std::move(timestamp)] {
                std::cerr << "Warning (packet timestamp = "
                          + timestamp + "): \n"
                          << "Timestamp is not in the format "
                          << "\"%d-%d-%d %d:%d:%d.%*d\"\n";
            }
        );
        return;
    }

    bool within_range = false;
    for (auto range : g_valid_time_range) {
        if (range.first <= rawtime && rawtime <= range.second) {
            within_range = true;
            break;
        }
    }

    std::string content;
    if (within_range) {
        content = std::move(job.xml_string);
        content += '\n';
    }
    insert_ordered_task(
        job.job_num,
        [content = std::move(content)] {
            (*g_output) << content;
        }
    );
}
