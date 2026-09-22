/**
 * @file    tuya_gw_dp_api.h
 * @brief   This file contains datapoint management interface
 * @version 0.1
 * 
 * @copyright Copyright 2021 Tuya Inc. www.tuya.com
 */
#ifndef TUYA_GW_DP_API_H
#define TUYA_GW_DP_API_H

#include <stdint.h>

#include "tuya_cloud_com_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef BYTE_T TY_DP_TYPE_E;
#define DP_TYPE_BOOL     PROP_BOOL
#define DP_TYPE_VALUE    PROP_VALUE
#define DP_TYPE_STR      PROP_STR
#define DP_TYPE_ENUM     PROP_ENUM
#define DP_TYPE_BITMAP   PROP_BITMAP

typedef TY_OBJ_DP_S      ty_dp_s;
typedef TY_RECV_OBJ_DP_S ty_obj_cmd_s;
typedef TY_RECV_RAW_DP_S ty_raw_cmd_s;
typedef TY_DP_QUERY_S    ty_data_query_s;

typedef struct {
    int (*dev_obj_cmd_cb)(const ty_obj_cmd_s *dp);
    int (*dev_raw_cmd_cb)(const ty_raw_cmd_s *dp);
    int (*dev_data_query_cb)(const ty_data_query_s *query);
} ty_dev_cmd_cbs_s;

typedef struct {
    int (*dev_obj_dp_report_cb)(const char *dev_id, const ty_dp_s *dps, uint32_t dps_cnt);
    int (*dev_raw_dp_report_cb)(const char *dev_id, uint32_t dpid, uint8_t *data, uint32_t len);
} ty_dev_data_cbs_s;

/**
 * @brief  register device command callback.
 *
 * @param[in] cbs. a series of callback function.
 * 
 * @retval  0: success
 * @retval !0: failure
 */
int tuya_user_iot_reg_dev_cmd_cb(const ty_dev_cmd_cbs_s *cbs);

/**
 * @brief  register device data report callback.
 *
 * @param[in] cbs. a series of callback function.
 * 
 * @retval  0: success
 * @retval !0: failure
 */
int tuya_user_iot_reg_dev_data_cb(const ty_dev_data_cbs_s *cbs);

/**
 * @brief  report obj dp to tuay cloud.
 *
 * @param[in] dev_id.  device unique ID.
 * @param[in] dps.     dp array.
 * @param[in] dps_cnt. length of dp array.
 * 
 * @retval  0: success
 * @retval !0: failure
 */
int tuya_user_iot_report_obj_dp(const char *dev_id,
                                const ty_dp_s *dps,
                                uint32_t dps_cnt);

/**
 * @brief  report raw dp to tuya cloud.
 *
 * @param[in] dev_id.  device unique ID.
 * @param[in] dpid.    dp ID.
 * @param[in] dps.     raw data.
 * @param[in] len.     length of raw data.
 * @param[in] timeout. function blocks until timeout seconds.
 * 
 * @retval  0: success
 * @retval !0: failure
 */
int tuya_user_iot_report_raw_dp(const char *dev_id,
                                uint32_t dpid,
                                uint8_t *data,
                                uint32_t len,
                                uint32_t timeout);

/**
 * @brief  send obj cmd to device in local.
 * 
 * @param[in] cmd. obj cmd.
 * 
 * @retval  0: success
 * @retval !0: failure
 */
int tuya_user_iot_send_obj_cmd(const ty_obj_cmd_s *cmd);

/**
 * @brief  send raw cmd to device in local.
 * 
 * @param[in] cmd. raw cmd.
 * 
 * @retval  0: success
 * @retval !0: failure
 */
int tuya_user_iot_send_raw_cmd(const ty_raw_cmd_s *cmd);

/**
 * @brief  get dp description with dev_id and dpid.
 * 
 * @param[in] dev_id.  device unique ID.
 * @param[in] dpid.    dp ID.
 * 
 * @retval   (DP_DESC_IF_S *): success, NULL: failure
 */
DP_DESC_IF_S *tuya_user_iot_get_dp_desc(const char *dev_id, uint8_t dpid);

/**
 * @brief  get dp value with dev_id and dpid.
 * 
 * @param[in] dev_id.  device unique ID.
 * @param[in] dpid.    dp ID.
 * 
 * @retval   (DP_PROP_VALUE_U *): success, NULL: failure
 */
DP_PROP_VALUE_U *tuya_user_iot_get_dp_value(const char *dev_id, uint8_t dpid);

/**
 * @brief  set dp report timeout.
 * 
 * @param[in] timeout. timeout(unit: second).
 * 
 * @retval  0: success
 * @retval !0: failure
 */
int tuya_user_iot_set_dp_report_timeout(uint32_t timeout);

#ifdef __cplusplus
}
#endif
#endif // TUYA_GW_DP_API_H