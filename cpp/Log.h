#ifndef LOG_H
#define LOG_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <syscall.h>

using namespace std;

struct Log {

    const char *prefix;

    Log (const char *p): prefix(p) {
        //trace("BEGIN\n");
    }
    ~Log () {
        //trace("END\n");
    }

    inline string tid () {
        char s[10];
        sprintf(s, "%08x ", (unsigned int)syscall(SYS_gettid));
        return s+6;
    }

    inline void fatal (const char *fmt, ...) {
        va_list args;
        std::string s("[!!] ");
        s += prefix;
        s += fmt;
        s = tid()+s;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }

    inline void error (const char *fmt, ...) {
        va_list args;
        std::string s(" [!] ");
        s += prefix;
        s += fmt;
        s = tid()+s;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }

    inline void warn (const char *fmt, ...) {
        va_list args;
        std::string s(" [?] ");
        s += prefix;
        s += fmt;
        s = tid()+s;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }

    inline void info (const char *fmt, ...) {
        va_list args;
        std::string s("     ");
        s += prefix;
        s += fmt;
        s = tid()+s;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }

    inline void debug (const char *fmt, ...) {
        va_list args;
        std::string s("   . ");
        s += prefix;
        s += fmt;
        s = tid()+s;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }

    inline void trace (const char *fmt, ...) {
        va_list args;
        std::string s("  .. ");
        s += prefix;
        s += fmt;
        s = tid()+s;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }
};

#endif

