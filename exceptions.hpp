/* Copyright [2019] Zhiyao Ma */
#ifndef EXCEPTIONS_HPP_
#define EXCEPTIONS_HPP_

#include <exception>
#include <stdexcept>

class ProgramBug : public std::runtime_error {
 public:
    explicit ProgramBug(const std::string &what_arg)
        : std::runtime_error(what_arg) {}
};

class ArgumentError : public std::runtime_error {
 public:
    explicit ArgumentError(const std::string &what_arg)
        : std::runtime_error(what_arg) {}
};

#endif  // EXCEPTIONS_HPP_
