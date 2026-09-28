#ifndef LOG_H
#define LOG_H

#include <iostream>

extern bool log_verbose;

// std::endl flushes the stream on every call, which is a syscall per log
// line. '\n' keeps output buffered; errors still get out on exit/flush.
#define LOG_TO(stream,level,message) stream << (level) << " " << __FILE__ << ":" << __LINE__ << " - " << message << '\n'; // NOLINT(misc-macro-parentheses): Not having parentheses around message is a feature, it allows using streams in LOG macros

#ifdef CAPRICE_NO_LOG
// Compile all logging out entirely: no formatting, no I/O, no code.
#define LOG_ERROR(message) ((void)0);
#define LOG_WARNING(message) ((void)0);
#define LOG_INFO(message) ((void)0);
#define LOG_VERBOSE(message) ((void)0);
#else
#define LOG_ERROR(message) LOG_TO(std::cerr, "ERROR  ", message)
#define LOG_WARNING(message) LOG_TO(std::cerr, "WARNING", message)
#define LOG_INFO(message) LOG_TO(std::cerr, "INFO   ", message)
#define LOG_VERBOSE(message) if(log_verbose) { LOG_TO(std::cout, "VERBOSE", message) }
#endif

#if defined(DEBUG) && !defined(CAPRICE_NO_LOG)
#define LOG_DEBUG(message) if(log_verbose) { LOG_TO(std::cout, "DEBUG  ", message) }
#else
#define LOG_DEBUG(message)
#endif

#endif
