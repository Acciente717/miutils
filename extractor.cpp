/**
 * Copyright [2019] Zhiyao Ma
 * 
 * This module implements a thread pool of extractors. Each extractor
 * runs on a Job structure. The Job structure contains a string, which
 * is a valid XML text string, and an associated sequence number.
 * 
 * Each extractor iterates through the `g_action_list`. When the predicate
 * function yields true, it calls the associated action function. For
 * instance, if we want to extract the time when we receive handover
 * command, the predicate function should return true on seeing
 * `mobilityControlInfo` field in the parsed XML tree, and the action
 * function should print out the timestamp.
 * 
 * The splitter module acts as the job producer to all extractors.
 */
#include "extractor.hpp"
#include "action_list.hpp"
#include "global_states.hpp"
#include "exceptions.hpp"
#include "parameters.hpp"
#include "macros.hpp"
#include <queue>
#include <mutex>
#include <vector>
#include <thread>
#include <condition_variable>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

static void take_actions_on_input(Job job);
static void smain_extractor();

/// The mutex lock guarding all other static global variables.
static std::mutex g_extractors_mtx;
/// Thread objects that run extractor threads.
static std::vector<std::thread> g_extractors;
/// The number of alive extractor threads.
static int g_alive_extractor_num = 0;
/// The number of running extractor threads.
static int g_running_extractor_num = 0;
/// The flag indicating whether the splitter, which acts as the producer of
/// the extractors, has finished its execution.
static bool g_splitter_finished = false;
/// The flag indication whether an error has occured and we should stop
/// all extractors prematurely.
static bool g_early_terminating = false;
/// The queue for storing pending jobs.
static std::queue<Job> g_job_queue;
/// The condition variable which is used to notify extractor threads that
/// the job_queue has become non-empty.
static std::condition_variable g_job_queue_nonempty_cv;
/// The condition variable which is used to notify the splitter thread,
/// which acts as the producer of the job_queue, that the job_queue has
/// become non-full.
static std::condition_variable g_job_queue_nonfull_cv;

/// Start the extractor threads. The number of extractor threads will
/// be `g_thread_num`.
void start_extractor() {
    std::lock_guard<std::mutex> guard(g_extractors_mtx);
    g_splitter_finished = false;
    for (int i = 0; i < g_thread_num; ++i) {
        g_extractors.emplace_back(smain_extractor);
    }
    g_alive_extractor_num = g_thread_num;
    g_running_extractor_num = g_thread_num;
}

/// Join all extractor threads.
void join_extractor() {
    for (auto &i : g_extractors) {
        if (i.joinable()) {
            i.join();
        }
    }
    g_extractors.clear();
}

/// Terminate all extractor threads prematurely.
void kill_extractor() {
    std::lock_guard<std::mutex> guard(g_extractors_mtx);
    g_early_terminating = true;
    g_job_queue_nonfull_cv.notify_all();
    g_job_queue_nonempty_cv.notify_all();
}

/// Notify all extractors threads that the splitter, which acts as the
/// producer of extractors, has finished execution.
void notify_splitter_finished() {
    std::lock_guard<std::mutex> guard(g_extractors_mtx);
    g_splitter_finished = true;
    g_job_queue_nonempty_cv.notify_all();
}

/// Add a new job to the extractors. This function may block if the
/// `job_queue` is currently full.
void produce_job_to_extractor(Job job) {
    std::unique_lock<std::mutex> queue_lck(g_extractors_mtx);

    // If `g_job_queue` is full, we must wait.
    g_job_queue_nonfull_cv.wait(
        queue_lck,
        []{
            return g_splitter_finished || g_early_terminating
                   || g_job_queue.size() < g_thread_num * HIGH_WATRE_MARK;
        }
    );

    /// Terminate prematurely.
    if_unlikely (g_early_terminating) {
        return;
    }

    // If the splitter, which is the producer of all extractors, is set
    // to be finished execution, then this is an error.
    if_unlikely (g_splitter_finished) {
        throw ProgramBug(
            "The splitter has been marked finished. However it is still "
            "producing new jobs to the extractors."
        );
    }

    // If we have any sleeping thread, choose one to wake it up.
    if (g_running_extractor_num != g_alive_extractor_num) {
        g_job_queue_nonempty_cv.notify_one();
    }

    g_job_queue.emplace(std::move(job));
}

