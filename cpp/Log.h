#ifndef LOG_H
#define LOG_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>

struct Log {

    const char *prefix;

    Log (const char *p): prefix(p) {
        trace("BEGIN\n");
    }
    ~Log () {
        trace("END\n");
    }

    inline void fatal (const char *fmt, ...) {
        va_list args;
        std::string s("[!!] ");
        s += prefix;
        s += fmt;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }

    inline void error (const char *fmt, ...) {
        va_list args;
        std::string s(" [!] ");
        s += prefix;
        s += fmt;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }

    inline void warn (const char *fmt, ...) {
        va_list args;
        std::string s(" [?] ");
        s += prefix;
        s += fmt;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }

    inline void info (const char *fmt, ...) {
        va_list args;
        std::string s("     ");
        s += prefix;
        s += fmt;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }

    inline void debug (const char *fmt, ...) {
        va_list args;
        std::string s("   . ");
        s += prefix;
        s += fmt;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }

    inline void trace (const char *fmt, ...) {
        va_list args;
        std::string s("  .. ");
        s += prefix;
        s += fmt;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }
};

#endif

