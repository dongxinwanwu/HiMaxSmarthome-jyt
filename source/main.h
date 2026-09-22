#ifndef __MAIN_H__
#define __MAIN_H__

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fcntl.h"
#include "sys/ioctl.h"
#include "sys/stat.h"
#include "sys/types.h"
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>

#include "common.h"
#include "list.h"
#include "thpool.h"
#include "zlog.h"

extern list_t *pZ3List;
extern list_t *pMbusDevList;
extern list_t *pRf433List;
extern list_t *pKnxDevList;
extern uint8_t g_Mac[13];

extern zlog_category_t *pLogCat;

int KnxDevIdMatch(void *pNode, void *pDevId);
int KnxPhyMatch (void *pNode, void *pPhy);

#endif