/// When all extractors have finished execution, the last one finished calls
/// this funcion to notify the main thread about this.
static void notify_main_thread() {
    std::lock_guard<std::mutex> guard(g_main_state_mtx);

    // If the main state is SplitterFinished, change it to ExtractorFinished.
    if (g_main_state == MainState::SplitterFinished) {
        g_main_state = MainState::ExtractorFinished;
        g_main_state_change_cv.notify_one();

    // If the main state is Error, leave it untouch.
    // If the main state is a value other than SplitterFinished or Error,
    // it is a bug of the program and we should raise an exception.
    } else if_unlikely (g_main_state != MainState::Error) {
        throw ProgramBug(
            "All extractors have just finished execution. "
            "The main state should be either SplitterFinished "
            "or Error, but is neither."
        );
    }
}

// The entrance function for sub(threads) running extractors.
static void smain_extractor() {
    try {
        std::unique_lock<std::mutex> queue_lck(g_extractors_mtx,
                                               std::defer_lock);
        while (true) {
            queue_lck.lock();

            // If all jobs are finished, exit the thread. Notify the main
            // thread when the last extractor thread exits.
            if (g_splitter_finished && g_job_queue.empty()) {
                if (--g_alive_extractor_num == 0) {
                    queue_lck.unlock();
                    notify_main_thread();
                }
                return;
            }

            // If the `g_job_queue` is empty but the splitter is still producing
            // new jobs, wait.
            if (g_job_queue.empty()) {
                --g_running_extractor_num;
                g_job_queue_nonempty_cv.wait(
                    queue_lck,
                    []{ return !g_job_queue.empty() || g_splitter_finished
                               || g_early_terminating; }
                );
                ++g_running_extractor_num;
            }

            // Terminate prematurely.
            if_unlikely (g_early_terminating) {
                return;
            }

            // If `g_job_queue` is empty, we woke from the above wait function
            // because `g_splitter_finished` was `true`. Since the splitter
            // has stop producing and the `g_job_queue` is empty, we should exit
            // the thread. Notify the main thread when the last extractor
            // thread exits.
            if (g_job_queue.empty()) {
                if (--g_alive_extractor_num == 0) {
                    queue_lck.unlock();
                    notify_main_thread();
                }
                return;
            }

            // If the queue is previously full, we should notify the splitter
            // that the queue now has an empty slot.
            if (g_job_queue.size() <= g_thread_num * LOW_WATER_MARK) {
                g_job_queue_nonfull_cv.notify_one();
            }

            // Consume a job from the queue and take actions.
            auto job = std::move(g_job_queue.front());
            g_job_queue.pop();
            queue_lck.unlock();
            take_actions_on_input(std::move(job));
        }
    } catch (...) {
        propagate_exeption_to_main();
    }
}

/// Scan through the action list. Take according action when the first
/// predicate function yields true.
static void take_actions_on_input(Job job) {
    // Convert the input string to an input stream and then
    // build the property tree.
    std::stringstream stream(job.xml_string);
    pt::ptree tree;
    pt::read_xml(stream, tree);

    // Scan through the action list.
    for (auto &i : g_action_list) {
        // If we find a predicate function yields true on the
        // input, take according action.
        if (i.predicate(tree, job)) {
            i.action(std::move(tree), std::move(job));
            return;
        }
    }

    throw ProgramBug(
        "All predicate functions in the action list yield false. "
        "The last predicate function MUST yield true."
    );
}
