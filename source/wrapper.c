#include "common.h"

#if ENABLE_MIMALLOC
#include "mimalloc.h"
EXPORT void *__wrap_malloc (size_t size) {
    return mi_malloc (size);
}

EXPORT void *__wrap_calloc (size_t num, size_t size) {
    return mi_calloc (num, size);
}

EXPORT void *__wrap_realloc (void *ptr, size_t new_size) {
    return mi_realloc (ptr, new_size);
}

EXPORT void __wrap_free (void *ptr) {
    mi_free (ptr);
}
#endif

#if ENABLE_MTRACE
#include "mcheck.h"
inline void enable_mtrace (void) {
    const char *trace_file = getenv ("MALLOC_TRACE");
    if (!trace_file) {
        fprintf (stderr, "[MTRACE] Warning: MALLOC_TRACE not set\n");
    }
    mtrace ();
}

inline void disable_mtrace (void) {
    muntrace ();
}
#else
inline void enable_mtrace (void) {}
inline void disable_mtrace (void) {}
#endif
