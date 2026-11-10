// km-prof.c — SIGPROF sampler for profiling kmc where perf/ptrace are blocked.
// Linked into a profiling copy of kmc by tools/km-prof (never the product build).
// Each sample records: rip, [rsp], then the rbp chain's return addresses
// (compiler frames are `push rbp; mov rbp,rsp`).  Appended to $KM_PROF_OUT.
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>
#include <fcntl.h>

#define MAXD 48
static int fd = -1;
static void on_prof(int sig, siginfo_t *si, void *uc_) {
    (void)sig; (void)si;
    ucontext_t *uc = uc_;
    unsigned long buf[MAXD + 3]; int n = 0;
    unsigned long rip = uc->uc_mcontext.gregs[REG_RIP];
    unsigned long rsp = uc->uc_mcontext.gregs[REG_RSP];
    unsigned long *bp = (unsigned long *)uc->uc_mcontext.gregs[REG_RBP];
    buf[n++] = rip;
    buf[n++] = *(unsigned long *)rsp;
    for (int i = 0; i < MAXD && bp && ((unsigned long)bp & 7) == 0
         && (unsigned long)bp > rsp && (unsigned long)bp - rsp < (1ul << 28); i++) {
        buf[n++] = bp[1];
        unsigned long *nx = (unsigned long *)bp[0];
        if (nx <= bp) break;
        bp = nx;
    }
    unsigned long hdr = n;
    if (write(fd, &hdr, 8) < 0) return;
    if (write(fd, buf, n * 8) < 0) return;
}
__attribute__((constructor)) static void km_prof_init(void) {
    const char *p = getenv("KM_PROF_OUT");
    if (!p) return;
    fd = open(p, O_WRONLY | O_CREAT | O_APPEND, 0644);
    struct sigaction sa; memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_prof; sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &sa, 0);
    struct itimerval it = {{0, 1000}, {0, 1000}};
    setitimer(ITIMER_PROF, &it, 0);
}
