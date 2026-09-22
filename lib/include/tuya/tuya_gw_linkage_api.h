/*
tuya_gw_linkage_api.h
Copyright(C),2021-09-30, 涂鸦科技 www.tuya.comm
*/

#ifndef __TUYA_GW_LINKAGE_API_H
#define __TUYA_GW_LINKAGE_API_H

#include "tuya_cloud_types.h"
#include "tuya_cloud_com_defs.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief 设置局域网联动中检查其他节点的心跳超时时间，默认60秒
 *          在初始化之前设置生效
 * @param timeout_s 心跳超时时间，单位是秒
 * @return 
 */

VOID tuya_iot_lan_linkage_node_timeout_set(IN UINT_T timeout_s);

#ifdef __cplusplus
}
#endif
#endif

