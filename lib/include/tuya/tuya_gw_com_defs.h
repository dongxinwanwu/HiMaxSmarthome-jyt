/*
tuya_gw_com_defs.h
Copyright(C),2018-2021, 涂鸦科技 www.tuya.comm
*/

#ifndef __TUYA_GW_COM_DEFS_H__
#define __TUYA_GW_COM_DEFS_H__

#include "tuya_cloud_types.h"
#include "tuya_cloud_com_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIGMESH_NET_KEY_LEN 32
#define SIGMESH_APP_KEY_LEN 32
#define SIGMESH_DEV_KEY_LEN 32
#define SIGMESH_DEV_MAC_LEN 32
#define BLE_VERIFY_KEY_LEN  8

#define SUBDEV_BIND_OPTIONS_SRAND_LEN 20

/* tuya-sdk group process type */
typedef enum {
    GRP_ADD = 0, // add a group
    GRP_DEL      // delete a group
}GRP_ACTION_E;

/* tuya-sdk scene process type */
typedef enum {
    SCE_ADD = 0, // add a scene
    SCE_DEL,     // delete a scene
    SCE_EXEC     // execute a scene
}SCE_ACTION_E;

/**
 * @brief Definition of delete device type
 */
typedef enum {
    /** mqtt */
    GWDEV_DELTYPE_MQTT = 0,
    /** sync */
    GWDEV_DELTYPE_SYNC = 1,
}GW_DELDEV_TYPE;

#if defined(ENABLE_SIGMESH) && (ENABLE_SIGMESH==1)
typedef struct{
    CHAR_T *encrypted_data;
    CHAR_T product_key[PRODUCT_KEY_LEN+1];
    CHAR_T ver[SW_VER_LEN+1];
    CHAR_T uuid[DEV_UUID_LEN+1];
    CHAR_T beacon_srand[SUBDEV_BIND_OPTIONS_SRAND_LEN+1];
}TY_SUBDEV_BIND_AUTH_KEY;

typedef struct{
    CHAR_T beacon_srand[SUBDEV_BIND_OPTIONS_SRAND_LEN+1];
    CHAR_T kind[SW_VER_LEN+1];
    CHAR_T flag[SW_VER_LEN+1];
    CHAR_T pv[SW_VER_LEN+1];
    CHAR_T ext_data[TOKEN_LEN+1];
    BOOL_T is_pre_bind;
}TY_SUBDEV_BIND_OPTIONS;
#endif

/**
 * @brief Device sub type ,can be different type of devices.
 */
typedef BYTE_T DEV_SUB_TYPE_T;
#define NONE_DEV_SUB_TYPE   0   // no sub type
#define BLE_SUB_TP_MESH     0   // mesh
#define BLE_SUB_TP_SINGLE   1   // single
#define BLE_SUB_TP_BEACON   2   // beacon
#define BLE_SUB_TP_BEACON_V2 3  /**< beacon2.0 */

typedef BYTE_T DEV_ABI_E;
#define ABI_DEV_MESH            0x8000  //bit 15
#define ABI_DEV_SINGLE_BLE      0x400   //bit10
#define ABI_DEV_BEACON          0x200000 //bit21

