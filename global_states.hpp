/* Copyright [2019] Zhiyao Ma */
#ifndef GLOBAL_STATES_HPP_
#define GLOBAL_STATES_HPP_

#include <exception>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>
#include <iostream>
#include <vector>

/// The states for the state machine in the main function.
enum class MainState {
    /// Initializing action list and starting sub threads.
    Initializing,
    /// All sub threads are running without exceptions.
    AllRunning,
    /// The splitter has finished, while the extractors and the in-order
    /// executor are still running.
    SplitterFinished,
    /// The splitter and all the extractors have finished, while the
    /// in-order executor is still running.
    ExtractorFinished,
    /// All sub threads have finished.
    InOrderExecutorFinished,
    /// Exceptions are thrown in some sub threads.
    Error
};

/// Parameter: the number of extractor thread.
extern int g_thread_num;

/// Parameter: input file names.
extern std::vector<std::string> g_input_file_names;

/// Parameter: input file streams.
extern std::vector< std::unique_ptr<std::istream,
                    std::function<void(std::istream*)>> > g_inputs;

/// Parameter: output file stream.
extern std::unique_ptr<std::ostream,
                       std::function<void(std::ostream*)> > g_output;

/// The global exception pointer.
extern std::exception_ptr g_pexcept;

/// The mutex protecting the main state and the global exception pointer.
extern std::mutex g_main_state_mtx;

/// The enum representing the main state.
extern MainState g_main_state;

/// The condition variable used to notify the change of main state.
extern std::condition_variable g_main_state_change_cv;

/// Sub threads call this function to propagate caught exception to the
/// main thread. It changes the main state to Error and set the exception
/// pointer.
extern void propagate_exeption_to_main();

#endif  // GLOBAL_STATES_HPP_
