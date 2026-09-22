/**
 * @file    tuya_gw_smconfig_api.h
 * @brief   This file contains send broadcast packet interface
 * @version 0.1
 * 
 * @copyright Copyright 2021 Tuya Inc. www.tuya.com
 */
#ifndef TUYA_GW_SMCONFIG_API_H
#define TUYA_GW_SMCONFIG_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*SM_INFO_CB)(char *ssid, uint32_t ssid_size, char *key, uint32_t key_size);

/**
 * @brief    initiate smart config.
 *
 * @param ifname. WiFi interface name.
 * @param cb.     get ssid and passwd callback.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
int tuya_user_iot_smconfig_init(const char *ifname, const SM_INFO_CB cb);

/**
 * @brief    start smart config.
 *
 * @param timeout. timeout to stop smart config.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
int tuya_user_iot_smconfig_start(uint32_t timeout);

/**
 * @brief    stop smart config.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
int tuya_user_iot_smconfig_stop(void);

#ifdef __cplusplus
}
#endif

#endif  /* TUYA_GW_SMCONFIG_API_H */