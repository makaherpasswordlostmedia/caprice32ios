#ifndef CRASHLOG_H
#define CRASHLOG_H

// Lightweight crash checkpoint logger.
//
// Problem this solves: stderr from a sandboxed iOS app does not reliably
// end up anywhere you can read after the process has already crashed, and
// the standard crash report only gives you a raw stripped stack (offsets
// into the binary, no symbols, no source line). That leaves you guessing
// which of dozens of startup steps was actually running when a SIGBUS/
// SIGSEGV hit.
//
// The fix: keep a small on-disk "last known checkpoint" that gets
// overwritten (synchronously, flushed to disk) right before every
// meaningfully risky step. If the process dies, a signal handler installed
// at startup catches the fatal signal, and appends the signal name, the
// faulting address (crucial for EXC_ARM_DA_ALIGN - it tells you whether the
// bad address is even suspicious, e.g. not a multiple of 4/8), and the last
// checkpoint string to that same file, then re-raises the signal so the
// normal OS crash reporter still fires as before.
//
// Everything the signal handler itself does is async-signal-safe: no
// malloc, no fprintf/iostream, no locks. Only write(2), and formatting via
// a small hand-rolled integer-to-hex routine.
//
// Usage:
//   crashlog::init();                     // call once, very early in main
//   CRASH_CHECKPOINT("before video_init"); // sprinkle before risky steps
//
// After a crash, read the file at crashlog::path() (HOME/crashlog.txt) -
// e.g. by adding a temporary "share log" button, or via iTunes File Sharing
// / the Files app if the app exposes its Documents directory. It will
// contain the last checkpoint reached and full signal/fault details, which
// combined with the existing crash report's stack (same binary offsets)
// tells you exactly which C++ statement was executing.

#include <atomic>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

