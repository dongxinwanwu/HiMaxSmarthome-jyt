/**
 * @file    tuya_gw_misc_api.h
 * @brief   This file contains subdevice management interface
 * @version 0.1
 * 
 * @copyright Copyright 2021 Tuya Inc. www.tuya.com
 */
#ifndef TUYA_GW_MISC_API_H
#define TUYA_GW_MISC_API_H

#include <stdint.h>
#include <stdbool.h>

#include "tuya_cloud_com_defs.h"
#include "tuya_gw_com_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef GRP_ACTION_E ty_group_action_t;
#define GROUP_ADD      GRP_ADD
#define GROUP_DEL      GRP_DEL

typedef SCE_ACTION_E ty_scene_action_t;
#define SCENE_ADD      SCE_ADD
#define SCENE_DEL      SCE_DEL
#define SCENE_EXEC     SCE_EXEC

typedef DEV_TYPE_T ty_dev_tp_t;
#define DEV_TP_ZIGBEE  DEV_ATTACH_MOD_1
#define DEV_TP_BLE     DEV_ATTACH_MOD_2
#define DEV_TP_SUBG    DEV_ATTACH_MOD_3
#define DEV_TP_ZWAVE   DEV_ATTACH_MOD_4
#define DEV_TP_433     DEV_ATTACH_MOD_5
#define DEV_TP_485     DEV_ATTACH_MOD_6
#define DEV_TP_KNX     DEV_ATTACH_MOD_7
#define DEV_TP_USR1    DEV_ATTACH_MOD_8
#define DEV_TP_USR2    DEV_ATTACH_MOD_9
#define DEV_TP_USR3    DEV_ATTACH_MOD_10

typedef struct {
    void (*misc_dev_add_cb)(bool permit, uint32_t timeout);
    void (*misc_dev_del_cb)(const char *dev_id);
    void (*misc_dev_bind_ifm_cb)(const char *dev_id, int result);
    void (*misc_dev_upgrade_cb)(const char *dev_id, const char *img);
    void (*misc_dev_reset_cb)(const char *dev_id);
    void (*misc_dev_heartbeat_cb)(const char *dev_id);
    void (*misc_dev_group_cb)(ty_group_action_t action, const char *dev_id, const char *grp_id);
    void (*misc_dev_scene_cb)(ty_scene_action_t action, const char *dev_id, const char *grp_id, const char *sce_id);
    void (*misc_dev_upgrade_v2_cb)(const char *dev_id, const char *img, const FW_UG_S *fw);
} ty_misc_dev_cbs_s;

/**
 * @brief    register device management callbacks.
 *
 * @param cbs.  a set of callbacks.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
int tuya_user_iot_reg_misc_dev_cb(const ty_misc_dev_cbs_s *cbs);

/**
 * @brief    bind device to gateway.
 *
 * @param tp.     firmware type.
 * @param uddd.   custom filed, the highest bit must be 1.
 * @param dev_id. device unique ID.
 * @param pid.    product ID.
 * @param ver.    device software version.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
int tuya_user_iot_misc_dev_bind(ty_dev_tp_t tp, uint32_t uddd, const char *dev_id, const char *pid, const char *ver);

/**
 * @brief    unbind device from gateway.
 *
 * @param dev_id. device unique ID.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
int tuya_user_iot_misc_dev_unbind(const char *dev_id);

/**
 * @brief    update device version.
 *
 * @param dev_id.  device unique ID.
 * @param version. device version.
 *
 * @retval   0: success
 * @retval  !0: failure
 */
int tuya_user_iot_misc_dev_ver_update(const char *dev_id, const char *version);

/**
 * @brief    fresh device heartbeat.
 *
 * @param dev_id.  device unique ID.
 *
 * @retval   0: sucess
 * @retval  !0: failure
 */
int tuya_user_iot_misc_dev_hb_fresh(const char *dev_id);

/**
 * @brief    configure device heartbeat.
 *
 * @param dev_id.    device unique ID.
 * @param timeout.   offline timeout, uint: seconds. 
 *                   timeout == 0xFFFFFFFF means always online.
 * @param max_retry. max retry to send heartbeat packet to subdevice when it is timeout.
 *                   lowpower == TRUE, this field is omitted.
 * @param lowpower.  TRUE: lowpower device, FALSE: not lowpower device.
 *
 * @retval   0: sucess
 * @retval  !0: failure
 */
int tuya_user_iot_misc_dev_hb_cfg(const char *dev_id, uint32_t timeout, uint32_t max_retry, int lowpower);

/**
 * @brief  get device online status.
 *
 * @param[in]  dev_id. device unique ID.
 * @param[out] online. TRUE: online, FALSE: offline.
 *
 * @retval  0: success
 * @retval !0: failure
 */
int tuya_user_iot_misc_dev_online_status(const char *dev_id, int *online);

/**
 * @brief    get device description.
 *
 * @param dev_id. device unique ID.
 *
 * @retval   (DEV_DESC_IF_S *): success, NULL: failure
 */
DEV_DESC_IF_S *tuya_user_iot_misc_dev_desc_get(const char *dev_id);

/**
 * @brief     traverse all subdevice.
 *
 * @param iter. iterator.
 *
 * @retval   (DEV_DESC_IF_S *) (NULL means traverse end)
 */
DEV_DESC_IF_S *tuya_user_iot_misc_dev_traversal(void **iter);

/**
 * @brief  update device and its extended firmware version. 
 * 
 * @param dev_id.   device unique ID.
 * @param version.  device version.
 * @param attr.     extended firmware
 * @param attr_num. number of extended firmware
 *
 * @retval   0: success
 * @retval  !0: failure
 */
int tuya_user_iot_misc_dev_ver_update_ext(const char *dev_id, const char *version, const GW_ATTACH_ATTR_T *attr, const UINT_T attr_num);

/**
 * @brief  bind device with extended firmware version.
 *
 * @param tp.       firmware type.
 * @param uddd.     custom filed, the highest bit must be 1.
 * @param dev_id.   device unique ID.
 * @param pid.      product ID.
 * @param ver.      device firmware version.
 * @param attr.     extended firmware
 * @param attr_num. number of extended firmware
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
int tuya_user_iot_misc_dev_bind_ext(ty_dev_tp_t tp, uint32_t uddd, const char *dev_id, const char *pid, const char *ver, \
                                    const GW_ATTACH_ATTR_T *attr, const UINT_T attr_num);

/**
 * @brief  report device upgrade progress
 * 
 * @param dev_id.     device unique ID.
 * @param tp.         firmware type.
 * @param percentage. percentage.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
int tuya_user_iot_misc_dev_upgd_progress_report(const char *dev_id, ty_dev_tp_t tp, UINT_T percentage);

#ifdef __cplusplus
}
#endif
#endif
