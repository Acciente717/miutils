/* Copyright [2020] Zhiyao Ma */
#include "macros.hpp"
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// Print the packet to the output file if the timestamp
/// is greater or equal than that of the latest packet we have ever seen.
void echo_packet_if_new(
    pt::ptree &&tree, Job &&job) {
    auto &&timestamp = get_packet_time_stamp(tree);

    auto rawtime = timestamp_str2long_microsec_hack(timestamp);
    if_unlikely (rawtime == static_cast<time_t>(-1)) {
        insert_ordered_task(
            job.job_num,
            [timestamp = std::move(timestamp)] {
                std::cerr << "Warning (packet timestamp = "
                          + timestamp + "): \n"
                          << "Timestamp does not match the pattern "
                          << "\"%d-%d-%d %d:%d:%d.%d\" "
                          << "or \"%d-%d-%d %d:%d:%d\". "
                          << "Dropped." << std::endl;
            }
        );
        return;
    }

    insert_ordered_task(
        job.job_num,
        [content = std::move(job.xml_string), rawtime, timestamp] {
            if (rawtime >= g_latest_seen_timestamp) {
                (*g_output) << content << std::endl;
                g_latest_seen_timestamp = rawtime;
                g_latest_seen_ts_string = timestamp;
            } else {
                std::cerr << "Dropping packet: "
                          << timestamp << " < "
                          << g_latest_seen_ts_string << std::endl;
            }
        }
    );
}