namespace crashlog {

namespace detail {

// Fixed-size buffer for the "last checkpoint" message. Updated from the
// main thread only (startup is single-threaded), so a plain char array is
// fine - no locking needed, and nothing here allocates.
inline char g_checkpoint[256] = "no checkpoint reached yet";

inline char g_log_path[512] = {0};

// Async-signal-safe unsigned-to-hex. writes into buf, returns length.
inline int u32_to_hex(unsigned int v, char *buf) {
   static const char *digits = "0123456789abcdef";
   char tmp[8];
   int n = 0;
   if (v == 0) {
      buf[0] = '0';
      return 1;
   }
   while (v && n < 8) {
      tmp[n++] = digits[v & 0xF];
      v >>= 4;
   }
   for (int i = 0; i < n; i++) {
      buf[i] = tmp[n - 1 - i];
   }
   return n;
}

inline void safe_write(const char *s) {
   if (g_log_path[0] == '\0') return;
   int fd = open(g_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
   if (fd < 0) return;
   size_t len = 0;
   while (s[len]) len++;
   write(fd, s, len);
   close(fd);
}

inline void signal_handler(int sig, siginfo_t *info, void *ucontext) {
   (void)ucontext;
   char buf[768];
   size_t p = 0;
   const char *sig_name =
      sig == SIGBUS  ? "SIGBUS"  :
      sig == SIGSEGV ? "SIGSEGV" :
      sig == SIGILL  ? "SIGILL"  :
      sig == SIGABRT ? "SIGABRT" :
      sig == SIGFPE  ? "SIGFPE"  : "SIGNAL";

   const char *hdr = "\n=== CRASH ===\nsignal: ";
   while (*hdr) buf[p++] = *hdr++;
   while (*sig_name) buf[p++] = *sig_name++;

   const char *addr_lbl = "\nfaulting address: 0x";
   while (*addr_lbl) buf[p++] = *addr_lbl++;
   unsigned int addr = (unsigned int)(uintptr_t)(info ? info->si_addr : nullptr);
   char hexbuf[8];
   int hexlen = u32_to_hex(addr, hexbuf);
   for (int i = 0; i < hexlen; i++) buf[p++] = hexbuf[i];

   // For alignment faults this is the single most useful extra fact:
   // is the faulting address even misaligned, and by how much mod 8/4?
   const char *mod_lbl = "\naddr mod 8 = ";
   while (*mod_lbl) buf[p++] = *mod_lbl++;
   buf[p++] = '0' + (char)(addr % 8);
   const char *mod4_lbl = ", mod 4 = ";
   while (*mod4_lbl) buf[p++] = *mod4_lbl++;
   buf[p++] = '0' + (char)(addr % 4);

   const char *cp_lbl = "\nlast checkpoint: ";
   while (*cp_lbl) buf[p++] = *cp_lbl++;
   const char *cp = g_checkpoint;
   while (*cp && p < sizeof(buf) - 8) buf[p++] = *cp++;
   buf[p++] = '\n';
   buf[p] = '\0';

   safe_write(buf);

   // Restore default handler and re-raise so the OS crash reporter still
   // produces its normal report on top of what we just logged.
   signal(sig, SIG_DFL);
   raise(sig);
}

} // namespace detail

// Call once, as early as possible in main()/cap32_main().
inline void init() {
   const char *home = getenv("HOME");
   if (home) {
      size_t i = 0;
      while (home[i] && i < sizeof(detail::g_log_path) - 32) {
         detail::g_log_path[i] = home[i];
         i++;
      }
      const char *suffix = "/crashlog.txt";
      size_t j = 0;
      while (suffix[j]) {
         detail::g_log_path[i + j] = suffix[j];
         j++;
      }
      detail::g_log_path[i + j] = '\0';
   }

   if (detail::g_log_path[0] != '\0') {
      // If the previous run left a crash entry, preserve it under a
      // stable filename before truncating the live log for this run.
      // Otherwise a second crash before anyone reads the file would
      // wipe out the first one's diagnostics.
      char lastcrash_path[544];
      size_t hlen = 0;
      if (home) { while (home[hlen] && hlen < sizeof(lastcrash_path) - 32) { lastcrash_path[hlen] = home[hlen]; hlen++; } }
      const char *lc_suffix = "/lastcrash.txt";
      size_t k = 0;
      while (lc_suffix[k]) { lastcrash_path[hlen + k] = lc_suffix[k]; k++; }
      lastcrash_path[hlen + k] = '\0';

      FILE *prev = fopen(detail::g_log_path, "rb");
      if (prev) {
         fseek(prev, 0, SEEK_END);
         long sz = ftell(prev);
         fseek(prev, 0, SEEK_SET);
         bool had_crash = false;
         if (sz > 0 && sz < (1 << 20)) {
            std::vector<char> contents(sz);
            if (fread(contents.data(), 1, sz, prev) == (size_t)sz) {
               // cheap substring search for the crash marker
               static const char marker[] = "=== CRASH ===";
               const size_t mlen = sizeof(marker) - 1;
               for (long a = 0; a + (long)mlen <= sz; a++) {
                  if (memcmp(contents.data() + a, marker, mlen) == 0) { had_crash = true; break; }
               }
               if (had_crash) {
                  FILE *out = fopen(lastcrash_path, "wb");
                  if (out) {
                     fwrite(contents.data(), 1, sz, out);
                     fclose(out);
                     fprintf(stderr, "crashlog: previous run crashed - saved to %s\n", lastcrash_path);
                     fflush(stderr);
                  }
               }
            }
         }
         fclose(prev);
      }
   }

   // Truncate any previous log at the start of a fresh run, so it's
   // unambiguous which run a crash entry belongs to.
   if (detail::g_log_path[0] != '\0') {
      int fd = open(detail::g_log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd >= 0) close(fd);
   }

   struct sigaction sa;
   memset(&sa, 0, sizeof(sa));
   sa.sa_sigaction = detail::signal_handler;
   sa.sa_flags = SA_SIGINFO;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGBUS,  &sa, nullptr);
   sigaction(SIGSEGV, &sa, nullptr);
   sigaction(SIGILL,  &sa, nullptr);
   sigaction(SIGABRT, &sa, nullptr);
   sigaction(SIGFPE,  &sa, nullptr);
}

// Update the last-known checkpoint and append it to the on-disk log
// immediately (not just kept in memory), so even a hard kill (not caught
// by any signal, e.g. a watchdog SIGKILL) still leaves a trail on disk.
inline void checkpoint(const char *msg) {
   size_t i = 0;
   while (msg[i] && i < sizeof(detail::g_checkpoint) - 1) {
      detail::g_checkpoint[i] = msg[i];
      i++;
   }
   detail::g_checkpoint[i] = '\0';

   char line[300];
   size_t p = 0;
   const char *pre = "checkpoint: ";
   while (*pre) line[p++] = *pre++;
   for (size_t k = 0; k < i; k++) line[p++] = msg[k];
   line[p++] = '\n';
   line[p] = '\0';
   detail::safe_write(line);
}

inline const char *path() {
   return detail::g_log_path;
}

} // namespace crashlog

#define CRASH_LOG_STRINGIFY_(x) #x
#define CRASH_LOG_STRINGIFY(x) CRASH_LOG_STRINGIFY_(x)
#define CRASH_CHECKPOINT(msg) ::crashlog::checkpoint(__FILE__ ":" CRASH_LOG_STRINGIFY(__LINE__) " - " msg)

#endif
