/*
 * FileName: common.h
 * Author:    wjian
 * Date:      2022-09-07
 * Version:   v1.0
 * Description: common file
 * */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "zlog.h"

/*
 * C99： 199901L, C11： 201112L, C17： 201710L, C2X： 202000L.
 * */
#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
#error "C version must be greater than C11"
#endif

/*
 * version: major version * 10000 + minor version * 100 + patchlevel.
 * GNUC: major version.
 * GNUC_MINOR: minor version.
 * GNUC_PATCHLEVEL: patchlevel.
 * */
#if defined __GNUC__
#if defined __GNUC__ && (__GNUC__ <= 4) && (__GNUC_MINOR__ < 9)
#error "GCC version must be greater than " __GNUC__ "." __GNUC_MINOR__ "." __GNUC_PATCHLEVEL__
#endif
#elif defined __clang__
#if defined clang_major && (clang_major <= 3)
#define __VERSION__ (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#error "Clang version must be greater than 3.0.0"
#endif
#endif

#define COLOR_BLACK "\e[1;30m"
#define COLOR_RED "\e[1;31m"
#define COLOR_GREEN "\e[1;32m"
#define COLOR_YELLOW "\e[1;33m"
#define COLOR_BLUE "\e[1;34m"
#define COLOR_PURPLE "\e[1;35m"
#define COLOR_DARK_GREEN "\e[1;36m"

#define BGCOLOR_BLACK "\e[1;40m"
#define BGCOLOR_RED "\e[1;41m"
#define BGCOLOR_GREEN "\e[1;42m"
#define BGCOLOR_YELLOW "\e[1;43m"
#define BGCOLOR_BLUE "\e[1;44m"
#define BGCOLOR_PURPLE "\e[1;45m"
#define BGCOLOR_DARK_GREEN "\e[1;46m"

#define COLOR_CLEAR "\e[0m"
#define COLOR_BOLD "\e[1m"
#define COLOR_LINE "\e[4m"
#define COLOR_BLINK "\e[5m"
#define COLOR_REVERSE "\e[7m"

// #if defined(__GNUC__) || defined(__clang__)
#ifndef NO_RETURN
#define NO_RETURN __attribute__ ((__noreturn__))
#endif
#ifndef USED
#define USED __attribute__ ((used))
#endif
#ifndef UNUSED
#define UNUSED __attribute__ ((unused))
#endif
#ifndef WEAK
#define WEAK __attribute__ ((weak))
#endif
#ifndef PACKED
#define PACKED __attribute__ ((packed, aligned (1)))
#endif
#ifndef ALIGNED
#define ALIGNED(x) __attribute__ ((aligned (x)))
#endif
#ifndef RESTRICT
#define RESTRICT __restrict
#endif
#ifndef PURE
#define PURE __attribute__ ((pure))
#endif
#ifndef INLINE
#define INLINE __attribute__ ((always_inline))
#endif
#ifndef CTOR
#define CTOR __attribute__ ((constructor))
#endif
#ifndef DTOR
#define DTOR __attribute__ ((destructor))
#endif
#ifndef SECTION
#define SECTION(level) __attribute__ ((used, aligned (sizeof (void *)), __section__ (".init_" #level)))
#endif
#ifndef CLEANUP
#define CLEANUP(fn) __attribute__ ((cleanup (fn)))
#endif
#ifndef ALIAS
#define ALIAS(name) __attribute__ ((alias (#name)))
#endif
#ifndef FLATTEN
#define FLATTEN __attribute__ ((flatten))
#endif
#ifndef HIDDEN
// gcc -fPIC -shared -fvisibility=hidden -o libmylib.so mylib.c
#define HIDDEN __attribute__ ((visibility ("hidden")))
#endif
#ifndef EXPORT
#define EXPORT __attribute__((visibility("default")))
#endif

#define AUTO_LOCK(lock)                         \
    void _release_lock_ (void *_ptr) {          \
        pthread_mutex_unlock (*(void **) _ptr); \
    }                                           \
    CLEANUP (_release_lock_)                    \
    pthread_mutex_t *_pMutex = lock;            \
    pthread_mutex_lock (_pMutex);

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#endif

#ifndef ENUM_TYPE_CASE
#define ENUM_TYPE_CASE(x) \
    case x: return (#x);
#endif

//
//#define offsetof(type, member) ((size_t) &((type *)0)->member)
//
#define container_of(ptr, type, member) ({          \
        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) ); })

extern zlog_category_t *pLogCat;
#define logHex(tag, buf, cnt)                                                                                                           \
    do {                                                                                                                                     \
        char *pLogBuf = (char *) calloc (1, cnt * 3 + 1);                                                                                    \
        if (!pLogBuf) break;                                                                                                                 \
        for (int k = 0; k < cnt; k++) {                                                                                                      \
            sprintf (pLogBuf + k * 3, "%02x ", buf[k]);                                                                                      \
        }                                                                                                                                    \
        pLogBuf[cnt * 3] = '\0';                                                                                                             \
        zlog (pLogCat, __FILE__, sizeof (__FILE__) - 1, __func__, sizeof (__func__) - 1, __LINE__, ZLOG_LEVEL_DEBUG, "%s --> %s", tag, pLogBuf); \
        free (pLogBuf);                                                                                                                      \
    } while (0)
#define logTrc(...) zlog(pLogCat, __FILE__, sizeof(__FILE__) - 1, __func__, sizeof(__func__) - 1, __LINE__, 10, __VA_ARGS__)
#define logDbg(...) zlog_debug(pLogCat, __VA_ARGS__)
#define logInfo(...) zlog_info(pLogCat, __VA_ARGS__)
#define logErr(...) zlog_error(pLogCat, __VA_ARGS__)

typedef struct {
    const char *pName;
    void *pFunc;
} Callback_t;

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint16_t patch;
    char desc[25];
} SoftWareVersion_t;

typedef struct {
    int key;
    uint8_t *pBuf;
    uint32_t len;
    uint32_t timestamp;
} PACKED Msg_t;

void DumpHex (const char *szTag, uint8_t *b, int count);
struct timespec GetClock (void);
uint64_t GetElapsed (struct timespec *tpstart);
int64_t TimespecDiffMs(const struct timespec *start, const struct timespec *end);
int DelayMs (uint64_t millis);
int AntiDebugCheck (void);

#endif
