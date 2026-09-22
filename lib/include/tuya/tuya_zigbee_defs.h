/**
 * @file    tuya_zigbee_defs.h
 * @brief   This file define data type for ZigBee
 * @version 0.1
 * 
 * @copyright Copyright (c) tuya.inc 2019
 */
#ifndef __TUYA_ZIGBEE_DEFS_H__
#define __TUYA_ZIGBEE_DEFS_H__

#include "tuya_cloud_types.h"
#include "tuya_cloud_com_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define Z3_DEV_ID_LEN    32
#define MAX_EP_NUM       10
#define MAX_CLUSTER_NUM  10
#define MANU_NAME_LEN    32
#define MODEL_ID_LEN     32

typedef VOID (*TY_Z3_DEV_JOIN_CB)(CONST CHAR_T *dev_id, CONST CHAR_T *pid, CONST CHAR_T *ver);
typedef VOID (*TY_Z3_DEV_LEAVE_CB)(CONST CHAR_T *dev_id);
typedef VOID (*TY_Z3_DEV_OFFLINE_CB)(CONST CHAR_T *dev_id);
typedef VOID (*TY_Z3_DEV_HEARTBEAT_QUERY_CB)(CONST CHAR_T *dev_id, CONST CHAR_T *ver);
typedef VOID (*TY_Z3_DEV_UPGRADE_STATUS_CB)(CONST CHAR_T *dev_id, INT_T status);
typedef VOID (*TY_Z3_DEV_UPGRADE_PROGRESS_CB)(CONST CHAR_T *dev_id, UINT_T total, UINT_T offset);
typedef VOID (*TY_Z3_DEV_OBJ_DP_REPT_CB)(CONST CHAR_T *dev_id, CONST TY_OBJ_DP_S *dps, CONST UINT_T dp_cnt);
typedef VOID (*TY_Z3_DEV_RAW_DP_REPT_CB)(CONST CHAR_T *dev_id, BYTE_T dpid, CONST BYTE_T *data, UINT_T len);

typedef struct {
    CHAR_T *manu_name;
    CHAR_T *model_id;
} TY_Z3_DEV_S;

typedef struct {
    TY_Z3_DEV_S  *devs;
    UINT16_T      dev_num;
} TY_Z3_DEVLIST_S;

typedef struct {
    CHAR_T     id[Z3_DEV_ID_LEN+1];
    UINT16_T   profile_id[MAX_EP_NUM];
    UINT16_T   device_id[MAX_EP_NUM];
    UINT16_T   cluster_id[MAX_EP_NUM][MAX_CLUSTER_NUM];
    UINT16_T   endpoint[MAX_EP_NUM];
    UCHAR_T    ep_num; // endpoints count
    UCHAR_T    uc_num; // clusters count
    UINT16_T   node_id;
    CHAR_T     manu_name[MANU_NAME_LEN+1];
    CHAR_T     model_id[MODEL_ID_LEN+1];
    CHAR_T     rejoin_flag;
    CHAR_T     power_source;
    UCHAR_T    version;
} TY_Z3_DESC_S;

typedef struct {
    CHAR_T     id[Z3_DEV_ID_LEN+1];
    UINT16_T   node_id;
    UINT16_T   profile_id;
    UINT16_T   cluster_id;
    UCHAR_T    src_endpoint;
    UCHAR_T    dst_endpoint;
    UINT16_T   group_id;
    UCHAR_T    cmd_type;          // zcl command type. 0x00: global command, 0x01: command is specific to a cluster.
    UCHAR_T    cmd_id;            // zcl command id
    UCHAR_T    frame_type;        // 0: Unicast, 1: Multicast, 2: Broadcast
    UINT16_T   manufacturer_code;
    CHAR_T     disable_ack;
    UINT16_T   msg_length;
    UCHAR_T   *message;
} TY_Z3_APS_FRAME_S;

typedef struct {
    UCHAR_T channel;
    CHAR_T rssi;
} TY_Z3_ENERGY_SCAN_RESULT_S;

typedef struct {
    UINT16_T panid;
    UCHAR_T channel;
    UCHAR_T lqi;
    CHAR_T rssi;
} TY_Z3_ACTIVE_SCAN_RESULT_S;

typedef struct {
    VOID (*join)(TY_Z3_DESC_S *dev);
    VOID (*leave)(CONST CHAR_T *dev_id);
    VOID (*report)(TY_Z3_APS_FRAME_S *frame);
    VOID (*notify)(VOID);
    VOID (*upgrade_end)(CONST CHAR_T *dev_id, INT_T rc, UCHAR_T version);
    VOID (*version)(CONST CHAR_T *dev_id, UCHAR_T version);
} TY_Z3_DEV_CBS_S;

typedef struct {
    TY_Z3_DEV_JOIN_CB              dev_join_cb;
    TY_Z3_DEV_LEAVE_CB             dev_leave_cb;
    TY_Z3_DEV_OFFLINE_CB           dev_offline_cb;
    TY_Z3_DEV_HEARTBEAT_QUERY_CB   dev_heartbeat_query_cb;
    TY_Z3_DEV_UPGRADE_STATUS_CB    dev_upgrade_status_cb;
    TY_Z3_DEV_UPGRADE_PROGRESS_CB  dev_upgrade_progress_cb;
    TY_Z3_DEV_OBJ_DP_REPT_CB       dev_obj_dp_rept_cb;
    TY_Z3_DEV_RAW_DP_REPT_CB       dev_raw_dp_rept_cb;
} TY_Z3_DEV_LC_MGR_S;

typedef VOID (*TY_Z3_NCP_GOT_VER_CB)(CHAR_T *ver);
typedef VOID (*TY_Z3_RFTEST_RESULT_CB)(USHORT_T npacket);

typedef VOID (*TY_Z3_NETWORK_SCAN_RESULT_CB)(TY_Z3_ACTIVE_SCAN_RESULT_S *active_results, UCHAR_T active_num, \
                                             TY_Z3_ENERGY_SCAN_RESULT_S *energy_results, UCHAR_T energy_num);

#ifdef __cplusplus
}
#endif
#endif
