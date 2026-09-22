/**
 * @file    tuya_gw_z3_api.h
 * @brief   This file contains ZigBee wireless protocol interface
 * @version 0.1
 * 
 * @copyright Copyright 2021 Tuya Inc. www.tuya.com
 */
#ifndef TUYA_GW_Z3_API_H
#define TUYA_GW_Z3_API_H

#include "tuya_zigbee_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef VOID (*ZIG_PERMIT_JOIN_CB)(BOOL_T permit, UINT_T timeout);
typedef VOID (*ZIG_NCP_UPGRADE_STATUS_CB)(BOOL_T status);

/**
 * @brief    register 3rd-party ZigBee device management. SDK will ignore the list of device,
 *           those devices should be developed by developer.
 * 
 * @param[in] devlist. a set of device contains its information.
 *                     devlist == NULL, mean filter all device.
 * @param[in] cbs.     register callback function.
 * 
 * @retval   0: success
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_mgr_register(TY_Z3_DEVLIST_S *devlist, TY_Z3_DEV_CBS_S *cbs);

/**
 * @brief    register ZigBee permit join callback.
 * 
 * @param[in] cb. callback function.
 * 
 * @retval   0: success
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_reg_permit_join_cb(ZIG_PERMIT_JOIN_CB cb);

/**
 * @brief    send a ZCL frame to ZigBee device.
 *
 * @param frame. ZCL message header and payload.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_send(TY_Z3_APS_FRAME_S *frame);

/**
 * @brief    make a ZigBee device to leave network.
 *
 * @param id.  unique ID for device.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_del(CONST CHAR_T *dev_id);

/**
 * @brief    upgrade a ZigBee device.
 *
 * @param id.    unique ID for device.
 * @param fw.    firmware info.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_upgrade(CONST CHAR_T *dev_id, CONST CHAR_T *img);

/**
 * @brief    upgrade NCP firmware.
 * 
 * @param cb.    upgrade result callback.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_ncp_upgrade(ZIG_NCP_UPGRADE_STATUS_CB cb);

/**
 * @brief    set NCP MAC address.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_ncp_set_mac(CONST CHAR_T *mac);

/**
 * @brief    reset NCP all setting.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_ncp_reset(VOID);

/**
 * @brief    get NCP version.
 * 
 * @param ver. version buffer.
 *
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_ncp_ver(CHAR_T *ver);

/**
 * @brief    rf test for coordinator.
 * 
 * @param cb.      callback function.
 * @param channel. select a channel to send packet.
 * @param power.   set coordinator tx power.
 * @param msg_len. length of payload.
 *                 if use Tuya ZigBee dongle for testing, msg_len = 12.
 * @param msg.     payload.
 *                 if use Tuya ZigBee dongle for testing, msg must be:
 *                 {0x55, 0xaa, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39} 
 * @param npacket. number of packet.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_rftest(TY_Z3_RFTEST_RESULT_CB cb, UCHAR_T channel, CHAR_T power, \
                                     UCHAR_T msg_len, UCHAR_T *msg, USHORT_T npacket);

/**
 * @brief   set channel.
 * 
 * @param channel.  channel.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_channel_set(UCHAR_T channel);

/**
 * @brief   get channel.
 * 
 * @param channel.  channel.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_channel_get(UCHAR_T *channel);

/**
 * @brief   channel scan.
 * 
 * @param channel_mask.  bit mask of channel. 11~26 is 0x07FFF800UL => 0111111111111111100000000000
 * @param cbs.           scan result callback.
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_network_scan(UINT_T channel_mask, TY_Z3_NETWORK_SCAN_RESULT_CB cb);

/**
 * @brief   set linkage filter timeout.
 * @note    This API should be called before `tuya_user_iot_init`.
 * 
 * @param timeout.  timeout(unit: second), 0x00: disable, 0xFFFF: Permanently enabled
 * 
 * @retval   0: sucess
 * @retval  !0: failure
 */
OPERATE_RET tuya_user_iot_zig_filterlink_timeout_set(USHORT_T timeout);

#ifdef __cplusplus
}
#endif
#endif
