/**
 * Copyright [2019] Zhiyao Ma
 * 
 * This module implements a reorder window for sorting purpose.
 * 
 * Every packets within the window are sorted by their timestamp.
 * The difference of the timestamps of the oldest and the newest
 * packet will not be greater than the out-of-order tolerance
 * value. Otherwise, the older one will be sent to output immediately.
 */

#include "sorter.hpp"
#include "exceptions.hpp"
#include "global_states.hpp"

ReorderWindow::ReorderWindow(time_t ooo_tolerance_) {
    if (static_cast<long>(ooo_tolerance_) <= 0L) {
        throw ArgumentError(
            "Reorder window size must be greater than 0, "
            "given: " + std::to_string(ooo_tolerance_)
        );
    }
    ooo_tolerance = ooo_tolerance_;
}

/// Send all remaining packets to the output in sequence.
void ReorderWindow::flush() {
    for (auto &p : window) {
        (*g_output) << p.second << std::endl;
    }
    window.clear();
}

/// Insert a new packet in to the window. Conditionally evict
/// older packets to the output if the window size is exceeded.
void ReorderWindow::update(time_t timestamp, std::string &&str) {
    // Insert into the multimap with hint. Assume that most
    // of the packets are in-sequence, so that we would better
    // start searching the insertion point from the tail.
    window.emplace_hint(window.end(), timestamp, str);

    // Evict all older packets causing the exceeding of the
    // window size.
    auto largest_time = window.rbegin()->first;
    for (auto it = window.begin();
         largest_time - it->first > ooo_tolerance;
         it = window.erase(it)) {
        (*g_output) << it->second << std::endl;
    }
}
