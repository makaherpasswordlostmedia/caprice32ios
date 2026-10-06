// Symbian: no POSIX signals/sigaltstack in OpenC. Same API as the original crashlog.h, but checkpoints
// are only kept in memory (last one retrievable via crashlog::last()); nothing is written to disk.
#ifndef CRASHLOG_H
#define CRASHLOG_H
#include <cstring>
#include <cstdio>
namespace crashlog {
namespace detail { inline char g_checkpoint[256] = "no checkpoint reached yet"; inline unsigned int g_last_pc = 0xFFFFFFFFu; }
inline void init() {}
inline void checkpoint(const char* msg) { strncpy(detail::g_checkpoint, msg, sizeof detail::g_checkpoint - 1); }
inline void checkpoint_fast(const char* msg) { checkpoint(msg); }
inline const char* path() { return ""; }
inline const char* last() { return detail::g_checkpoint; }
inline void register_thread_altstack() {}
inline void set_last_pc(unsigned int pc) { detail::g_last_pc = pc; }
}
#define CRASH_LOG_STRINGIFY_(x) #x
#define CRASH_LOG_STRINGIFY(x) CRASH_LOG_STRINGIFY_(x)
#define CRASH_CHECKPOINT(msg) ((void)0)
#define CRASH_CHECKPOINT_FAST(msg) ((void)0)
#define CRASHLOG_FAST(msg) ((void)0)
#define CRASHLOG_SET_PC(pc) ((void)0)
#endif
