/* Copyright [2019] Zhiyao Ma */
#ifndef ACTION_LIST_HPP_
#define ACTION_LIST_HPP_

#include <vector>
#include <functional>
#include <boost/property_tree/ptree.hpp>

namespace pt = boost::property_tree;

class Job;

/// A pair of functions. The predicate function will be called first.
/// If it yields true, the action function will then be called.
struct ConditionalAction {
    std::function<bool(const pt::ptree &, const Job &)> predicate;
    std::function<void(pt::ptree &&, Job &&)> action;
};

using ActionList = std::vector<ConditionalAction>;

/// The list storing all `ConditionalAction`s.
extern ActionList g_action_list;

/// Initialize the `g_action_list`. It pushes `ConditionalActions` to
/// the list. Note that we need to push a guard predicate function which
/// always yields true at the end of the list. This is because each action
/// function acts as the producer to the in-order executor module. That module
/// executes all output tasks in sequence (by looking at the sequence number).
/// Even if we have no output on the current input XML tree, we should
/// produce a dummy output task, which essentially output nothing, to the
/// in-order executor module.
extern void initialize_action_list_with_extractors();

/// Initialize the `g_action_list` to do the filter work. Due to the same
/// reason as `initialize_action_list_with_extractors()`, we must put
/// a dummy function at the end of the list.
extern void initialize_action_list_with_filter();

#endif  // ACTION_LIST_HPP_