typedef struct {
    CHAR_T id[DEV_ID_LEN+1];
    CHAR_T sw_ver[SW_VER_LEN+1];
    CHAR_T schema_id[SCHEMA_ID_LEN+1];
    CHAR_T product_key[PRODUCT_KEY_LEN+1];
    CHAR_T firmware_key[PRODUCT_KEY_LEN+1];
    BOOL_T is_oem;
    #if defined(ENABLE_SIGMESH) && (ENABLE_SIGMESH==1)
    CHAR_T sigmesh_dev_key[SIGMESH_DEV_KEY_LEN+1];
    CHAR_T sigmesh_mac[SIGMESH_DEV_MAC_LEN+1];
    CHAR_T sec_key[SIGMESH_DEV_KEY_LEN+1];
    #endif   
    CHAR_T virt_id[DEV_ID_LEN+1];

    USER_DEV_DTL_DEF_T uddd; // user detial type define
    USER_DEV_DTL_DEF_T uddd2; /* 最高为1，预留给用户自有子设备*/
    DEV_TYPE_T tp;
    DEV_SUB_TYPE_T sub_tp;
    CHAR_T uuid[DEV_UUID_LEN+1];
    INT_T abi;// ability
    BOOL_T bind;
    BOOL_T sync;
    #if defined(ENABLE_SIGMESH) && (ENABLE_SIGMESH==1)
    BOOL_T sigmesh_sync;
    BOOL_T ble_mesh_bind_rept_sync;
    USHORT_T beacon_addr;   // beacon short addr
    CHAR_T verify_key[BLE_VERIFY_KEY_LEN+1];
    #endif
    BOOL_T bind_status;
    BYTE_T attr_num;
    GW_ATTACH_ATTR_T attr[GW_ATTACH_ATTR_LMT];
    BOOL_T reset_flag;
    #if defined(TUYA_OPERATOR_TYPE) && (TUYA_OPERATOR_TYPE != 0)
    CH_CODE_ST ch_dminfo;
    #endif
	CHAR_T subList_flag;//优化sublist比较速度
    #if ((!defined(DISABLE_ZIGBEE_LQI)) || (defined(DISABLE_ZIGBEE_LQI) && (DISABLE_ZIGBEE_LQI == 0))) //临时使用，之后从这边移除
    DEV_QOS_ST dev_qos;
    #endif
    CHAR_T schema_flag;//优化schema更新比较的速度
    CHAR_T schema_sync;//schema是否同步完成, 1 未同步
    CHAR_T schema_ver[SCHEMA_VER_LEN+1];
    UINT_T dev_attr; //子设备属性
    #if defined(ENABLE_SIGMESH) && (ENABLE_SIGMESH==1)
    TY_SUBDEV_BIND_OPTIONS options;
    #endif
}DEV_DESC_IF_S;

/**
 * @brief Handler to send heartbeat to subdevice
 * 
 * @param[in] dev_id sub-device id
 */
typedef VOID (*DEV_HEARTBEAT_SEND_CB)(IN CONST CHAR_T *dev_id);

/**
 * @brief Handler to send device online status change notify
 * 
 * @param[in] dev_id sub-device id
 * @param[in] online_stat sub-device online status
 */
typedef VOID (*DEV_ONLINE_STAT_SEND_CB)(IN CONST CHAR_T *dev_id, IN CONST BOOL_T online_stat);


/***********************************************************
*  callback function: GW_PERMIT_ADD_DEV_CB
*  Desc:    permit to add subdevices callback
*  Input:   tp      subdevice type
*  Input:   permit  is permit to add subdevices
*  Output:  timeout timeout
*  Return:  BOOL_T  if function process success, return true, if function process fails,return false.
***********************************************************/
typedef BOOL_T (*GW_PERMIT_ADD_DEV_CB)(IN CONST GW_PERMIT_DEV_TP_T tp,IN CONST BOOL_T permit,IN CONST UINT_T timeout);

/***********************************************************
*  callback function: GW_DEV_DEL_CB
*  Desc:    when a subdevice is deleted,use this callback to notify user
*  Input:   dev_id  subdevice_id
***********************************************************/
typedef VOID (*GW_DEV_DEL_CB)(IN CONST CHAR_T *dev_id, IN CONST GW_DELDEV_TYPE type);

/***********************************************************
*  callback function: GW_DEV_GRP_INFM_CB
*  Desc:    the group process command callback
*  Input:   action  group action type
*  Input:   dev_id  subdevice_id
*  Input:   grp_id  group_id
*  Return:  OPRT_OK:bind success. other values:bind failure
***********************************************************/
typedef OPERATE_RET (*GW_DEV_GRP_INFM_CB)(IN CONST GRP_ACTION_E action,IN CONST CHAR_T *dev_id,IN CONST CHAR_T *grp_id);

/***********************************************************
*  callback function: GW_DEV_SCENE_INFM_CB
*  Desc:    the scene process command callback
*  Input:   action  scene action type
*  Input:   dev_id  subdevice_id
*  Input:   grp_id  group_id
*  Input:   sce_id  scene_id
*  Return:  OPRT_OK:bind success. other values:bind failure
***********************************************************/
typedef OPERATE_RET (*GW_DEV_SCENE_INFM_CB)(IN CONST SCE_ACTION_E action,IN CONST CHAR_T *dev_id,\
                                                 IN CONST CHAR_T *grp_id,IN CONST CHAR_T *sce_id);

