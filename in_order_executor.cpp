/**
 * Copyright [2019] Zhiyao Ma
 * 
 * This module implementes an in-order executor.
 * 
 * The extractors act as the
 * producer for this in-order executor. Each task provided is associated with
 * a sequence number. If they happen to arrive out-of-order, e.g. some tasks
 * with larger sequence numbers arrive before those with smaller ones, they
 * will be temporarily buffered in the queue. In other words, provided tasks
 * are executed in strict ascending order of the sequence number, e.g. i-th,
 * (i+1)-th, (i+2)-th...
 * 
 * Note that the producer to this module MUST guarantee that the provided
 * sequence number is consecutive.
 */
#include "in_order_executor.hpp"
#include "global_states.hpp"
#include "exceptions.hpp"
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

/// The priority queue to guarantee in-order execution.
static std::priority_queue<OrderedTask,
                           std::vector<OrderedTask>,
                           std::greater<OrderedTask>> g_pending_task;
/// The mutex lock that guards on all static global variables.
static std::mutex g_pending_task_mtx;
/// The condition variable used to notify that the priority queue has
/// just become non-empty.
static std::condition_variable g_pending_task_nonempty_cv;
/// The sequence number of the next task to be executed.
static long g_next_task_num = 0;
/// The flag indicating whether the in-order executor should exit prematurely.
static bool g_early_terminating = false;
/// The flag indicating whether all the extractors, which act as the
/// producer to the in-order executor, has exited.
static bool g_no_more_task = false;
/// The thread object of the in-order executor.
static std::thread g_executor_thread;

/// Provide a task associated with a sequence number to the in-order
/// executor. Note that the producer to this module MUST guarantee that the
/// provided sequence number is consecutive.
void insert_ordered_task(long seq_num, std::function<void()> func) {
    std::lock_guard<std::mutex> guard(g_pending_task_mtx);
    if (seq_num == g_next_task_num) {
        g_pending_task_nonempty_cv.notify_one();
    }
    g_pending_task.push({seq_num, func});
}

/// When the in-order executor has finished execution, it calls this funcion
/// to notify the main thread about this.
static void notify_main_thread() {
    std::lock_guard<std::mutex> guard(g_main_state_mtx);

    // If the main state is ExtractorFinished, change it to
    // InOrderExecutorFinished.
    if (g_main_state == MainState::ExtractorFinished) {
        g_main_state = MainState::InOrderExecutorFinished;
        g_main_state_change_cv.notify_one();

    // If the main state is Error, leave it untouch.
    // If the main state is a value other than SplitterFinished or Error,
    // it is a bug of the program and we should raise an exception.
    } else if (g_main_state != MainState::Error) {
        throw ProgramBug(
            "In-order executor has just finished execution. "
            "The main state should be either ExtractorFinished "
            "or Error, but is neither."
        );
    }
}

/// The entrance function for the in-order executor.
static void smain_in_order_executor() {
    try {
        std::unique_lock<std::mutex> lck(g_pending_task_mtx);
        while (true) {
            // Wait if we currently have no more task to execute or we cannot
            // execute them in-order.
            g_pending_task_nonempty_cv.wait(
                lck,
                [] { return
                        g_early_terminating ||
                        (!g_pending_task.empty() &&
                        g_pending_task.top().seq_num == g_next_task_num) ||
                        g_no_more_task;
                }
            );

            // Check if we should exit prematuerly.
            if (g_early_terminating) {
                return;
            }

            // Check if the producer has exited.
            if (g_no_more_task) {
                // If we have finished all tasks, we should notify the main
                // thread and exit now.
                if (g_pending_task.empty()) {
                    lck.unlock();
                    notify_main_thread();
                    return;
                }
                // If we still have pending tasks, but they are out-of-order,
                // they can never be executed in-order. We should throw an
                // exception. This is a program bug.
                if (g_pending_task.top().seq_num != g_next_task_num) {
                    throw ProgramBug(
                        "All extractors have finished execution. There will "
                        "be no more task for the in-order executor. However "
                        "The in-order executor still has pending tasks, but "
                        "they are out-of-order. They can never be executed "
                        "in-order."
                    );
                }
            }

            // Execute in-order tasks.
            while (!g_pending_task.empty() &&
                g_pending_task.top().seq_num == g_next_task_num) {
                g_pending_task.top().func();
                g_pending_task.pop();
                ++g_next_task_num;
            }
        }
    } catch (...) {
        propagate_exeption_to_main();
    }
}

/// Start the in-order executor.
void start_in_order_executor() {
    std::lock_guard<std::mutex> guard(g_pending_task_mtx);
    g_next_task_num = 0;
    g_early_terminating = false;
    g_no_more_task = false;
    g_executor_thread = std::thread(smain_in_order_executor);
}

/// Kill the in-order executor prematurely. Note that the thread is not
/// joined in this function. One should call `join_in_order_executor`
/// afterwards.
void kill_in_order_executor() {
    std::lock_guard<std::mutex> guard(g_pending_task_mtx);
    g_early_terminating = true;
    g_pending_task_nonempty_cv.notify_one();
}

/// Notify the in-order executor that the extractors, which act as the
/// producer to it, have all exited.
void notify_extractor_finished() {
    std::lock_guard<std::mutex> guard(g_pending_task_mtx);
    g_no_more_task = true;
    g_pending_task_nonempty_cv.notify_one();
}

/// Join the in-order executor thread.
void join_in_order_executor() {
    if (g_executor_thread.joinable()) {
        g_executor_thread.join();
    }
}
