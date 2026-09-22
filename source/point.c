#include "point.h"
#include "misc_dev_plugins.h"
#include "uthash.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static kv_item_t *kv_table = NULL;
static pthread_rwlock_t kv_lock = PTHREAD_RWLOCK_INITIALIZER;

/* 内部工具函数：获取当前时间戳 (ms) */
static uint64_t now_ms (void) {
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* 插入或更新 */
int kv_set (const char *grp, KnxDataType_t *pValue, uint32_t expire_ms) {
    pthread_rwlock_wrlock (&kv_lock);

    kv_item_t *item = NULL;
    HASH_FIND_STR (kv_table, grp, item);
    if (!item) {
        item = (kv_item_t *) calloc (1, sizeof (kv_item_t));
        if (!item) {
            pthread_rwlock_unlock (&kv_lock);
            return -1;
        }
        item->pGrp = strdup (grp);
        item->pValue = (KnxDataType_t *) calloc (1, sizeof (KnxDataType_t));
        HASH_ADD_KEYPTR (hh, kv_table, item->pGrp, strlen (item->pGrp), item);
        logDbg (COLOR_RED "knx add group %s" COLOR_CLEAR, grp);
    }

    clock_gettime (CLOCK_MONOTONIC, &item->timestamp);
    item->expire = expire_ms;
    pthread_rwlock_unlock (&kv_lock);

    return 0;
}

int kv_update (const char *grp, KnxDataType_t *pValue) {
    kv_item_t *item = NULL;
    int ret = -1;

    pthread_rwlock_wrlock (&kv_lock);
    HASH_FIND_STR (kv_table, grp, item);
    if (item) {
        memcpy (item->pValue, pValue, sizeof (KnxDataType_t));
        clock_gettime (CLOCK_MONOTONIC, &item->timestamp);
        item->bDone = false;
        ret = 0;
        // logDbg (COLOR_RED "[knx bounce update] group:%s, value:%d " COLOR_CLEAR, grp, item->pValue->value);
    }
    pthread_rwlock_unlock (&kv_lock);

    return ret;
}

/* 查找，自动检查是否过期 */
KnxDataType_t *kv_get (const char *grp) {
    pthread_rwlock_rdlock (&kv_lock);

    kv_item_t *item = NULL;
    HASH_FIND_STR (kv_table, grp, item);
    if (!item) {
        pthread_rwlock_unlock (&kv_lock);
        return NULL;
    }

    uint64_t now = now_ms ();
    uint64_t ts = (uint64_t) item->timestamp.tv_sec * 1000 + item->timestamp.tv_nsec / 1000000;
    if (item->expire > 0 && now > ts + item->expire) {
        KnxDataType_t *pData = (KnxDataType_t *) calloc (1, sizeof (KnxDataType_t));
        memcpy (pData, item->pValue, sizeof (KnxDataType_t));
        pthread_rwlock_unlock (&kv_lock);
        return pData;
    }

    pthread_rwlock_unlock (&kv_lock);

    return NULL;
}

void KNX_DevDebounce (void *param) {
    if (!param) {
        logErr ("KNX_DevMonitorEntry invalid param");
        return;
    }
    queue_t *pQueue = (queue_t *) param;
    KnxDataType_t *pData = NULL;
    DelayMs (2000);
    while (true) {
        DelayMs (100);
        pthread_rwlock_rdlock (&kv_lock);
        kv_item_t *cur, *tmp;
        uint64_t now = now_ms ();
        HASH_ITER (hh, kv_table, cur, tmp) {
            uint64_t ts = (uint64_t) cur->timestamp.tv_sec * 1000 + cur->timestamp.tv_nsec / 1000000;
            if (!cur->bDone && now > ts + cur->expire) {
                cur->bDone = true;
                if (strlen (cur->pValue->groupAddr) > 0) {
                    logDbg (COLOR_YELLOW "[knx debounce]knx->groupAddr:%s, knx->value:%d" COLOR_CLEAR, cur->pValue->groupAddr, cur->pValue->value);
                    KnxDataType_t *pData = (KnxDataType_t *) calloc (1, sizeof (KnxDataType_t));
                    memcpy (pData, cur->pValue, sizeof (KnxDataType_t));
                    queue_put (pQueue, (void *) pData);
                }
            }
        }
        pthread_rwlock_unlock (&kv_lock);
    }
}

/* 查找，不检查过期，不拷贝，直接返回指针 */
bool kv_find (const char *grp) {
    pthread_rwlock_rdlock (&kv_lock);
    kv_item_t *item = NULL;
    HASH_FIND_STR (kv_table, grp, item);
    if (!item) {
        pthread_rwlock_unlock (&kv_lock);
        // logDbg (COLOR_RED"knx group %s not found"COLOR_CLEAR, grp);
        return false;
    }
    pthread_rwlock_unlock (&kv_lock);
    // logDbg (COLOR_RED"knx group %s be found"COLOR_CLEAR, grp);

    return true;
}

/* 删除 */
int kv_del (const char *grp) {
    pthread_rwlock_wrlock (&kv_lock);

    kv_item_t *item = NULL;
    HASH_FIND_STR (kv_table, grp, item);
    if (item) {
        HASH_DEL (kv_table, item);
        free (item->pGrp);
        free (item);
    }

    pthread_rwlock_unlock (&kv_lock);
    return item ? 0 : -1;
}

/* 清理全部 */
void kv_clear (void) {
    pthread_rwlock_wrlock (&kv_lock);

    kv_item_t *cur, *tmp;
    HASH_ITER (hh, kv_table, cur, tmp) {
        HASH_DEL (kv_table, cur);
        free (cur->pGrp);
        free (cur);
    }

    pthread_rwlock_unlock (&kv_lock);
}
