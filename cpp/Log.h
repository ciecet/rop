#ifndef LOG_H
#define LOG_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#ifndef ROP_DEBUG
#define ROP_DEBUG 0
#endif

namespace acba {

struct Log {

    const char *prefix;

    Log (const char *p): prefix(p) {
        //trace("BEGIN\n");
    }
    ~Log () {
        //trace("END\n");
    }

    inline std::string tid () {
        char s[10];
        sprintf(s, "%08x ", (unsigned int)getpid());
        return s+5;
    }

#if ROP_DEBUG
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
        std::string s(" ### ");
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
        std::string s("     ");
        s += prefix;
        s += fmt;
        s = tid()+s;
        va_start(args, fmt);
        vprintf(s.c_str(), args);
        va_end(args);
    }
#else
    inline void fatal (const char *fmt, ...) {}
    inline void error (const char *fmt, ...) {}
    inline void warn (const char *fmt, ...) {}
    inline void info (const char *fmt, ...) {}
    inline void debug (const char *fmt, ...) {}
    inline void trace (const char *fmt, ...) {}
#endif
};

}

#endif

