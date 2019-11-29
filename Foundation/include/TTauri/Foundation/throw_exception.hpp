// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include <fmt/format.h>

namespace TTauri {

[[noreturn]] void _throw_invalid_operation_error(char const *source_file, int source_line, std::string message);
[[noreturn]] void _throw_overflow_error(char const *source_file, int source_line, std::string message);

template<typename... Args>
[[noreturn]] void throw_invalid_operation_error(char const *source_file, int source_line, char const *str, Args &&... args) {
    _throw_invalid_operation_error(source_file, source_line, fmt::format(str, std::forward<Args>(args)...));
}

template<typename... Args>
[[noreturn]] void throw_overflow_error(char const *source_file, int source_line, char const *str, Args &&... args) {
    _throw_overflow_error(source_file, source_line, fmt::format(str, std::forward<Args>(args)...));
}

#define TTAURI_THROW_INVALID_OPERATION_ERROR(...) throw_invalid_operation_error(__FILE__, __LINE__, __VA_ARGS__);
#define TTAURI_THROW_OVERFLOW_ERROR(...) throw_overflow_error(__FILE__, __LINE__, __VA_ARGS__);


}