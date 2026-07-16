// =============================================================================
// Apex Stick Trainer V1.0 — Entry Point
// =============================================================================
// Must define SDL_MAIN_HANDLED BEFORE any SDL include to prevent MinGW linking
// conflicts with SDL2main. Use main() instead of SDL_main().
#define SDL_MAIN_HANDLED

#include "App.hpp"
#include <cstdio>
#include <csignal>
#include <cstdlib>

#ifdef __MINGW32__
#include <windows.h>
#endif

// Crash diagnostics — writes a line to crash_diag.txt (flushed each call).
// Used to bracket init/loop steps so a crash leaves a trail showing the last
// completed step. Also updates g_lastStep so the SEH/signal handlers can report
// where we were when a crash hit.
const char* g_lastStep = "(none)";

void setCrashStep(const char* s) { g_lastStep = s; }

void diag(const char* msg) {
    g_lastStep = msg;
    static FILE* f = std::fopen("crash_diag.txt", "w");
    if (f) { std::fprintf(f, "%s\n", msg); std::fflush(f); }
}

void diagf(const char* fmt, ...) {
    static FILE* f = std::fopen("crash_diag.txt", "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);
    std::fputc('\n', f);
    std::fflush(f);
}

// -----------------------------------------------------------------------------
// Crash capture: C++ try/catch does NOT catch access violations (SEH) on
// Windows. These handlers catch SIGSEGV / illegal instruction / abort and the
// Win32 unhandled-exception filter, logging the fault so we can localize the
// heisenbug that disappears when logging is added.
// -----------------------------------------------------------------------------
static void sigHandler(int sig) {
    const char* name = "unknown";
    switch (sig) {
        case SIGSEGV: name = "SIGSEGV (segfault / access violation)"; break;
        case SIGABRT: name = "SIGABRT (abort)"; break;
        case SIGFPE:  name = "SIGFPE (float exception)"; break;
        case SIGILL:  name = "SIGILL (illegal instruction)"; break;
    }
    diagf("[CRASH] signal %d (%s) after step: %s", sig, name, g_lastStep);
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

#ifdef __MINGW32__
static LONG WINAPI unhandledExFilter(EXCEPTION_POINTERS* ep) {
    DWORD code = ep ? ep->ExceptionRecord->ExceptionCode : 0;
    const char* kind = "EXCEPTION";
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:    kind = "ACCESS_VIOLATION"; break;
        case EXCEPTION_STACK_OVERFLOW:      kind = "STACK_OVERFLOW"; break;
        case EXCEPTION_ILLEGAL_INSTRUCTION: kind = "ILLEGAL_INSTRUCTION"; break;
        case EXCEPTION_BREAKPOINT:          kind = "BREAKPOINT"; break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:  kind = "FLT_DIV_BY_ZERO"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:  kind = "INT_DIV_BY_ZERO"; break;
    }
    ULONG_PTR info0 = (ep && ep->ExceptionRecord->NumberParameters >= 1)
                      ? ep->ExceptionRecord->ExceptionInformation[0] : 0;
    ULONG_PTR info1 = (ep && ep->ExceptionRecord->NumberParameters >= 2)
                      ? ep->ExceptionRecord->ExceptionInformation[1] : 0;
    void* addr = ep ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    diagf("[CRASH] SEH 0x%08lX (%s) at %p info=(%lu,%lu) after step: %s",
          (unsigned long)code, kind, addr, (unsigned long)info0,
          (unsigned long)info1, g_lastStep);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

// Macro to stamp the "current step" so crash handlers can report it.
#define STEP(S) do { g_lastStep = (S); diag(S); } while (0)

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::signal(SIGSEGV, sigHandler);
    std::signal(SIGABRT, sigHandler);
    std::signal(SIGFPE,  sigHandler);
    std::signal(SIGILL,  sigHandler);
#ifdef __MINGW32__
    SetUnhandledExceptionFilter(unhandledExFilter);
#endif

    STEP("[main] start");

    try {
        STEP("[main] constructing App");
        App app;
        STEP("[main] App constructed, entering run()");
        app.run();
        STEP("[main] run() returned cleanly");
    } catch (const std::exception& e) {
        diagf("[FATAL] std::exception: %s (after step: %s)", e.what(), g_lastStep);
        fprintf(stderr, "[FATAL] %s\n", e.what());
        return 1;
    } catch (...) {
        diagf("[FATAL] unknown exception (after step: %s)", g_lastStep);
        return 1;
    }
    STEP("[main] exit 0");
    return 0;
}