/***********************************************************
*  callback function: GW_BIND_DEV_INFORM_CB
*  Desc:    the subdevice bind result callback
*  Input:   dev_id: subdevice_id
*  Input:   op_ret  OPRT_OK:bind success. other values:bind failure
***********************************************************/
typedef VOID (*GW_BIND_DEV_INFORM_CB)(IN CONST CHAR_T *dev_id, IN CONST OPERATE_RET op_ret);

/***********************************************************
*  callback function: DEV_WAKEUP_CB
*  Desc:    Wake up the low power subdevice
*  Input:   dev_id: subdevice id
*  Input:   time: wakeup duration unit:second
***********************************************************/
typedef VOID (*GW_DEV_WAKEUP_CB)(IN CONST CHAR_T *dev_id,IN UINT_T duration);

/***********************************************************
*  callback function: GW_DEV_SIGMESH_TOPO_UPDAET_CB
*  Desc:    the sigmesh topo update cb
*  Input:   dev_id: subdevice_id
*  Input:   op_ret  OPRT_OK:bind success. other values:bind failure
***********************************************************/
typedef VOID (*GW_DEV_SIGMESH_TOPO_UPDAET_CB)(IN CONST CHAR_T *dev_id);

/***********************************************************
*  callback function: GW_DEV_SIGMESH_TOPO_UPDAET_CB
*  Desc:    the sigmesh topo update cb
*  Input:   dev_id: subdevice_id
*  Input:   op_ret  OPRT_OK:bind success. other values:bind failure
***********************************************************/
typedef VOID (*GW_DEV_SIGMESH_DEV_CACHE_DEL_CB)(IN CONST CHAR_T *dev_id);

/***********************************************************
*  Callback function: GW_DEV_SIGMESH_CONN_CB
*  Desc:    The sigmesh connection callback, tell devie 
*           to disconnect or connect timeout value
*  Output: igmesh_dev_conn_inf_json
*         {
*           "type":"CONNECT", 
*          "nodeId":[xxxx,xxxx2],
*          "timeout":-1  //连接时 参数有效-1表示不会断开，
*                       //1000表示连上1000s后断开，若无timeout
*                       //参数 则只进行连接，timeout使用原设备
*                       //记录的timeout
*          }
*       或{
*         "type":"DISCONNECT",
*         "nodeId":[xxxx,xxxx2]
*        }
***********************************************************/
typedef VOID (*GW_DEV_SIGMESH_CONN_CB)(OUT CHAR_T *sigmesh_dev_conn_inf_json);

/***********************************************************
*  callback function: GW_BIND_BLE_MESH_DEV_INFORM_CB
*  Desc:    the ble mesh subdevice bind result callback
*  Input:   dev_id: subdevice_id
*  Input:   virt_id: subdevice virtual id
*  Input:   op_ret  OPRT_OK:bind success. other values:bind failure
***********************************************************/
typedef VOID (*GW_BIND_BLE_MESH_DEV_INFORM_CB)(IN CONST CHAR_T *dev_id, IN CONST CHAR_T *virt_id, IN CONST OPERATE_RET op_ret);


typedef struct {
    GW_PERMIT_ADD_DEV_CB gw_add_dev_cb;
    GW_DEV_DEL_CB gw_del_cb;
    GW_DEV_GRP_INFM_CB gw_dev_grp_cb;
    GW_DEV_SCENE_INFM_CB gw_dev_scene_cb;
    GW_BIND_DEV_INFORM_CB gw_ifm_cb;
    #if defined(ENABLE_SIGMESH) && (ENABLE_SIGMESH==1)
    GW_DEV_SIGMESH_TOPO_UPDAET_CB gw_dev_sigmesh_topo_update_cb;
    GW_DEV_SIGMESH_DEV_CACHE_DEL_CB gw_dev_sigmesh_dev_cache_del;
    #endif
    GW_DEV_WAKEUP_CB gw_dev_wakeup_cb;
    #if defined(ENABLE_SIGMESH) && (ENABLE_SIGMESH==1)
    GW_DEV_SIGMESH_CONN_CB gw_dev_sigmesh_conn_cb;
    GW_BIND_BLE_MESH_DEV_INFORM_CB gw_ble_mesh_ifm_cb;
    #endif
}TY_IOT_GW_CBS_S;

#ifdef __cplusplus
}
#endif
#endif
