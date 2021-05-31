//          Copyright Jean Pierre Cimalando 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE.md or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <cstdarg>

#if defined(__GNUC__)
    #define PRINTF_ATTR(a, b) __attribute__((format(printf, a, b)));
#else
    #define PRINTF_ATTR(a, b)
#endif

class Log {
public:
    static void i(const char *format, ...) PRINTF_ATTR(1, 2);
    static void w(const char *format, ...) PRINTF_ATTR(1, 2);
    static void e(const char *format, ...) PRINTF_ATTR(1, 2);
    static void s(const char *format, ...) PRINTF_ATTR(1, 2);

    static void vi(const char *format, va_list ap);
    static void vw(const char *format, va_list ap);
    static void ve(const char *format, va_list ap);
    static void vs(const char *format, va_list ap);

private:
    static void generic(char symbol, const char *tag, const char *color, const char *format, va_list ap);
};

void panic(const char *format, ...) PRINTF_ATTR(1, 2);
