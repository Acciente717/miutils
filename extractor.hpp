/* Copyright [2019] Zhiyao Ma */
#ifndef EXTRACTOR_HPP_
#define EXTRACTOR_HPP_

#include <string>

/// The job structure that the splitter provides to the extractors.
struct Job {
    /// The sequence number. It is in ascending order and is consecutive.
    long job_num;
    /// The XML text string.
    std::string xml_string;
    /// The input file name.
    std::string file_name;
    /// The line number corresponding to the start of the
    /// XML string in the input file.
    long start_line_number;
    /// The line number corresponding to the end of the
    /// XML string in the input file.
    long end_line_number;
};

/// Start the extractor threads. The number of extractor threads will
/// be `g_thread_num`.
extern void start_extractor();

/// Join all extractor threads.
extern void join_extractor();

/// Notify all extractors threads that the splitter, which acts as the
/// producer of extractors, has finished execution.
extern void notify_splitter_finished();

/// Add a new job to the extractors. This function may block if the
/// `job_queue` is currently full.
extern void produce_job_to_extractor(Job job);

#endif  // EXTRACTOR_HPP_
