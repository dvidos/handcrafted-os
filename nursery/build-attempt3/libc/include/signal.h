#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h> // For pid_t
#include <stddef.h>    // For NULL

typedef int sig_atomic_t; // Type for signal handler-safe variables

// Function pointer types for signal handlers
typedef void (*__sighandler_t)(int);
#define SIG_DFL ((__sighandler_t)0)   // Default signal handling
#define SIG_IGN ((__sighandler_t)1)   // Ignore signal
#define SIG_ERR ((__sighandler_t)-1)  // Error return from signal

// --- Standard Signals ---
#define SIGHUP     1   // Hangup detected on controlling terminal or death of controlling process
#define SIGINT     2   // Interrupt from keyboard
#define SIGQUIT    3   // Quit from keyboard
#define SIGILL     4   // Illegal Instruction
#define SIGTRAP    5   // Trace/breakpoint trap
#define SIGABRT    6   // Abort signal from abort()
#define SIGBUS     7   // Bus error (bad memory access)
#define SIGFPE     8   // Floating point exception
#define SIGKILL    9   // Kill signal
#define SIGUSR1   10   // User-defined signal 1
#define SIGSEGV   11   // Invalid memory reference
#define SIGUSR2   12   // User-defined signal 2
#define SIGPIPE   13   // Broken pipe: write to pipe with no readers
#define SIGALRM   14   // Timer signal from alarm()
#define SIGTERM   15   // Termination signal
#define SIGCHLD   17   // Child stopped or terminated
#define SIGCONT   18   // Continue if stopped
#define SIGSTOP   19   // Stop process
#define SIGTSTP   20   // Stop typed at terminal
#define SIGTTIN   21   // Terminal input for background process
#define SIGTTOU   22   // Terminal output for background process
#define SIGWINCH  28   // Window size change

// --- Signal set operations (minimal definitions) ---
typedef struct {
    unsigned long __bits[1]; // One bit per signal, adjust size as needed
} sigset_t;

int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set); // Not explicitly found, but useful
int sigaddset(sigset_t *set, int signum);
int sigdelset(sigset_t *set, int signum); // Not explicitly found, but useful
int sigismember(const sigset_t *set, int signum);

// --- sigaction structure ---
struct sigaction {
    __sighandler_t sa_handler; // Pointer to signal handling function
    sigset_t sa_mask;          // Signals to block in handler
    int sa_flags;              // Special flags and options
    // void (*sa_restorer)(void); // System-specific, not always exposed
};

// sa_flags
#define SA_NOCLDSTOP 0x00000001 // Do not generate SIGCHLD when child stops
#define SA_RESETHAND 0x80000000 // Reset handler to SIG_DFL on entry
#define SA_RESTART   0x10000000 // Restart functions if interrupted by handler

// --- Function Prototypes (based on usage analysis) ---
__sighandler_t signal(int signum, __sighandler_t handler);
int raise(int signum);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int kill(pid_t pid, int sig); // Used by bmake

// --- Additional functions found in usage, typically from unistd.h or sys/signal.h ---
// int sigprocmask(int how, const sigset_t *set, sigset_t *oldset); // Used in bmake's sigaction.c but not explicitly grepped for.
// int sigsuspend(const sigset_t *mask); // Used in bmake's sigaction.c but not explicitly grepped for.

#endif // _SIGNAL_H