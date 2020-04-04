/* Copyright [2019] Zhiyao Ma */
#ifndef IN_ORDER_EXECUTOR_HPP_
#define IN_ORDER_EXECUTOR_HPP_

#include <functional>

/// A function to be executed (a task) associated with a sequence number.
struct OrderedTask {
    long seq_num;
    std::function<void()> func;
    friend bool operator>(const OrderedTask &lhs, const OrderedTask &rhs);
};

inline bool operator>(const OrderedTask &lhs, const OrderedTask &rhs) {
    return lhs.seq_num > rhs.seq_num;
}

/// Provide a task associated with a sequence number to the in-order
/// executor. Note that the producer to this module MUST guarantee that the
/// provided sequence number is consecutive.
extern void insert_ordered_task(long seq_num, std::function<void()> func);

/// Kill the in-order executor prematurely. Note that the thread is not
/// joined in this function. One should call `join_in_order_executor`
/// afterwards.
extern void kill_in_order_executor();

/// Start the in-order executor.
extern void start_in_order_executor();

/// Join the in-order executor thread.
extern void join_in_order_executor();

/// Notify the in-order executor that the extractors, which act as the
/// producer to it, have all exited.
extern void notify_extractor_finished();

#endif  // IN_ORDER_EXECUTOR_HPP_
