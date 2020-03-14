/* Copyright [2019] Zhiyao Ma */
#ifndef SORTER_HPP_
#define SORTER_HPP_

#include <map>
#include <ctime>
#include <string>

/// A packet sorter.
class ReorderWindow {
    time_t ooo_tolerance;
    std::multimap<time_t, std::string> window;
 public:
    explicit ReorderWindow(time_t ooo_tolerance_);
    void update(time_t timestamp, std::string &&str);
    void flush();
};

#endif  // SORTER_HPP_
