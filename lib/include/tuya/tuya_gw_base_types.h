#ifndef TUYA_GW_BASE_TYPES_H
#define TUYA_GW_BASE_TYPES_H

#include <stdint.h>

#include "tuya_os_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAC_MAX_LEN     6
#define ADDR_MAX_LEN    16

typedef NW_IP_S   ip_info_s;
typedef NW_MAC_S  mac_info_s;

#ifdef __cplusplus
}
#endif
#endif