/* Copyright [2019] Zhiyao Ma */
#ifndef PARAMETERS_HPP_
#define PARAMETERS_HPP_

/// Extractor thread number limit.
constexpr int THREAD_LIMIT = 256;

/// Defalut extractor thread number.
constexpr int THREAD_DEFAULT = 16;

/// The full water mark for the queue between the splitter and the extractors.
/// `FULL_WATRE_MARK * g_thread_num` is the maximum job number that can be
/// buffered in the queue. If it reaches that value, the splitter will be
/// temporarily blocked.
constexpr int FULL_WATRE_MARK = 128;

/// The middle water mark for the queue between the spitter and the extractors.
/// `MIDDLE_WATER_MARK * g_thread_num` is the threshold for deciding whether
/// the producing speed of the splitter or the consuming speed of the
/// extractors is larger.
constexpr int MIDDLE_WATER_MARK = 32;

/// The low water mark for the queue between the splitter and the extractors.
/// When the number of pending jobs in the queue drops below
/// `LOW_WATER_MARK * g_thread_num`, the splitter thread will be notified
/// to resume running.
constexpr int LOW_WATER_MARK = 8;

/// The size of the buffer to read input characters.
constexpr int READ_BUFF_SIZE = 16384;

#endif  // PARAMETERS_HPP_
