/* Copyright [2020] Zhiyao Ma */
#include "macros.hpp"
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

void update_reorder_window(pt::ptree &&tree, Job &&job) {
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

    auto &&content = job.xml_string;
    insert_ordered_task(
        job.job_num,
        [content, rawtime]() mutable {
            g_reorder_window->update(
                rawtime, std::move(content)
            );
        }
    );
}
