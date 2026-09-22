#include "common.h"
#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ptrace.h>

void DumpHex (const char *szTag, uint8_t *b, int count) {
    if (count <= 0) {
        return;
    }
    printf ("%s(len:%d): \n", szTag, count);
    int i = 0;
    for (i = 0; i < count; i++) {
        printf ("%02x ", b[i]);
        if ((i + 1) % 16 == 0) {
            printf ("\n");
        }
    }
    printf ("\n");
}

struct timespec GetClock (void) {
    struct timespec tp;
    clock_gettime (CLOCK_MONOTONIC, &tp);

    return tp;
}

uint64_t GetElapsed (struct timespec *tpstart) {
    struct timespec tpend;
    clock_gettime (CLOCK_MONOTONIC, &tpend);
    uint64_t timedif = (tpend.tv_sec - tpstart->tv_sec) * 1000 * 1000 * 1000 + (tpend.tv_nsec - tpstart->tv_nsec);

    return timedif;
}

int64_t TimespecDiffMs(const struct timespec *start, const struct timespec *end) {
    int64_t sec  = end->tv_sec  - start->tv_sec;
    int64_t nsec = end->tv_nsec - start->tv_nsec;

    if (nsec < 0) {
        sec  -= 1;
        nsec += 1000000000LL;
    }

    return sec * 1000LL + nsec / 1000000LL;
}

void timer_start(struct timeval *timer) {
    (void) gettimeofday(timer, NULL);
}

double timer_end(const struct timeval *start) {
    struct timeval now;
    double elapsed;

    (void) gettimeofday(&now, NULL);
    elapsed = (now.tv_sec - start->tv_sec) + ((double) now.tv_usec - start->tv_usec) / 1000000;
    /* Return a minimum duration of 1 microsecond. */
    if (elapsed <= 0.0)
        elapsed = 0.000001;
    
    return (elapsed);
}

int DelayMs(uint64_t millis) {
    int rc;
    struct timespec t, rem;

    rem.tv_sec = (time_t) millis / 1000;
    rem.tv_nsec = (long) (millis % 1000) * 1000000;

    do {
        t = rem;
        rc = nanosleep(&t, &rem);
    } while (rc != 0 && errno == EINTR);

    return rc;
}

int DetectLdPreload(void) {
    const char *ld_preload = getenv("LD_PRELOAD");
    if (ld_preload) {
        fprintf(stderr,"LD_PRELOAD detected: %s\n", ld_preload);
        return -1;
    }

    return 0;
}

int AntiDebugCheck(void) {
    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1 && errno != 0) {
        fprintf(stderr, "Process is being traced! %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (DetectLdPreload()) {
        unsetenv("LD_PRELOAD");
        printf("LD_PRELOAD environment variable unset.\n");
    }

    return 0;
}



#if 0
int IsFuncInLibc(void *fn) {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) return -1;

    char line[512];
    uintptr_t addr = (uintptr_t)fn;
    int ret = -1;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "libc") && strstr(line, ".so")) {
            // 解析地址区间
            uintptr_t start, end;
            if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                if (addr >= start && addr < end) {
                    ret = 0;
                    break;
                }
            }
        }
    }

    fclose(fp);
    return ret;
}

const char *funcsLists[] = {"malloc", "calloc", "free", "open", "read", "write", "close", "fopen", "fclose", "fread", "fwrite", "system", "execve", NULL};

int DetectHookedFuns(void) {
    int ret = 0;

    for (const char **func = funcsLists; *func != NULL; func++) {
        void *addr = dlsym(RTLD_NEXT, *func);
        if (!addr) {
            printf("[!] %s not found in libc\n", *func);
            ret++;
        } else if (IsFuncInLibc(addr) != 0) {
            printf("[!] %s possibly hooked! address: %p\n", *func, addr);
            ret++;
        } else {
            printf("[+] %s is in libc: %p\n", *func, addr);
        }
    }

    return ret;
}
#endif


