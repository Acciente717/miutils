/**
 * Copyright [2019] Zhiyao Ma
 * 
 * This module defines all actions that need to be taken by the extractor.
 * 
 * Each `ConditionalAction` contains a predicate and an action function.
 * The predicate function is called first, and if it yields true, the action
 * function will then be called.
 * 
 * All `CondiitionalAction`s are stored in a list. Note that ONLY THE FIRST
 * action function in the list whose corresponding predicate function yields
 * true will be called. All predicate and action functions after it will be
 * skipped.
 * 
 * HOW TO ADD A NEW ACTION:
 * 1. Write a predicate function with signature
 *    (const pt::ptree &, long) -> bool
 * 2. Write a action function with signature
 *    (pt::ptree &&tree, long seq_num) -> void
 * 3. Push them to `g_action_list` in the function `initialize_action_list`.
 *    Note that the last predicate function MUST yield true.
 * 4. Make sure that if you want to output anything in the action function,
 *    wrap it in the lambda and pass it to the in-order executor by calling
 *    `insert_ordered_task`. See `print_timestamp` for example.
 */
#include "action_list.hpp"
#include "in_order_executor.hpp"
#include "global_states.hpp"
#include <string>
#include <iostream>

static bool recursive_find_mobility_control_info(const pt::ptree &tree);
static void print_timestamp(pt::ptree &&tree, long seq_num);

/// The list storing all `ConditionalAction`s.
ActionList g_action_list;


/// Initialize the `g_action_list`. It pushes `ConditionalActions` to
/// the list. Note that we need to push a guard predicate function which
/// always yields true at the end of the list. This is because each action
/// function acts as the producer to the in-order executor module. That module
/// executes all output tasks in sequence (by looking at the sequence number).
/// Even if we have no output on the current input XML tree, we should
/// produce a dummy output task, which essentially output nothing, to the
/// in-order executor module.
void initialize_action_list() {
    // Predicate: find the "mobilityControlInfo is present" string recursively.
    // Action: print the timestamp of this packet.
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, long seq_num) {
                return recursive_find_mobility_control_info(tree);
            },
            print_timestamp
        }
    );

    // Predicate: always true
    // Action: do nothing
    g_action_list.push_back(
        {
            [](const pt::ptree &tree, long seq_num) { return true; },
            [](pt::ptree &&tree, long seq_num) {
                insert_ordered_task(seq_num, []{});
            }
        }
    );
}

// Recursively scan through the input XML tree. Yield true if it sees
// a field that contains "mobilityControlInfo is present" as a substring.
static bool recursive_find_mobility_control_info(const pt::ptree &tree) {
    if (tree.data().find("mobilityControlInfo is present")
        != std::string::npos) {
        return true;
    }
    for (const auto &i : tree) {
        if (recursive_find_mobility_control_info(i.second)) {
            return true;
        }
    }
    return false;
}

/// Print the timestamp that located at path
/// "dm_log_packet.pair.<xmlattr>.key.timestamp". It throws an exception if
/// the path is invalid.
static void print_timestamp(pt::ptree &&tree, long seq_num) {
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
