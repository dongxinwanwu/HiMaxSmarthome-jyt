#include "common.h"
#include "execinfo.h"
#include "fcntl.h"
#include "knxHandle.h"
#include "libxml/parser.h"
#include "list.h"
#include "misc_dev_plugins.h"
#include "modbusHandle.h"
#include "parseXML.h"
#include "publicDef.h"
#include "thpool.h"
#include "tuya_gw_dp_api.h"
#include "tuya_gw_infra_api.h"
#include "tuya_gw_misc_api.h"
#include "tuya_gw_z3_api.h"
#include "version.h"
#include "zlog.h"
#include <dlfcn.h>
#include <linux/input.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/select.h>
#include <unistd.h>
#include <getopt.h>
#include "fakeServer.h"
#include "production_utils.h"

#define Z3_PROFILE_ID_HA 0x0104
#define ZCL_BASIC_CLUSTER_ID 0x0000
#define ZCL_ON_OFF_CLUSTER_ID 0x0006
#define Z3_CMD_TYPE_GLOBAL 0x00
#define Z3_CMD_TYPE_PRIVATE 0x01
#define ZCL_OFF_COMMAND_ID 0x00
#define ZCL_ON_COMMAND_ID 0x01

#define ZCL_READ_ATTRIBUTES_COMMAND_ID 0x00
#define READ_ATTRIBUTES_RESPONSE_COMMAND_ID 0x01
#define REPORT_ATTRIBUTES_COMMAND_ID 0x0A
#define READ_ATTRIBUTER_RESPONSE_HEADER 3 /* Attributer ID: 2 Bytes, Status: 1 Btye */
#define REPORT_ATTRIBUTES_HEADER 2        /* Attributer ID: 2 Bytes */

#define CRASH_SCRIPT "/opt/smarthome/script/crash_script.sh"
#define UPGRADE_SCRIPT "/opt/smarthome/script/upgrade_script.sh"
#define STARTUP_SCRIPT "/opt/smarthome/script/startup_script.sh"

const char *pAcdGroupXPath = "/Device/Group/AirCondition/Cell";
const char *pCurtainGroupXPath = "/Device/Group/Curtain/Cell";

typedef enum {
    MSG_TYPE_LOG_CONFIG_SYNC = 1,
    MSG_TYPE_DI_DEVICES_DEBUG,
    MSG_TYPE_MBUS_DEVICES_DEBUG,
    MSG_TYPE_KNX_DEVICES_DEBUG,
} MsgType_e;

typedef struct {
    bool bZ3;
    bool bMbusAcd;
    bool bMbusVent;
    bool bMbusHeat;
    bool bMbusAqi;
    bool bMbusHumidifier;
    bool bMbusPurifier;
    bool bKnxAcd;
    bool bKnxVent;
    bool bKnxHeat;
    bool bKnxPanel;
    bool bKnxCurtain;
    bool bDi;
    bool bDo;
    bool bGroup;
} HasSysDev_t;

typedef struct {
    long type;
    char context[256];
} msgq_t;



SoftWareVersion_t SoftVer = {
    .major = 1,
    .minor = 0,
    .patch = 2,
    .desc = "Release"};
DeviceElement_t GatewayElement;
int engineer = 0;
bool g_bOnline = false;
volatile bool g_bActive = false;
bool bZigbeeModuleEnabled = false;
bool bKnxAutoAcKEnabled = true;
HasSysDev_t g_HasSysDev = {0};
Gateway_t *pGw = NULL;
queue_t *pPosQueue, *pBindPosQueue;
bool bXmlInit = false;

void key_scan (void *param);

__attribute__ ((unused)) void device_join (void *param);

int LoadZ3Plugin (list_t *pList);

int LoadModbusPlugin (list_t *pList);

int LoadKnxPlugin (list_t *pList);

int Knx_DevBind (list_t *pLst, bool permit);

int Knx_DevUnbind (list_t *pLst, const char *pDevId);

int Modbus_DevBind (list_t *pLst, bool permit);

int Modbus_DevUnbind (list_t *pLst, const char *pDevId);

void flashLight (void *param);

int EngineeringMode (void);

int BusinessMode (void);

int ReadEngineeringMode (void);

extern void PrintLogo (void);

void statusLed (bool bOn);

char *ProcessName (void);

char *ProcessPath (void);

void SingleInstance (void);

void associateKnxDev (void);

void associateModbusDev (void);

void ModbusDevGroup (void);

void di_scan (void *param);

int DIO_DevBind (list_t *pLst, bool permit);

int DIO_DevUnbind (list_t *pLst, const char *pDevId);

void ExceptionHandler (void);

char *DumpDiDevInfo (const char *pDevId);

char *DumpMbusDevInfo (const char *pDevId);

void RunLedThread (void *param);
void RunLed (bool bOn);
void ReportPosThread (void *param);
void ReportBindPosThread (void *param);

static list_t *pHwModelList = NULL;
list_t *pZ3List = NULL;
list_t *pMbusDevList = NULL;
list_t *pRf433List = NULL;
list_t *pKnxDevList = NULL;
list_t *pDioDevList = NULL;
list_t *pGroupList = NULL;
list_t *pKnxScene = NULL;

static threadpool thpool;
uint8_t g_Mac[13] = {0};
zlog_category_t *pLogCat = NULL;

TY_Z3_DEV_S my_z3_dev[] = {
    {(char *) "TUYATEC-nPGIPl5D", (char *) "TS0001"},
    {(char *) "TUYATEC-1xAC7DHQ", (char *) "TZ3000"}};

TY_Z3_DEVLIST_S my_z3_devlist = {
    .devs = my_z3_dev,
    .dev_num = CNTSOF (my_z3_dev),
};

extern int GetMac (uint8_t *pMac);

int Z3ModelIdMatch (void *pNodeValue, void *pModelId) {
    if (!pNodeValue || !pModelId) return 0;

    Z3Dev_t *pZ3Dev = (Z3Dev_t *) pNodeValue;
    if (!strcmp (pZ3Dev->pZ3->pModelId, (char *) pModelId)) {// default: model ID is unique.
        if (pZ3Dev->element.pPrimaryKey[0] == 0) {           // devId not exist!
            return 1;
        }
    }

    return 0;
}

int Z3DevIdMatch (void *pNodeValue, void *pDevId) {
    if (!pNodeValue || !pDevId) return 0;

    Z3Dev_t *pZ3 = (Z3Dev_t *) pNodeValue;
    if (!strcmp (pZ3->element.pPrimaryKey, (char *) pDevId)) {
        return 1;
    }

    return 0;
}

int KnxDevIdMatch (void *pNode, void *pDevId) {
    if (!pNode || !pDevId) return 0;

    KnxBase_t *pBase = (KnxBase_t *) pNode;
    if (!strcmp (pBase->element.pPrimaryKey, (char *) pDevId)) {
        return 1;
    }

    return 0;
}

int KnxPhyMatch (void *pNode, void *pPhy) {
    if (!pNode || !pPhy) return 0;

    KnxBase_t *pBase = (KnxBase_t *) pNode;
    if (Knx_ComparePhy (pBase->phyAddr, (char *) pPhy) == 0) {

        return 1;
    }

    return 0;
}

int Rf433DevIdMatch (void *pNodeValue, void *pDevId) {
    if (!pNodeValue || !pDevId) return 0;

    RfIf_t *pRf = (RfIf_t *) pNodeValue;
    if (!strcmp (pRf->pPrimaryKey, (char *) pDevId)) {
        return 1;
    }

    return 0;
}

int DioDevIdMatch (void *pNode, void *pDevId) {
    if (!pNode || !pDevId) return 0;

    DiDev_t *pDi = (DiDev_t *) pNode;
    if (!strcmp (pDi->element.pPrimaryKey, (char *) pDevId)) {
        return 1;
    }

    return 0;
}

int KnxSceneIdMatch (void *pNode, void *pDevId) {
    if (!pNode || !pDevId) return 0;

    KnxScene_t *pScene = (KnxScene_t *) pNode;
    if (pScene->remap == *(int *) pDevId) {
        return 1;
    }

    return 0;
}

void PreStartup (void) {
    if (access (STARTUP_SCRIPT, F_OK) == 0) {
        system ("chmod +x " STARTUP_SCRIPT);
        system (STARTUP_SCRIPT);
        logDbg ( "startup_script.sh executed!");
    }
}

static void __test_switch_read_attr (const char *dev_id) {
    OPERATE_RET op_ret = OPRT_OK;
    TY_Z3_APS_FRAME_S frame;
    USHORT_T atrr_buf[5] = {0x0000, 0x4001, 0x4002, 0x8001, 0x5000};
    logErr( COLOR_RED "开关:%s read attr." COLOR_CLEAR, dev_id);

    memset (&frame, 0x00, sizeof (TY_Z3_APS_FRAME_S));

    strncpy (frame.id, dev_id, sizeof (frame.id));
    frame.profile_id = Z3_PROFILE_ID_HA;
    frame.cluster_id = ZCL_ON_OFF_CLUSTER_ID;
    frame.cmd_type = Z3_CMD_TYPE_GLOBAL;
    frame.src_endpoint = 0x01;
    frame.dst_endpoint = 0xff;// all endpoints
    frame.cmd_id = ZCL_READ_ATTRIBUTES_COMMAND_ID;

    frame.msg_length = sizeof (atrr_buf);
    frame.message = (UCHAR_T *) atrr_buf;

    op_ret = tuya_user_iot_zig_send (&frame);
    if (op_ret != OPRT_OK) {
        log_err ("tuya_user_iot_zig_send err: %d", op_ret);
        return;
    }
}

static void switch_read_ver (const char *dev_id) {
    OPERATE_RET op_ret = OPRT_OK;
    TY_Z3_APS_FRAME_S frame;
    USHORT_T atrr_buf[1] = {0x0001};
    logErr ( COLOR_RED "面板(%s)心跳请求." COLOR_CLEAR, dev_id);

    strncpy (frame.id, dev_id, sizeof (frame.id));
    frame.profile_id = Z3_PROFILE_ID_HA;
    frame.cluster_id = ZCL_BASIC_CLUSTER_ID;
    frame.cmd_type = Z3_CMD_TYPE_GLOBAL;
    frame.src_endpoint = 0x01;
    frame.dst_endpoint = 0x01;
    frame.cmd_id = ZCL_READ_ATTRIBUTES_COMMAND_ID;

    frame.msg_length = sizeof (atrr_buf);
    frame.message = (UCHAR_T *) atrr_buf;

    op_ret = tuya_user_iot_zig_send (&frame);
    if (op_ret != OPRT_OK) {
        log_err ("tuya_user_iot_zig_send err: %d", op_ret);
        return;
    }
}

static int _iot_get_uuid_authkey_cb (char *uuid, int uuid_size, char *authkey, int authkey_size) {
    if (!uuid || !authkey) return -1;
    log_err ("uuid:%s, authkey:%s, %d,%d", GatewayElement.szUUID, GatewayElement.szAuthKey, uuid_size, authkey_size);
    strncpy (uuid, GatewayElement.szUUID, uuid_size);
    strncpy (authkey, GatewayElement.szAuthKey, authkey_size);

    return 0;
}

static int _iot_get_product_key_cb (char *pk, int pk_size) {
    strncpy (pk, GatewayElement.pProductId, pk_size);

    return 0;
}

static int _gw_active_status_changed_cb (ty_gw_status_t status) {
    zlog_info (pLogCat, "gateway %s", status ? "active" : "inactive");
    if (status == TY_GW_STATUS_REGISTERED) {
        // g_bActive = true;
        // system ("touch " GW_ACTIVE_PATH);
        // system ("rwdisk -w 0x200E0 active");

        int posKey = 1;
        if (queue_put (pPosQueue, (void *) &posKey) != 0) {
            logErr ( "put position queue failed!");
        }
    } else {
        // g_bActive = false;
        // system ("rm -f " GW_ACTIVE_PATH);
        // system ("rwdisk -w 0x200E0 ------");

        void *iter = NULL;
        DEV_DESC_IF_S *dev_if = tuya_user_iot_misc_dev_traversal (&iter);
        while (dev_if) {
            int ret = tuya_user_iot_misc_dev_unbind (dev_if->id);
            if (ret != OPRT_OK) {
                logErr ( COLOR_RED "sub-device:%s unbind fail!" COLOR_CLEAR, dev_if->id);
                continue;
            }
            zlog_info (pLogCat, COLOR_GREEN "sub-device:%s unbind success!" COLOR_CLEAR, dev_if->id);
            dev_if = tuya_user_iot_misc_dev_traversal (&iter);
        }
    }

    return 0;
}

static int _gw_online_status_changed_cb (bool online) {
    if (online) {
        system ("echo heartbeat > /sys/class/leds/chip-heartbeat/trigger");
    }
    g_bActive = g_bOnline = online;
    if (g_bOnline) {
        zlog_info (pLogCat, COLOR_GREEN "gateway online" COLOR_CLEAR);
        statusLed (true);
    } else {
        zlog_info (pLogCat, COLOR_RED "gateway offline" COLOR_CLEAR);
        statusLed (false);
        return 0;
    }

    int posKey = 0;
    if (queue_put (pPosQueue, (void *) &posKey) != 0) {
        logErr ( "put position queue failed!");
    }

    return 0;
}

static int _gw_fetch_local_log_cb (char *path, int path_len) {
    zlog_info (pLogCat, COLOR_RED "Note: the local log will be fetched!" COLOR_CLEAR);
    system ("tar -czf /tmp/log.tar.gz /opt/smarthome/log");
    strncpy (path, "/tmp/log.tar.gz", MIN ((uint32_t) path_len, strlen ("/tmp/log.tar.gz")));

    return 0;
}

static void _gw_reboot_cb (void) {
    zlog_info (pLogCat, COLOR_RED "Warning: The gateway will be restarted in 2 seconds." COLOR_CLEAR);
    sleep (2);
    statusLed (false);

    system ("reboot");
}

static void _gw_reset_cb (void) {
    zlog_info (pLogCat, COLOR_RED "Warning: The gateway will be resetting to factory settings in 5 seconds." COLOR_CLEAR);
    sleep (3);

    void *iter = NULL;
    DEV_DESC_IF_S *dev_if = tuya_user_iot_misc_dev_traversal (&iter);
    while (dev_if) {
        int ret = tuya_user_iot_misc_dev_unbind (dev_if->id);
        if (ret != OPRT_OK) {
            logErr ( COLOR_RED "sub-device:%s unbind fail!" COLOR_CLEAR, dev_if->id);
            continue;
        }
        zlog_info (pLogCat, COLOR_GREEN "sub-device:%s unbind success!" COLOR_CLEAR, dev_if->id);
        dev_if = tuya_user_iot_misc_dev_traversal (&iter);
    }
#if 0
    system ("rm -rf /opt/smarthome/tmp/*");
#else
    tuya_user_iot_reset_gw ();
#endif
    BusinessMode ();
    //    tuya_user_iot_unactive_gw ();
}

static void _gw_upgrade_cb (const char *img) {
    zlog_info (pLogCat, COLOR_RED "Upgrade ready! img: %s" COLOR_CLEAR, img);
    char pCmd[256] = {0};
    snprintf (pCmd, sizeof (pCmd), "tar -xf %s -C /", img);
    system (pCmd);
    zlog_info (pLogCat, COLOR_RED COLOR_BOLD "upgrade completed, restart..." COLOR_CLEAR);

    if (access (UPGRADE_SCRIPT, F_OK) == 0) {
        system ("chmod +x " UPGRADE_SCRIPT);
        system (UPGRADE_SCRIPT);
        system ("rm -f " UPGRADE_SCRIPT);
        logDbg ( "upgrade_script.sh executed!");
    }
    sleep (1);
    system ("reboot");
}

static int _dev_obj_cmd_cb (const ty_obj_cmd_s *dp) {
    for (uint32_t i = 0; i < dp->dps_cnt; i++) {
        if (!dp->cid) {
            if (dp->dps[i].dpid == 4) {// 报警声开关
                logDbg ( "报警声开关: %d", dp->dps[i].value.dp_value);
            } else if (dp->dps[i].dpid == 32) {// 主机状态
                logDbg ( "主机状态: %d", dp->dps[i].value.dp_value);
            } else if (dp->dps[i].dpid == 34) {// 恢复出厂设置
                logDbg ( "恢复出厂设置: %d", dp->dps[i].value.dp_value);
                tuya_user_iot_reset_gw ();
            } else if (dp->dps[i].dpid == 45) {// 主动报警
                logDbg ( "主动报警: %d", dp->dps[i].value.dp_value);
            } else if (dp->dps[i].dpid == 101) {// 系统场景
                listSetMatchMethod (pKnxScene, KnxSceneIdMatch);
                listNode *pKnxNode = listSearchKey (pKnxScene, (int *) &dp->dps[i].value.dp_value);
                if (!pKnxNode) {
                    logDbg ( "系统场景%d不存在", dp->dps[i].value.dp_value);
                    return -1;
                }
                logDbg ( "系统场景%d被执行", dp->dps[i].value.dp_value);
                KnxScene_t *pScene = (KnxScene_t *) pKnxNode->value;
                Knx_Write1Byte (pScene->addr, pScene->value);
            } else {
                logDbg ( "未知dpid: %d", dp->dps[i].dpid);
            }
            continue;
        }

        DEV_DESC_IF_S *dev_if = tuya_user_iot_misc_dev_desc_get (dp->cid);
        if (!dev_if) {
            logErr ( "device id is not found");
            return -1;
        }
        char *pType = NULL;
        if (dev_if->tp == DEV_TP_485) {
            pType = (char *) "RS485";
        } else if (dev_if->tp == DEV_TP_KNX) {
            pType = (char *) "KNX";
        } else {
            pType = (char *) "Other";
        }
        logDbg ( COLOR_BOLD COLOR_BLUE "[tuya ctrl] cid=%s type=%s dp=%d value=%d" COLOR_CLEAR, dp->cid, pType, dp->dps[i].dpid, dp->dps[i].value.dp_value);
        tuya_user_iot_report_obj_dp (dp->cid, &dp->dps[i], 1);

        if (dev_if->tp == DEV_TP_ZIGBEE) {
            listSetMatchMethod (pZ3List, Z3DevIdMatch);
            listNode *pNode = listSearchKey (pZ3List, dp->cid);
            if (!pNode) {
                logErr ( COLOR_YELLOW "Z3 dev_id:%s not found!" COLOR_CLEAR, dp->cid);
                return -1;
            }
            Z3Dev_t *pZ3 = (Z3Dev_t *) pNode;
            if (pZ3->pZ3->z3_dev_control) {
                TY_Z3_APS_FRAME_S frame;
                pZ3->pZ3->z3_dev_control (dp->cid, (ty_obj_dp_s *) &dp->dps[i], (ty_z3_aps_frame_s *) &frame);
                int ret = tuya_user_iot_zig_send (&frame);
                if (frame.message) {
                    free (frame.message);
                }
                if (ret != 0) {
                    log_err ("tuya_user_iot_zig_send err: %d", ret);
                    return -1;
                }
            }
        } else if (dev_if->tp == DEV_TP_485) {
            ModbusDevWrite (dp->cid, &dp->dps[i]);
        } else if (dev_if->tp == DEV_TP_KNX) {
            KnxGroupWrite (dp->cid, &dp->dps[i]);
        } else if (dev_if->tp == DEV_TP_433) {
            logDbg ( "Stay tuned for the next version!" COLOR_CLEAR);
        } else {
            logErr ( "_dev_obj_cmd_cb device type %d not supported!", dev_if->tp);
        }
    }

    return 0;
}

static int _dev_raw_cmd_cb (const ty_raw_cmd_s *dp) {
    zlog_info (pLogCat, "device raw cmd callback");
    zlog_info (pLogCat, "cmd_tp: %d, dtt_tp: %d, dpid: %d, len: %u", dp->cmd_tp, dp->dtt_tp, dp->dpid, dp->len);

    log_debug ("data: ");
    for (uint32_t i = 0; i < dp->len; i++) {
        printf ("%02x ", dp->data[i]);
    }

    printf ("\n");

    return 0;
}

static void _misc_dev_add_cb (bool permit, uint32_t timeout) {
    if (permit) {
        zlog_info (pLogCat, "add misc device, timeout=%u", timeout);
        statusLed (true);
        Knx_DevBind (pKnxDevList, permit);
        Modbus_DevBind (pMbusDevList, permit);
        DIO_DevBind (pDioDevList, permit);
    } else {
        zlog_info (pLogCat, "stop add misc device");
        statusLed (false);
    }
}

static void _misc_dev_del_cb (const char *dev_id) {
    int ret = 0;
    DEV_DESC_IF_S *dev_if = NULL;

    zlog_info (pLogCat, "delete misc device: %s", dev_id);

    dev_if = tuya_user_iot_misc_dev_desc_get (dev_id);
    if (dev_if == NULL) {
        logErr ("dev_id %s is not found", dev_id);
        return;
    }

    if (dev_if->tp == DEV_TP_ZIGBEE) {
        ret = tuya_user_iot_zig_del (dev_id);
        if (ret != 0) {
            logErr ("tuya_user_iot_zig_del err: %d", ret);
            return;
        }
    } else if (dev_if->tp == DEV_TP_KNX) {
        /*  release resources */
        ret = Knx_DevUnbind (pKnxDevList, dev_id);
        if (ret == 0) return;
    } else if (dev_if->tp == DEV_TP_485) {
        ret = Modbus_DevUnbind (pMbusDevList, dev_id);
        if (ret == 0) return;
    } else if (dev_if->tp == DEV_TP_USR1) {
        ret = DIO_DevUnbind (pDioDevList, dev_id);
        if (ret == 0) return;
    } else {
        logErr ("device type %d not supported!", dev_if->tp);
        return;
    }
}

static void _misc_dev_bind_ifm_cb (const char *dev_id, int result) {
    int ret = 0;
    DEV_DESC_IF_S *dev_if = NULL;

    if (result != 0) {
        logErr ( "bind dev(%s) to gateway error", dev_id);
        return;
    }

    dev_if = tuya_user_iot_misc_dev_desc_get (dev_id);
    if (dev_if == NULL) {
        logErr ( "dev_id %s is not found\n", dev_id);
        return;
    }
    logDbg ( COLOR_RED "bind dev(%s -- %s) to gateway success" COLOR_CLEAR, dev_id, dev_if->id);
    // bind dev(02c7ecdad9252103 -- 02c7ecdad9252103) to gateway success

    if (queue_put (pBindPosQueue, (void *) dev_if) != 0) {
        logErr ( "put position queue failed!");
    }

    listSetMatchMethod (pKnxDevList, KnxDevIdMatch);
    listNode *pKnxNode = listSearchKey (pKnxDevList, (char *) dev_id);
    if (pKnxNode) {
        KnxBase_t *pKnx = (KnxBase_t *) pKnxNode->value;
        if (pKnx->bOnlineCheck == false) {
            logDbg ( COLOR_RED "knx device[%s] not config heartbeat!" COLOR_CLEAR, dev_id);
            return;
        }
    }

    listSetMatchMethod (pMbusDevList, ModbusDevIdMatch);
    listNode *pMbusNode = listSearchKey (pMbusDevList, (char *) dev_id);
    if (pMbusNode) {
        MbusBase_t **ppMbus = (MbusBase_t **) pMbusNode->value;
        if ((*ppMbus)->bOnlineCheck == false) {
            logDbg ( COLOR_RED "mbus device[%s] not config heartbeat!" COLOR_CLEAR, dev_id);
            return;
        }
    }

    ret = tuya_user_iot_misc_dev_hb_cfg (dev_id, 300, 6, false);
    if (ret != 0) {
        logErr ( "tuya_user_iot_misc_dev_hb_fresh err: %d", ret);
        return;
    }
    logDbg ( "bind dev(%s) to gateway success", dev_id);

    ret = tuya_user_iot_misc_dev_hb_fresh (dev_id);
    if (ret != 0) {
        logErr ( "tuya_user_iot_misc_dev_hb_fresh err: %d", ret);
        return;
    }

    if (dev_if->tp == DEV_TP_ZIGBEE) {
        __test_switch_read_attr (dev_id);
    }

    zlog_info (pLogCat, "bind misc device: %s", dev_id);
}

static void _misc_dev_upgrade_cb (const char *dev_id, const char *img) {
    DEV_DESC_IF_S *dev_if = NULL;

    zlog_info (pLogCat, "misc device: %s upgrade: %s", dev_id, img);

    dev_if = tuya_user_iot_misc_dev_desc_get (dev_id);
    if (dev_if == NULL) {
        log_err ("dev_id %s is not found", dev_id);
        return;
    }

    if (dev_if->tp == DEV_TP_ZIGBEE) {
        tuya_user_iot_zig_upgrade (dev_id, img);
    } else {
        // other device protocol
        // USER TODO
    }
}

static void _misc_dev_reset_cb (const char *dev_id) {
    int ret = 0;
    DEV_DESC_IF_S *dev_if = NULL;

    zlog_info (pLogCat, "misc device: %s reset", dev_id);

    dev_if = tuya_user_iot_misc_dev_desc_get (dev_id);
    if (dev_if == NULL) {
        log_err ("dev_id %s is not found", dev_id);
        return;
    }

    if (dev_if->tp == DEV_TP_ZIGBEE) {
        ret = tuya_user_iot_zig_del (dev_id);
        if (ret != 0) {
            log_err ("tuya_user_iot_zig_del err: %d", ret);
            return;
        }
    } else {
        // other device protocol
        // USER TODO
    }
}

static void _misc_dev_hb_cb (const char *dev_id) {
    DEV_DESC_IF_S *dev_if = NULL;
    int ret = 0;

    dev_if = tuya_user_iot_misc_dev_desc_get (dev_id);
    if (dev_if == NULL) {
        logErr ( "heartbeat: dev_id=%s not exist", dev_id);
        return;
    }

    logTrc ( "[heartbeat] %s dev_id:%s", (dev_if->tp == DEV_TP_KNX ? "KNX" : (dev_if->tp == DEV_TP_485 ? "RS485" : "Other")), dev_id);

    if (dev_if->tp == DEV_TP_ZIGBEE) {
        log_err ("[Z3 HB] device id:%s\n", dev_id);
        listSetMatchMethod (pZ3List, Z3DevIdMatch);
        listNode *pNode = listSearchKey (pZ3List, (char *) dev_id);
        if (!pNode) {
            logErr ( "zigbee 3.0 dev_id:%s not found!" COLOR_CLEAR, dev_id);
            return;
        }
        Z3Dev_t *pZ3 = (Z3Dev_t *) pNode;
        if (pZ3->pZ3->z3_dev_heartbeat) {
            TY_Z3_APS_FRAME_S frame;
            pZ3->pZ3->z3_dev_heartbeat (dev_id, (ty_z3_aps_frame_s *) &frame);
            ret = tuya_user_iot_zig_send (&frame);
            if (frame.message) {
                free (frame.message);
            }
            if (ret != 0) {
                log_err ("tuya_user_iot_zig_send err: %d", ret);
                return;
            }
        } else {
            switch_read_ver (dev_id);
        }
    } else if (dev_if->tp == DEV_TP_KNX) {
        static bool bFirst = false;
        static uint32_t cnt = 0;

        if (!bFirst) {
            if (++cnt >= 2) {
                bFirst = true;
            }
            Knx_SyncDevStatus (thpool);
        } else {
            Knx_RequestHB (dev_id);
        }
    } else if (dev_if->tp == DEV_TP_485) {
        static bool bFirst = false;
        static uint32_t cnt = 0;

        if (!bFirst) {
            if (++cnt >= 1) {
                bFirst = true;
            }
            Mbus_SyncDevStatus (thpool);
        } else {
            Mbus_RequestHB (thpool, dev_id);
        }
    } else if (dev_if->tp == DEV_TP_USR1) {
        logDbg ( "[DI HB] device id:%s", dev_id);
        tuya_user_iot_misc_dev_hb_fresh (dev_id);
    }
}

static void __my_z3_dev_join (TY_Z3_DESC_S *dev) {
    if (dev == NULL) {
        log_err ("invalid param");
        return;
    }
    logErr ( "__my_z3_dev_join------\n");

    listSetMatchMethod (pZ3List, Z3ModelIdMatch);
    listNode *pNode = listSearchKey (pZ3List, dev->model_id);// todo:默认model_id唯一
    if (!pNode) {
        logErr ( COLOR_YELLOW "zigbee 3.0 manu_name:%s,model_id:%s not found!\n" COLOR_CLEAR,
                    dev->manu_name,
                    dev->model_id);
        return;
    }
    Z3Dev_t *pZ3Dev = (Z3Dev_t *) pNode;

    memcpy ((char *) pZ3Dev->element.pPrimaryKey, dev->id, MIN (DEVICE_ID_LENGTH, strlen (dev->id)));
    logErr ( "pZ3Dev->pPrimaryKey:%s\n", pZ3Dev->element.pPrimaryKey);
    logErr (
                COLOR_YELLOW "zigbee 3.0 dev_id:%s,node_id:0x%04x,manu_name:%s,model_id:%s join.\n" COLOR_CLEAR,
                dev->id, dev->node_id, dev->manu_name, dev->model_id);

    int ret = tuya_user_iot_misc_dev_bind (DEV_TP_ZIGBEE, pZ3Dev->uddd, pZ3Dev->element.pPrimaryKey,
                                           pZ3Dev->element.pProductId, "1.0.7");
    if (ret != OPRT_OK) {
        log_err ("tuya_user_iot_dev_bind err: %d", ret);
        return;
    }
}

static void __my_z3_dev_leave (CONST CHAR_T *dev_id) {
    OPERATE_RET op_ret = OPRT_OK;

    if (dev_id == NULL) {
        log_err ("invalid param");
        return;
    }
    logErr ( "__my_z3_dev_leave------%s\n", dev_id);

    logErr ( COLOR_YELLOW "Z3 dev_id:%s leave.\n" COLOR_CLEAR, dev_id);
    listSetMatchMethod (pZ3List, Z3DevIdMatch);
    listNode *pNode = listSearchKey (pZ3List, (char *) dev_id);
    if (!pNode) {
        logErr ( COLOR_YELLOW "zigbee 3.0 dev_id:%s not found!\n" COLOR_CLEAR, dev_id);
        return;
    }
    listDelNode (pZ3List, pNode);

    op_ret = tuya_user_iot_misc_dev_unbind (dev_id);
    if (op_ret != OPRT_OK) {
        log_err ("tuya_user_iot_dev_unbind err: %d", op_ret);
        return;
    }
}

static void __my_z3_dev_report (TY_Z3_APS_FRAME_S *frame) {
    INT_T i = 0;
    TY_OBJ_DP_S dp_data = {0};
    int ret = OPRT_OK;

    if (frame == NULL) {
        log_err ("invalid param");
        return;
    }

    logErr ( COLOR_GREEN "device zcl report callback:\n" COLOR_CLEAR);
    logErr ( COLOR_GREEN "dev_id: %s\n" COLOR_CLEAR, frame->id);
    logErr ( COLOR_GREEN "profile_id: 0x%04x\n" COLOR_CLEAR, frame->profile_id);
    logErr ( COLOR_GREEN "cluster_id: 0x%04x\n" COLOR_CLEAR, frame->cluster_id);
    logErr ( COLOR_GREEN "node_id: 0x%04x\n" COLOR_CLEAR, frame->node_id);
    logErr ( COLOR_GREEN "src_ep: %d\n" COLOR_CLEAR, frame->src_endpoint);
    logErr ( COLOR_GREEN "dst_ep: %d\n" COLOR_CLEAR, frame->dst_endpoint);
    logErr ( COLOR_GREEN "group_id: %d\n" COLOR_CLEAR, frame->group_id);
    logErr ( COLOR_GREEN "cmd_type: %d\n" COLOR_CLEAR, frame->cmd_type);
    logErr ( COLOR_GREEN "command: 0x%02x\n" COLOR_CLEAR, frame->cmd_id);
    logErr ( COLOR_GREEN "frame_type: %d\n" COLOR_CLEAR, frame->frame_type);
    logErr ( COLOR_GREEN "msg_len: %d\n" COLOR_CLEAR, frame->msg_length);
    logErr ( COLOR_GREEN "msg: ");
    for (i = 0; i < frame->msg_length; i++) {
        printf ("%02x ", frame->message[i]);
    }
    printf ("\n" COLOR_CLEAR);

    ret = tuya_user_iot_misc_dev_hb_fresh (frame->id);
    if (ret != OPRT_OK) {
        log_warn ("tuya_user_iot_misc_dev_hb_fresh err: %d", ret);
    }

    listSetMatchMethod (pZ3List, Z3DevIdMatch);
    listNode *pNode = listSearchKey (pZ3List, frame->id);
    if (!pNode) {
        logErr ( COLOR_YELLOW "zigbee 3.0 dev_id:%s not found!\n" COLOR_CLEAR, frame->id);
        return;
    }
    Z3Dev_t *pZ3 = (Z3Dev_t *) pNode;
    if (pZ3->pZ3->z3_dev_report) {
        ret = pZ3->pZ3->z3_dev_report ((ty_z3_aps_frame_s *) frame, (ty_obj_dp_s *) &dp_data);
        if (ret != 0) {
            logErr ( "z3_dev_report fail\n");
            return;
        }
        ret = tuya_user_iot_report_obj_dp (frame->id, &dp_data, 1);
        if (ret != OPRT_OK) {
            log_err ("tuya_user_iot_report_obj_dp err: %d", ret);
        }
    }
}

static void __my_z3_dev_notify (VOID) {
    OPERATE_RET op_ret = OPRT_OK;
    //    TY_Z3_APS_FRAME_S frame;
    DEV_DESC_IF_S *dev_if = NULL;
    VOID *iter = NULL;

    logDbg ( COLOR_GREEN "__my_z3_dev_notify zigbee设备通知" COLOR_CLEAR);

    dev_if = tuya_user_iot_misc_dev_traversal (&iter);
    while (dev_if) {
        if (dev_if->tp == DEV_TP_ZIGBEE) {
            __test_switch_read_attr (dev_if->id);

            op_ret = tuya_user_iot_misc_dev_hb_cfg (dev_if->id, 120, 3, FALSE);
            if (op_ret != OPRT_OK) {
                log_warn ("tuya_user_iot_misc_dev_hb_cfg err: %d", op_ret);
            }
        }

        dev_if = tuya_user_iot_misc_dev_traversal (&iter);
    }
}

static void __my_z3_dev_upgrade_end (CONST CHAR_T *dev_id, INT_T rc, UCHAR_T version) {
    logDbg ( "dev upgrade end, dev_id: %s, result: %d, version: %d", dev_id, rc, version);

    // update new version
    tuya_user_iot_misc_dev_ver_update (dev_id, "8.8.8");
}

static void __my_z3_dev_version (CONST CHAR_T *dev_id, UCHAR_T version) {
    OPERATE_RET op_ret = OPRT_OK;

    logDbg ( "dev version report, dev_id: %s, version: %d", dev_id, version);

    op_ret = tuya_user_iot_misc_dev_hb_fresh (dev_id);
    if (op_ret != OPRT_OK) {
        logErr ( "tuya_user_iot_misc_dev_hb_fresh err: %d", op_ret);
    }
}

static void __z3_network_scan_result_cb (TY_Z3_ACTIVE_SCAN_RESULT_S *active_results, UCHAR_T active_num,
                                         TY_Z3_ENERGY_SCAN_RESULT_S *energy_results, UCHAR_T energy_num) {
    int i = 0;

    if ((active_results == NULL) && (energy_results == NULL)) {
        log_err ("network scan error");
        return;
    }

    if (active_results != NULL) {
        log_debug ("active scan results");
        for (i = 0; i < active_num; i++) {
            log_debug ("panid: 0x%04x, channel: 0x%02x, rssi: %d, lqi: %d",
                       active_results[i].panid,
                       active_results[i].channel,
                       active_results[i].rssi,
                       active_results[i].lqi);
        }
    }

    if (energy_results != NULL) {
        log_debug ("energy scan results");
        for (i = 0; i < energy_num; i++) {
            log_debug ("channel: 0x%02x, rssi: %d", energy_results[i].channel, energy_results[i].rssi);
        }
    }
}

void __attribute__ ((weak)) test_tuya_user_iot_unactive_gw (void) {
    logErr ( "test_tuya_user_iot_unactive_gw\n");
    tuya_user_iot_unactive_gw ();
}

static void sig_handler (int signo) {
    // note: release resource
    if (SIGPIPE == signo) return;
    system ("sync");
    RunLed (false);
    statusLed (false);
    if (access (CRASH_SCRIPT, F_OK) == 0) {
        system ("chmod +x " CRASH_SCRIPT);
        system (CRASH_SCRIPT);
        logDbg ( "crash_script.sh executed!");
    }

    sleep (1);
    zlog_fatal (pLogCat, "%s(%d), %s process will exit immediately!", strsignal (signo), signo, ProcessName ());
    struct timespec tp = GetClock ();
    int days = tp.tv_sec / (3600 * 24);
    int hours = (tp.tv_sec % (3600 * 24)) / 3600;
    int minutes = (tp.tv_sec % 3600) / 60;
    int seconds = tp.tv_sec % 60;
    zlog_fatal (pLogCat, "The gateway running for %d days %d hours %d minutes %d seconds", days, hours, minutes, seconds);

    if (SIGINT != signo) {
        ExceptionHandler ();
    }
    //    signal (signo, SIG_DFL);
    //    kill(getpid(), signo);

    exit (EXIT_FAILURE);
}

void __attribute__ ((weak)) test_tuya_user_iot_permit_join (void) {
    static bool permit = false;
    int ret = 0;
    logErr ( "test_tuya_user_iot_permit_join");

    permit ^= 1;

    ret = tuya_user_iot_permit_join (permit, 300);
    if (ret != 0) {
        log_err ("tuya_user_iot_permit_join error, ret: %d", ret);
        return;
    }
}

void __attribute__ ((weak)) test_tuya_user_iot_network_scan (void) {
    int ret = 0;
    logErr ( "test_tuya_user_iot_network_scan\n");

    ret = tuya_user_iot_zig_network_scan (0x07FFF800UL, __z3_network_scan_result_cb);
    if (ret != 0) {
        log_err ("tuya_user_iot_zig_network_scan error, ret: %d", ret);
    }
}

void __attribute__ ((weak)) test_tuya_user_iot_channel_get (void) {
    int ret = 0;
    uint8_t channel = 0;
    logErr ( "test_tuya_user_iot_channel_get\n");

    ret = tuya_user_iot_zig_channel_get (&channel);
    if (ret != 0) {
        log_err ("tuya_user_iot_zig_channel_get error, ret: %d", ret);
        return;
    }
    log_debug ("channel: %d", channel);
}

void misc_heartbeat_cfg (void) {
    DEV_DESC_IF_S *dev_if = NULL;
    VOID *iter = NULL;

    logDbg ( "force set heartbeat interval to 180s");

    dev_if = tuya_user_iot_misc_dev_traversal (&iter);
    while (dev_if) {
        if (dev_if->tp != DEV_TP_ZIGBEE) {
            int ret = tuya_user_iot_misc_dev_hb_cfg (dev_if->id, 300, 6, FALSE);
            if (ret != 0) {
                logDbg ( "tuya_user_iot_misc_dev_hb_cfg err: %d", ret);
            }
        }

        dev_if = tuya_user_iot_misc_dev_traversal (&iter);
    }
}


void WEAK signal_init (void) {
    if (signal (SIGBUS, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGBUS!");
    }
    if (signal (SIGFPE, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGFPE!");
    }
    if (signal (SIGHUP, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGHUP!");
    }
    if (signal (SIGILL, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGILL!");
    }
    if (signal (SIGINT, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGINT!");
    }
    signal (SIGPIPE, SIG_IGN);
    if (signal (SIGQUIT, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGQUIT!");
    }
    if (signal (SIGSEGV, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGSEGV!");
    }
    if (signal (SIGABRT, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGABRT!");
    }
    if (signal (SIGSYS, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGSYS!");
    }
    if (signal (SIGTERM, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGTERM!");
    }
    if (signal (SIGTRAP, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGTRAP!");
    }
    if (signal (SIGUSR1, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGUSR1!");
    }
    if (signal (SIGUSR2, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGUSR2!");
    }
    if (signal (SIGVTALRM, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGVTALRM!");
    }
    if (signal (SIGXCPU, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGXCPU!");
    }
    if (signal (SIGXFSZ, sig_handler) == SIG_ERR) {
        logErr ( "Can't catch SIGXFSZ!");
    }
}

void getLicenseFromDisk () {
    char mac[32] = {0};
    char *uuid = calloc (64, sizeof (char));
    char *key = calloc (64, sizeof (char));
    FILE *fd = popen ("rwdisk -r 0x20000 32", "r");
    if (fd == NULL) {
        logErr ( "popen license error!");
        exit (EXIT_FAILURE);
    }
    fgets (mac, 18, fd);
    pclose (fd);
    char *input = mac;
    char *res = input;
    while (*input) {
        if (*input != ':') {
            *res = *input;
            res++;
        }
        input++;
    }
    *res = '\0';
    if (memcmp (g_Mac, mac, 12) != 0) {
        logErr ( "Unauthorized mac address: %s - %s", g_Mac, mac);
        exit (EXIT_FAILURE);
    }

    fd = popen ("rwdisk -r 0x20020 20", "r");
    if (fd == NULL) {
        logErr ( "popen uuid error!");
        exit (EXIT_FAILURE);
    }
    // fgets (uuid, 64, fd);
    fread(uuid, 1, 21, fd);

    pclose (fd);
    uuid[strlen (uuid) - 1] = '\0';

    fd = popen ("rwdisk -r 0x20060 32", "r");
    if (fd == NULL) {
        logErr ( "popen key error!");
        exit (EXIT_FAILURE);
    }
    // fgets (key, 64, fd);
    fread(key, 1, 33, fd);
    pclose (fd);
    logDbg ( "key length: %d char: %c", strlen (key), key[strlen (key) - 2]);
    key[strlen (key) - 1] = '\0';

    pGw = ParseGatewayCfg (XML_OBJECT_MODEL_PATH);
    GatewayElement.szUUID = uuid;
    GatewayElement.szAuthKey = key;
    GatewayElement.pProductId = "qdhgdfnanotjumfv";
    //    GatewayElement.pProductId = "1aechqkxk40bhxeo";
    //    GatewayElement.pProductId = pGw->pProductId;
    logDbg ( "UUID:%s, KEY:%s, PID:%s", GatewayElement.szUUID, GatewayElement.szAuthKey, GatewayElement.pProductId);
    if (!GatewayElement.szUUID || !GatewayElement.szAuthKey) {
        logErr ( "get license failed");
        system ("echo heartbeat > /sys/class/leds/board-heartbeat/trigger");
        exit (EXIT_FAILURE);
    }

#if 0
    logDbg ( COLOR_YELLOW"rwdisk -r 0x200E0 6"COLOR_CLEAR);
    fd = popen ("rwdisk -r 0x200E0 7", "r");
    if (fd == NULL) {
        logErr ( "popen gateway active error!");
        exit (EXIT_FAILURE);
    }
    char szActive[7] = {0};
    fgets (szActive, 6, fd);
    pclose (fd);
    logDbg ( "szActive:%s", szActive);
    if (memcmp (szActive, "active", 6) == 0) {
        // g_bActive = true;
        logDbg ( COLOR_BOLD"gateway is active!"COLOR_CLEAR);
    } else {
        // g_bActive = false;
        logDbg ( COLOR_BOLD"gateway is inactive!"COLOR_CLEAR);
    }
#endif

    system ("echo none > /sys/class/leds/board-heartbeat/trigger");
}

void knxAutoAck (void) {
    uint8_t stopAck[1] = {0x0E};
    uint8_t autoAck[4] = {0xF1, 0xFF, 0xCC, 0x00};
    uint8_t startAck[1] = {0x0F};
    uint8_t resetReq[1] = {1};

    Serial_t *pSerial = SerialInit ();
    if (!pSerial) {
        logErr ( "SerialInit failed!");
        return;
    }
    int ret = pSerial->openExt (pSerial, "/dev/ttymxc5", 19200, 8, 'e', 1);
    if (ret != 0) {
        logErr ( "open serial \"/dev/ttymxc5\" failed!");
        return;
    }
    usleep (1000 * 100);

    pSerial->send (pSerial, resetReq, 1);
    usleep (1000 * 500);

    pSerial->send (pSerial, stopAck, 1);
    usleep (1000 * 100);

    pSerial->send (pSerial, autoAck, 4);
    usleep (1000 * 100);
    pSerial->send (pSerial, autoAck, 4);
    usleep (1000 * 100);

    pSerial->send (pSerial, startAck, 1);
    usleep (1000 * 100);

    pSerial->close (pSerial);

    SerialDestroy (pSerial);
}

void cmdHandle (int argc, char *argv[]) {
    int opt;
    if (argc <= 0) return;
    const char *const short_options = "hvu::t:";
    const struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"upgrade", optional_argument, NULL, 'u'},
        {"test", required_argument, NULL, 't'},
        {NULL, 0, NULL, 0}};

    char usage[] = "Usage: smarthome [-h|--help] [-v|--version] [-u|--upgrade] [-t|--test test_item]\n"
                   "  -h, --help          Show this help message and exit\n"
                   "  -v, --version       Show version information and exit\n"
                   "  -u, --upgrade[=x.y.z] Upgrade smarthome to version x.y.z\n"
                   "  -t, --test          Run test item, e.g. 'knx', 'modbus', 'zigbee', 'di'\n";

    char *pNext = NULL;
    while ((opt = getopt_long (argc, (char *const *) argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                printf ("%s\n", usage);
                exit (EXIT_SUCCESS);
                break;
            case 'v':
                printf ("version:%s-%s, branch:%s, commit time:%s, author:%s, build type: %s!", FW_VERSION_STR, FW_METADATA_STR, FW_GIT_BRANCH, FW_GIT_DATE, FW_GIT_AUTHOR, FW_BUILD_TYPE);
                exit (EXIT_SUCCESS);
            case 'u':
                // bXmlInit = true;
                // xmlInitParser ();
                // UpdateKnxVentSpeedCtrlGrp (XML_OBJECT_MODEL_PATH, "11/2/16", "12/2/16");
                // UpdateKnxDimmerGrp (XML_OBJECT_MODEL_PATH, 501, "1/0/81", "2/0/81", "5/0/1", "6/0/1");
                // UpdateKnxDimmerGrp (XML_OBJECT_MODEL_PATH, 502, "1/0/82", "2/0/82", "5/0/2", "6/0/2");

                break;
            case 't':
                pNext = optarg;
                if (!pNext) {
                    printf ("%s\n", usage);
                    exit (EXIT_SUCCESS);
                }

                if (memcmp (pNext, "xml", 3) == 0) {
                    // bXmlInit = true;
                    // xmlInitParser ();
                    // UpdateKnxVentSpeedCtrlGrp (XML_OBJECT_MODEL_PATH, "11/2/16", "12/2/16");
                    // UpdateKnxDimmerGrp (XML_OBJECT_MODEL_PATH, 501, "1/0/81", "2/0/81", "5/0/1", "6/0/1");
                    // UpdateKnxDimmerGrp (XML_OBJECT_MODEL_PATH, 502, "1/0/82", "2/0/82", "5/0/2", "6/0/2");
                    break;
                } else {
                    printf ("option -t only support xml\n");
                    exit (EXIT_SUCCESS);
                }

                return;
            default:
                printf ("%s\n", usage);
                exit (EXIT_SUCCESS);
        }
    }
}

int main (int argc, char *argv[]) {
    int ret = 0;
    system ("echo heartbeat > /sys/class/leds/chip-heartbeat/trigger");
    system ("echo none > /sys/class/leds/board-heartbeat/trigger");

    // AntiDebugCheck();    /* Stopped(SIGCHLD) */
    SingleInstance ();
    PreStartup ();

    int rc = zlog_init ("/opt/smarthome/config/zlog.conf");
    if (rc != 0) {
        printf ("zlog init failed: %d\n", rc);
        exit (EXIT_FAILURE);
    }

    pLogCat = zlog_get_category ("log");
    if (!pLogCat) {
        printf ("get general log category failed!\n");
        zlog_fini ();
        exit (EXIT_FAILURE);
    }
    cmdHandle(argc, argv);

    PrintLogo ();
    zlog_info (pLogCat, "[Firmware] V%s-%s-%s Branch=%s CommitTime=%s Author=%s BuildTime=%s %s", 
        FW_VERSION_STR, FW_METADATA_STR, FW_BUILD_TYPE, FW_GIT_BRANCH, FW_GIT_DATE, FW_GIT_AUTHOR, __DATE__, __TIME__);

    signal_init ();
    ty_gw_attr_s gw_attr = {
        .storage_path = (char *) "/opt/smarthome/tmp",
        .cache_path = (char *) "/opt/smarthome/tmp",
        .ver = (char *) FW_VERSION_STR,
        .log_level = TY_LOG_ERR};

    ret = ReadEngineeringMode ();
    if (ret < 0) {
        gw_attr.is_engr = 0;
    } else {
        gw_attr.is_engr = 1;
    }

    ty_gw_infra_cbs_s gw_infra_cbs = {
        .get_uuid_authkey_cb = _iot_get_uuid_authkey_cb,
        .get_product_key_cb = _iot_get_product_key_cb,
        .gw_fetch_local_log_cb = _gw_fetch_local_log_cb,
        .gw_reboot_cb = _gw_reboot_cb,
        .gw_reset_cb = _gw_reset_cb,
        .gw_upgrade_cb = _gw_upgrade_cb,
        .gw_active_status_changed_cb = _gw_active_status_changed_cb,
        .gw_online_status_changed_cb = _gw_online_status_changed_cb,
    };

    ty_dev_cmd_cbs_s dev_cmd_cbs = {
        .dev_obj_cmd_cb = _dev_obj_cmd_cb,
        .dev_raw_cmd_cb = _dev_raw_cmd_cb,
    };

    ty_misc_dev_cbs_s misc_dev_cbs = {
        .misc_dev_add_cb = _misc_dev_add_cb,
        .misc_dev_del_cb = _misc_dev_del_cb,
        .misc_dev_bind_ifm_cb = _misc_dev_bind_ifm_cb,
        .misc_dev_upgrade_cb = _misc_dev_upgrade_cb,
        .misc_dev_reset_cb = _misc_dev_reset_cb,
        .misc_dev_heartbeat_cb = _misc_dev_hb_cb,
    };

    TY_Z3_DEV_CBS_S z3_dev_cbs = {
        .join = __my_z3_dev_join,
        .leave = __my_z3_dev_leave,
        .report = __my_z3_dev_report,
        .notify = __my_z3_dev_notify,
        .upgrade_end = __my_z3_dev_upgrade_end,
        .version = __my_z3_dev_version,
    };
    if (!bXmlInit) {
        xmlInitParser ();
    }

    memset (g_Mac, 0, sizeof (g_Mac));
    GetMac (g_Mac);

#if 0
    getLicenseFromDisk ();
#else
    pGw = ParseGatewayCfg (XML_OBJECT_MODEL_PATH);
    GetLicenses(g_Mac, &GatewayElement);
    GatewayElement.pProductId = pGw->pProductId;
#endif


    // if (access ("/opt/smarthome/tmp/16", F_OK) != 0) {
    //     system ("touch /opt/smarthome/tmp/16");
    //     UpdateKnxVentSpeedCtrlGrp (XML_OBJECT_MODEL_PATH, "11/2/16", "12/2/16");
    //     UpdateKnxDimmerGrp (XML_OBJECT_MODEL_PATH, 501, "1/0/81", "2/0/81", "5/0/1", "6/0/1");
    //     UpdateKnxDimmerGrp (XML_OBJECT_MODEL_PATH, 502, "1/0/82", "2/0/82", "5/0/2", "6/0/2");
    // }

    thpool = thpool_init (15);
    if (thpool == NULL) {
        logErr ( "thpool_init failed\n");
        exit (EXIT_FAILURE);
    }
    pPosQueue = queue_create_limited (10);
    if (!pPosQueue) {
        logErr ( "create pPosQueue fail!");
        exit (EXIT_FAILURE);
    }
    pBindPosQueue = queue_create_limited (200);
    if (!pBindPosQueue) {
        logErr ( "create pPosQueue fail!");
        exit (EXIT_FAILURE);
    }

    /* 创建链表 */
    pHwModelList = listCreate ();
    if (!pHwModelList) {
        logErr ( "create HwModel list fail!\n");
        exit (EXIT_FAILURE);
    }

    pKnxScene = listCreate ();
    if (!pKnxScene) {
        logErr ( "create KNX scene list fail!\n");
        exit (EXIT_FAILURE);
    }

    pZ3List = listCreate ();
    if (!pZ3List) {
        logErr ( "create Z3 list fail!\n");
        exit (EXIT_FAILURE);
    }

    pMbusDevList = listCreate ();
    if (!pMbusDevList) {
        logErr ( "create pMbusDevList fail!");
        exit (EXIT_FAILURE);
    }

    pKnxDevList = listCreate ();
    if (!pKnxDevList) {
        logErr ( "create pKnxDevList fail!");
        exit (EXIT_FAILURE);
    }

    pRf433List = listCreate ();
    if (!pRf433List) {
        logErr ( "create RF433 list fail!\n");
        exit (EXIT_FAILURE);
    }

    pDioDevList = listCreate ();
    if (!pDioDevList) {
        logErr ( "create DIO list fail!\n");
        exit (EXIT_FAILURE);
    }

    pGroupList = listCreate ();
    if (!pGroupList) {
        logErr ( "create Group list fail!\n");
        exit (EXIT_FAILURE);
    }

    /* 解析配置文件 */
    if (access (XML_OBJECT_MODEL_PATH, F_OK | R_OK) != 0) {
        logErr ( "device config file: \"%s\" not be exist!", XML_OBJECT_MODEL_PATH);
        exit (EXIT_FAILURE);
    }

    ret = ParseZ3Cfg (XML_OBJECT_MODEL_PATH, pZ3List);
    if (ret == 0) {
        g_HasSysDev.bZ3 = true;
    }
    ret = ParseMbusAcdCfg (XML_OBJECT_MODEL_PATH, pMbusDevList);
    if (ret == 0) {
        g_HasSysDev.bMbusAcd = true;
    }
    ret = ParseMbusVentCfg (XML_OBJECT_MODEL_PATH, pMbusDevList);
    if (ret == 0) {
        g_HasSysDev.bMbusVent = true;
    }
    ret = ParseMbusHeatCfg (XML_OBJECT_MODEL_PATH, pMbusDevList);
    if (ret == 0) {
        g_HasSysDev.bMbusHeat = true;
    }
    ret = ParseMbusAqiCfg (XML_OBJECT_MODEL_PATH, pMbusDevList);
    if (ret == 0) {
        g_HasSysDev.bMbusAqi = true;
    }
    ret = ParseMbusPanelCfg (XML_OBJECT_MODEL_PATH, pMbusDevList);
    if (ret == 0) {
        g_HasSysDev.bMbusAqi = true;
    }
    ret = ParseKnxAcdCfg (XML_OBJECT_MODEL_PATH, pKnxDevList);
    if (ret == 0) {
        g_HasSysDev.bKnxAcd = true;
    }
    ret = ParseKnxVentCfg (XML_OBJECT_MODEL_PATH, pKnxDevList);
    if (ret == 0) {
        g_HasSysDev.bKnxVent = true;
    }
    ret = ParseKnxHeatCfg (XML_OBJECT_MODEL_PATH, pKnxDevList);
    if (ret == 0) {
        g_HasSysDev.bKnxHeat = true;
    }
    ParseKnxSceneCfg (XML_OBJECT_MODEL_PATH, pKnxScene);
    ParseKnxPanelCfg (XML_OBJECT_MODEL_PATH, pKnxDevList);
    ParseKnxDimmerCfg (XML_OBJECT_MODEL_PATH, pKnxDevList);
    ParseKnxSmartPanelCfg (XML_OBJECT_MODEL_PATH, pKnxDevList);
    ParseKnxCurtainCfg (XML_OBJECT_MODEL_PATH, pKnxDevList);
    ret = ParseMbusCurtainCfg (XML_OBJECT_MODEL_PATH, pMbusDevList);
    if (ret == 0) {
        g_HasSysDev.bKnxCurtain = true;
    }
    ret = ParseDiCfg (XML_OBJECT_MODEL_PATH, pDioDevList);
    if (ret == 0) {
        g_HasSysDev.bDi = true;
    }
    ret = ParseDoCfg (XML_OBJECT_MODEL_PATH, pDioDevList);
    if (ret == 0) {
        g_HasSysDev.bDo = true;
    }
    ParseGroupCfg (XML_OBJECT_MODEL_PATH, pAcdGroupXPath, pGroupList);
    ParseGroupCfg (XML_OBJECT_MODEL_PATH, pCurtainGroupXPath, pGroupList);

    ParseZigbeeCfg (XML_OBJECT_MODEL_PATH, &bZigbeeModuleEnabled);
    if (bZigbeeModuleEnabled) {
        gw_attr.tty_device = (char *) "/dev/ttymxc6";
        gw_attr.tty_baudrate = 115200;
    }
    ParseKnxCfg (XML_OBJECT_MODEL_PATH, &bKnxAutoAcKEnabled);

    associateKnxDev ();
    associateModbusDev ();
    ModbusDevGroup ();

    /* 载入插件 */
    LoadZ3Plugin (pZ3List);
    LoadModbusPlugin (pMbusDevList);
    LoadKnxPlugin (pKnxDevList);

    if (bKnxAutoAcKEnabled) {
        knxAutoAck ();
    }
    /* 初始化KNX */
    KnxInit (thpool);
    ModbusInit (thpool, pMbusDevList);

    /* 初始化涂鸦接口 */
    thpool_add_work (thpool, key_scan, NULL);
    thpool_add_work (thpool, flashLight, NULL);
    if (g_HasSysDev.bDi) {
        thpool_add_work (thpool, di_scan, NULL);
    }
    thpool_add_work (thpool, fakeServer, NULL);
    thpool_add_work (thpool, RunLedThread, NULL);
    //    thpool_add_work (thpool, multicast_rcv, NULL);
    //    thpool_add_work (thpool, broadcast_rcv, NULL);
    thpool_add_work (thpool, ReportPosThread, pPosQueue);
    thpool_add_work (thpool, ReportBindPosThread, pBindPosQueue);

    tuya_user_iot_pre_init ();

    ret = tuya_user_iot_reg_dev_cmd_cb (&dev_cmd_cbs);
    if (ret != 0) {
        logErr ( "tuya_user_iot_reg_dev_cmd_cb failed, ret: %d", ret);
        return ret;
    }

    ret = tuya_user_iot_reg_misc_dev_cb (&misc_dev_cbs);
    if (ret != 0) {
        logErr ( "tuya_user_iot_reg_misc_dev_cb failed, ret: %d", ret);
        return ret;
    }

    ret = tuya_user_iot_zig_mgr_register (&my_z3_devlist, &z3_dev_cbs);
    if (ret != 0) {
        logErr ( "tuya_user_iot_zig_mgr_register err: %d", ret);
        return ret;
    }

    ret = tuya_user_iot_init (&gw_attr, &gw_infra_cbs);
    if (ret != 0) {
        logErr ( "tuya_user_iot_init failed, ret=%d", ret);
        return ret;
    }
    misc_heartbeat_cfg ();

    Mbus_ForceSyncDevStatus (thpool);
    thpool_wait (thpool);
    thpool_destroy (thpool);
    xmlCleanupParser ();
    logErr ( "Don't come here!");
    zlog_fini ();

    exit (EXIT_FAILURE);
}

void key_scan (void *param) {
    (void) param;
    struct input_event inputevent;
    int press = 0;
    int release = 0;

    int fd = open (KEY_PATH, O_RDWR);
    if (fd < 0) {
        logErr ( "Can't open file " KEY_PATH);
        return;
    }

    for (;;) {
        int err = read (fd, &inputevent, sizeof (inputevent));
        if (err > 0) {
            if (inputevent.type == EV_KEY) {
                if (inputevent.code < BTN_MISC) {
                    if (inputevent.code == 171) {
                        if (inputevent.value >= 1) {
                            press++;
                            if (press == 250) {
                                statusLed (true);
                            }
                        } else if (inputevent.value == 0) {
                            if (press > 250) {
                                tuya_user_iot_reset_gw ();
                            } else {
                                release++;
                                if (release == 5) {
                                    int ret = EngineeringMode ();// 已经是工程模式了
                                    if (ret != 0) {
                                        BusinessMode ();// 工程模式切换为业务模式
                                    }
                                    _gw_reboot_cb ();
                                }
                            }
                            press = 0;
                        }
                    }
                }
            }
        }
    }

    close (fd);

    logErr ( "key_scan thread crashed");
}

__attribute__ ((unused)) void device_join (void *param) {
    (void) param;
    struct timeval time;
    time.tv_sec = 180;
    time.tv_usec = 0;

    logDbg ( "misc device join");
    system ("echo heartbeat > /sys/class/leds/chip-heartbeat/trigger");
    tuya_user_iot_permit_join (true, 180);

    select (0, NULL, NULL, NULL, &time);
    logDbg ( "misc device join timeout");
    system ("echo none > /sys/class/leds/chip-heartbeat/trigger");
}

void flashLight (void *param) {
    (void) param;
    struct timespec tim, tim2;
    tim.tv_sec = 0;
    tim.tv_nsec = (1000 * 1000 * 250);

    while (true) {
        if (engineer == 1) {
            statusLed (true);
            nanosleep (&tim, &tim2);
            statusLed (false);
        }

        nanosleep (&tim, &tim2);
    }
}

int LoadZ3Plugin (list_t *pList) {
    if (!pList) return -1;

    listIter it;
    listNode *pNode = NULL;
    void *pvHandle;
    char path[128] = {0};

    listRewind (pList, &it);
    while ((pNode = listNext (&it)) != NULL) {
        Z3Dev_t *pZ3Dev = (Z3Dev_t *) pNode->value;
        strcpy (path, DEVICE_LIBRARY_PATH);
        strncat (path, pZ3Dev->element.szPlugin, MIN (sizeof (path) - strlen (path) - 1, strlen (pZ3Dev->element.szPlugin)));
        if (access (path, F_OK | X_OK) != 0) {
            logErr ( COLOR_RED "Z3 plugin: \"%s\" not be found!\n" COLOR_CLEAR, path);
            continue;
        }
        pvHandle = dlopen (path, RTLD_LAZY);
        if (!pvHandle) {
            logErr ( "dlopen - %s", dlerror ());
            continue;
        }
        dlerror ();
        char dlsymbol[SHARED_LIBRARY_LENGTH + PREFIX_SIZE] = {0};
        memset (dlsymbol, 0, sizeof (dlsymbol));
        strncpy (dlsymbol, pZ3Dev->element.szPlugin, MIN (SHARED_LIBRARY_LENGTH, strlen (pZ3Dev->element.szPlugin)));
        char *ptr = strchr (dlsymbol, '.');
        if (!ptr) {
            logErr ( "Invalid plugin name: %s", pZ3Dev->element.szPlugin);
            continue;
        }
        *ptr = '\0';
        pZ3Dev->pZ3 = dlsym (pvHandle, dlsymbol);
        if (!pZ3Dev->pZ3) {
            dlclose (pvHandle);
            logErr ( "dlsym - %s", dlerror ());
            continue;
        }
    }

    return 0;
}

int LoadModbusPlugin (list_t *pList) {
    if (!pList) return -1;

    listIter it;
    listNode *pNode = NULL;
    void *pvHandle;
    char path[128] = {0};

    listRewind (pList, &it);
    while ((pNode = listNext (&it)) != NULL) {
        MbusBase_t **ppBase = (MbusBase_t **) pNode->value;
        if (!(*ppBase)->element.szPlugin) {
            logDbg ( "mbus(%s) plugin name does not exist!", (*ppBase)->element.szName);
            continue;
        }
#if 0
        if (!(*ppBase)->element.pProductId) {
            zlog_debug(pLogCat, "The mbus(%s) is input device, plugin not be load!", (*ppBase)->element.szName);
            continue;
        }
#endif
        strcpy (path, DEVICE_LIBRARY_PATH);
        logDbg ( "pBase->element.szPlugin=%s", (*ppBase)->element.szPlugin);
        strncat (path, (*ppBase)->element.szPlugin, MIN (sizeof (path) - strlen (path) - 1, strlen ((*ppBase)->element.szPlugin)));
        if (access (path, F_OK | X_OK) != 0) {
            logErr ( "mbus plugin: \"%s\" not be found!", path);
            continue;
        }
        pvHandle = dlopen (path, RTLD_LAZY);
        if (!pvHandle) {
            logErr ( "dlopen - %s", dlerror ());
            continue;
        }
        logDbg ( "Load mbus plugin \"%s\" success!", path);
        dlerror ();
        char dlsymbol[SHARED_LIBRARY_LENGTH + PREFIX_SIZE] = {0};
        memset (dlsymbol, 0, sizeof (dlsymbol));
        strncpy (dlsymbol, (*ppBase)->element.szPlugin, MIN (SHARED_LIBRARY_LENGTH, strlen ((*ppBase)->element.szPlugin)));
        char *ptr = strchr (dlsymbol, '.');
        if (!ptr) {
            logErr ( "Invalid plugin name: %s", (*ppBase)->element.szPlugin);
            continue;
        }
        *ptr = '\0';
        (*ppBase)->pIf = dlsym (pvHandle, dlsymbol);
        if (!(*ppBase)->pIf) {
            dlclose (pvHandle);
            logErr ( COLOR_RED "dlsym(%s) fail - %s" COLOR_CLEAR, dlsymbol, dlerror ());
            sleep (1);
            continue;
        } else {
            logDbg ( "mbus plugin symbol \"%s\" be found!", dlsymbol);
            if ((*ppBase)->pIf->init) {
                (*ppBase)->pIf->init (NULL);
            }
            if ((!(*ppBase)->pIf->build && !(*ppBase)->pIf->buildExt) || (!(*ppBase)->pIf->parse && !(*ppBase)->pIf->parseExt)) {
                dlclose (pvHandle);
                logErr ( COLOR_RED "mbus plugin interface does not exist!" COLOR_CLEAR);
            }
        }
    }

    return 0;
}

int LoadKnxPlugin (list_t *pList) {
    if (!pList) {
        logErr ( "LoadKnxPlugin invalid parameter");
        return -1;
    }

    listIter it;
    listNode *pNode = NULL;
    void *pvHandle;
    char path[128] = {0};

    listRewind (pList, &it);
    while ((pNode = listNext (&it)) != NULL) {
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if (!pKnx->element.szPlugin) {
            logDbg ( "The knx %s plugin name does not exist!", pKnx->element.szName);
            continue;
        }
        strcpy (path, DEVICE_LIBRARY_PATH);
        strncat (path, pKnx->element.szPlugin, MIN (sizeof (path) - strlen (path) - 1, strlen (pKnx->element.szPlugin)));
        if (access (path, F_OK | X_OK) != 0) {
            logErr ( "knx plugin:\"%s\" not be found!", path);
            continue;
        }

        pvHandle = dlopen (path, RTLD_LAZY);
        if (!pvHandle) {
            logErr ( "dlopen - %s", dlerror ());
            continue;
        }
        logDbg ( "load knx plugin \"%s\" success!", path);
        dlerror ();
        char dlsymbol[SHARED_LIBRARY_LENGTH + PREFIX_SIZE] = {0};
        memset (dlsymbol, 0, sizeof (dlsymbol));
        strncpy (dlsymbol, pKnx->element.szPlugin, MIN (SHARED_LIBRARY_LENGTH, strlen (pKnx->element.szPlugin)));
        char *ptr = strchr (dlsymbol, '.');
        if (!ptr) {
            logErr ( "Invalid plugin name: %s", pKnx->element.szPlugin);
            continue;
        }
        *ptr = '\0';

        pKnx->pIf = dlsym (pvHandle, dlsymbol);
        if (!pKnx->pIf) {
            dlclose (pvHandle);
            logErr ( "dlsym - %s", dlerror ());
        } else {
            logDbg ( "knx plugin symbol \"%s\" be found!", dlsymbol);
            if (!pKnx->pIf->control || !pKnx->pIf->report) {
                dlclose (pvHandle);
                logErr ( "knx plugin interface does not exist!");
            }
        }
    }

    return 0;
}

int Modbus_DevBind (list_t *pLst, bool permit) {
    if (!pLst) {
        logErr ( "Modbus_DevBind invalid parameter");
        return -1;
    }
    listIter iter;
    listNode *pNode;
    listRewind (pLst, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        MbusBase_t **ppBase = (MbusBase_t **) pNode->value;
        if (!(*ppBase)->element.bMaster) {// 非逻辑主设备
            logDbg ( "mbus device bind fail, %s %s is slave!", (*ppBase)->element.szName, (*ppBase)->element.pPrimaryKey);
            continue;
        }

        if ((*ppBase)->bGroupMember) {// 组成员
            logDbg ( "mbus device bind fail, %s %s is group member!", (*ppBase)->element.szName, (*ppBase)->element.pPrimaryKey);
            continue;
        }
        DEV_DESC_IF_S *dev_if = tuya_user_iot_misc_dev_desc_get ((*ppBase)->element.pPrimaryKey);
        if (!dev_if) {
            if (permit) {
                int ret = tuya_user_iot_misc_dev_bind (DEV_TP_485, 0x2222, (*ppBase)->element.pPrimaryKey, (*ppBase)->element.pProductId, "1.0.0");
                if (ret != OPRT_OK) {
                    logErr ( COLOR_RED "mbus %s bind fail! %d" COLOR_CLEAR, (*ppBase)->element.szName, ret);
                    continue;
                }
                logDbg ( "mbus %s bind sucess! device id=%s, product id=%s", (*ppBase)->element.szName, (*ppBase)->element.pPrimaryKey, (*ppBase)->element.pProductId);
            } else {
                logDbg ( "mbus %s bind fail! assert not here!", (*ppBase)->element.szName);
            }
        }
    }

    return 0;
}

int Modbus_DevUnbind (list_t *pLst, const char *pDevId) {
    if (!pLst) {
        logErr ( "Knx_DevUnbind: pLst==NULL\n");
        return -1;
    }
    (void) pDevId;
    listIter iter;
    listNode *pNode;
    listRewind (pLst, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        MbusBase_t **ppBase = (MbusBase_t **) pNode->value;
        if (!(*ppBase)->element.bMaster) continue;
        DEV_DESC_IF_S *dev_if = tuya_user_iot_misc_dev_desc_get ((*ppBase)->element.pPrimaryKey);
        if (!dev_if) {
            int ret = tuya_user_iot_misc_dev_unbind ((*ppBase)->element.pPrimaryKey);
            if (ret != OPRT_OK) {
                logErr ( "knx %s unbind fail! %d", (*ppBase)->element.szName, ret);
                continue;
            }
            logDbg ( "knx %s unbind sucess! device id=%s, product id=%s", (*ppBase)->element.szName, (*ppBase)->element.pPrimaryKey, (*ppBase)->element.pProductId);
        }
    }

    return -1;
}

int Knx_DevBind (list_t *pLst, bool permit) {
    if (!pLst) {
        logErr ( "Knx_DevBind invalid parameter");
        return -1;
    }
    if (!permit) {
        logDbg ( "Knx_DevBind permit join");
        return 0;
    }

    listIter iter;
    listNode *pNode;

    listRewind (pLst, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if (!pKnx->element.bMaster) {// 非逻辑主设备
            logTrc ( "knx device bind fail, %s %s is slave!", pKnx->element.szName, pKnx->element.pPrimaryKey);
            continue;
        }

        DEV_DESC_IF_S *dev_if = tuya_user_iot_misc_dev_desc_get (pKnx->element.pPrimaryKey);
        if (dev_if) continue;
        if (tuya_user_iot_misc_dev_bind (DEV_TP_KNX, 0x2222, pKnx->element.pPrimaryKey, pKnx->element.pProductId, "1.0.0") != OPRT_OK) {
            logErr ( "knx %s bind fail!", pKnx->element.szName);
            continue;
        }
        logDbg ( "knx %s bind sucess! device id=%s, product id=%s", pKnx->element.szName, pKnx->element.pPrimaryKey, pKnx->element.pProductId);
    }

    return 0;
}

int DIO_DevBind (list_t *pLst, bool permit) {
    if (!pLst) {
        logErr ( "DIO_DevBind invalid parameter");
        return -1;
    }
    listIter iter;
    listNode *pNode;

    listRewind (pLst, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        DeviceElement_t *pElement = (DeviceElement_t *) pNode->value;
        if ((pElement->szName[0] != 'D') || (pElement->szName[1] != 'I')) continue;
        DiDev_t *pDi = (DiDev_t *) pNode->value;

        DEV_DESC_IF_S *dev_if = tuya_user_iot_misc_dev_desc_get (pDi->element.pPrimaryKey);
        if (!dev_if) {
            if (permit) {
                int ret = tuya_user_iot_misc_dev_bind (DEV_TP_USR1, 0x2222, pDi->element.pPrimaryKey,
                                                       pDi->element.pProductId, "1.0.0");
                if (ret != OPRT_OK) {
                    logErr ( "%s%d bind fail! %d", pDi->element.szName, pDi->channel, ret);
                    continue;
                }
                logDbg ( "%s%d bind sucess! device id=%s, product id=%s",
                            pDi->element.szName, pDi->channel, pDi->element.pPrimaryKey, pDi->element.pProductId);
            }
        } else {
            tuya_user_iot_misc_dev_hb_cfg (pDi->element.pPrimaryKey, 300, 3, false);
        }
    }

    return 0;
}

int Knx_DevUnbind (list_t *pLst, const char *pDevId) {
    if (!pLst) {
        logErr ( "Knx_DevUnbind invalid parameter");
        return -1;
    }
    (void) pDevId;
    listIter iter;
    listNode *pNode;

    listRewind (pLst, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) {
            continue;
        }
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;

        DEV_DESC_IF_S *dev_if = tuya_user_iot_misc_dev_desc_get (pKnx->element.pPrimaryKey);
        if (!dev_if) {
            int ret = tuya_user_iot_misc_dev_unbind (pKnx->element.pPrimaryKey);
            if (ret != OPRT_OK) {
                logErr ( "knx %s unbind fail! %d", pKnx->element.szName, ret);
                continue;
            }
            logDbg ( "knx %s unbind sucess! device id=%s, product id=%s", pKnx->element.szName,
                        pKnx->element.pPrimaryKey, pKnx->element.pProductId);
        }
    }

    return -1;
}

int DIO_DevUnbind (list_t *pLst, const char *pDevId) {
    if (!pLst) {
        logErr ( "Knx_DevUnbind invalid parameter");
        return -1;
    }
    (void) pDevId;
    listIter iter;
    listNode *pNode;

    listRewind (pLst, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) {
            continue;
        }
        DeviceElement_t *pElement = (DeviceElement_t *) pNode->value;
        if ((pElement->szName[0] != 'D') || (pElement->szName[1] != 'I')) continue;
        DiDev_t *pDi = (DiDev_t *) pNode->value;

        DEV_DESC_IF_S *dev_if = tuya_user_iot_misc_dev_desc_get (pDi->element.pPrimaryKey);
        if (!dev_if) {
            int ret = tuya_user_iot_misc_dev_unbind (pDi->element.pPrimaryKey);
            if (ret != OPRT_OK) {
                logErr ( "knx %s unbind fail! %d", pDi->element.szName, ret);
                continue;
            }
            logDbg ( "knx %s unbind sucess! device id=%s, product id=%s", pDi->element.szName,
                        pDi->element.pPrimaryKey, pDi->element.pProductId);
        }
    }

    return -1;
}

/* note: 目前只支持新风设备 */
void associateKnxDev (void) {
    listIter iter;
    listNode *pKnxNode;

    listRewind (pKnxDevList, &iter);
    while ((pKnxNode = listNext (&iter)) != NULL) {
        if (!pKnxNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pKnxNode->value;
        if (!pKnx->element.bMaster) continue;
        if (pKnx->element.primaryType != KNX_DEVICE_VENTILATION) continue;

        listSetMatchMethod (pMbusDevList, ModbusDevIdMatch);
        listNode *pMbusNode = listSearchKey (pMbusDevList, (char *) pKnx->element.pForeignKey);
        if (!pMbusNode) continue;

        pKnx->element.pvForeignKey = pMbusNode->value;
        MbusBase_t **ppBase = (MbusBase_t **) pMbusNode->value;
        (*ppBase)->element.pvForeignKey = pKnxNode->value;
        zlog_info (pLogCat, "associate knx device %s to mbus device %s success.", pKnx->element.pPrimaryKey, (*ppBase)->element.pPrimaryKey);
    }
}

void associateModbusDev (void) {
    listIter iter;
    listNode *pModbusNode;

    listRewind (pMbusDevList, &iter);
    while ((pModbusNode = listNext (&iter)) != NULL) {
        if (!pModbusNode->value) continue;
        MbusBase_t **ppBase = (MbusBase_t **) pModbusNode->value;
        if (!(*ppBase)->element.bMaster) continue;    // 从设备只能被主设备绑定
        if (!(*ppBase)->element.pForeignKey) continue;//

        listSetMatchMethod (pKnxDevList, KnxDevIdMatch);
        listNode *pKnxNode = listSearchKey (pKnxDevList, (char *) (*ppBase)->element.pForeignKey);
        if (!pKnxNode) continue;
        free ((char *) (*ppBase)->element.pForeignKey);
        (*ppBase)->element.pvForeignKey = pKnxNode->value;

        KnxBase_t *pBase = (KnxBase_t *) pKnxNode->value;
        pBase->element.pvForeignKey = pModbusNode->value;
        zlog_info (pLogCat, "associate mbus device %s to knx device %s success!", (*ppBase)->element.pPrimaryKey, pBase->element.pPrimaryKey);
    }
}

void ModbusDevGroup (void) {
    listIter iter;
    listNode *pGroupNode;

    listRewind (pGroupList, &iter);
    while ((pGroupNode = listNext (&iter)) != NULL) {
        if (!pGroupNode->value) continue;
        Group_t *pGroup = (Group_t *) pGroupNode->value;

        listSetMatchMethod (pMbusDevList, ModbusDevIdMatch);
        listNode *pMbusNodeOwner = listSearchKey (pMbusDevList, (char *) pGroup->pPrimaryKey);
        if (!pMbusNodeOwner) break;
        MbusBase_t **ppBaseOwner = (MbusBase_t **) pMbusNodeOwner->value;
        logDbg ( "group owner:%s", (*ppBaseOwner)->element.pPrimaryKey);
        while (*pGroup->ppForeignKey) {
            logDbg ( "group member:%s", *pGroup->ppForeignKey);
            listSetMatchMethod (pMbusDevList, ModbusDevIdMatch);
            listNode *pMbusNodeMember = listSearchKey (pMbusDevList, (char *) *pGroup->ppForeignKey);
            if (!pMbusNodeMember) continue;
            while ((*ppBaseOwner)->pNext) {
                ppBaseOwner = &(*ppBaseOwner)->pNext;
                logDbg ( "ppBaseOwner:%p", ppBaseOwner);
            }
            (*ppBaseOwner)->pNext = *(MbusBase_t **) pMbusNodeMember->value;
            pGroup->ppForeignKey++;
        }
    }
    // todo: free group list
}

int EngineeringMode (void) {
    int fd = open (ENGINEERING_MODE_PATH, O_RDWR | O_TRUNC | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        logErr ( "open %s error!", ENGINEERING_MODE_PATH);
        return -1;
    }
    close (fd);

    return 0;
}

int BusinessMode (void) {
    int ret = remove (ENGINEERING_MODE_PATH);
    if (ret < 0) {
        logErr ( "remove engineering mode file failed!");
    }

    return ret;
}

int ReadEngineeringMode (void) {
    int fd = open (ENGINEERING_MODE_PATH, O_RDONLY, 0666);
    if (fd < 0) {
        logDbg ( "business mode is on!");
        return -1;
    } else {
        logDbg ( "engineering mode is on!");
    }
    close (fd);

    return 0;
}

void statusLed (bool bOn) {
    if (bOn) {
        system ("echo heartbeat > /sys/class/leds/board-heartbeat/trigger");
    } else {
        system ("echo none > /sys/class/leds/board-heartbeat/trigger");
    }
}

void RunLed (bool bOn) {
    if (bOn) {
        system ("echo heartbeat > /sys/class/leds/chip-heartbeat/trigger");
    } else {
        system ("echo none > /sys/class/leds/chip-heartbeat/trigger");
    }
}

char *ProcessName (void) {
    char buf[1024] = {'\0'};
    static char *pPath = NULL;

    if (pPath) {
        return pPath;
    }

    int ret = readlink ("/proc/self/exe", buf, sizeof (buf));
    if (ret < 0 || ret > (int) sizeof (buf)) {
        logErr ( "readlink error!");
        return NULL;
    }

    char *pName = strrchr (buf, '/');
    pPath = (char *) calloc (1, strlen (pName) + 1);
    if (pPath) {
        strcpy (pPath, pName + 1);
    }

    return pPath;
}

char *ProcessPath (void) {
    char buf[1024] = {'\0'};
    static char *pPath = NULL;

    if (pPath) {
        return pPath;
    }

    int ret = readlink ("/proc/self/exe", buf, sizeof (buf));
    if (ret < 0 || ret > (int) sizeof (buf)) {
        logErr ( "readlink error!");
        return NULL;
    }

    char *pName = strrchr (buf, '/');
    *pName = '\0';
    pPath = (char *) calloc (1, strlen (buf) + 1);
    if (pPath) {
        strcpy (pPath, buf);
    }

    return pPath;
}

void SingleInstance (void) {
    char lockFile[128] = "/opt/smarthome/bin/";
    int lockFileSize = strlen (lockFile);
    char *pName = ProcessName ();
    if (!pName) {
        printf ("ProcessName error!\n");
        exit (0);
    }
    snprintf (lockFile + lockFileSize, sizeof (lockFile) - lockFileSize, "%s.lock", pName);

    int fd = open (lockFile, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        printf ("open %s error!\n", lockFile);
        exit (0);
    }

    if (lockf (fd, F_TLOCK, 0) < 0) {
        printf (COLOR_RED "%s already running!\n" COLOR_CLEAR, pName);
        exit (0);
    }
#if 0
    char buf[1024] = {'\0'};
    sprintf(buf, "%d\n", getpid());
    write(fd, buf, strlen(buf));

    int pid = 0;
    read(fd, buf, sizeof(buf));
    pid = atoi(buf);
    if (pid > 0) {
        printf("%s is running!\n", pName);
        exit(0);
    }
#endif
}

void ReportPosThread (void *param) {
    queue_t *pQueue = (queue_t *) param;
    int key = 0;
    TY_OBJ_DP_S Obj;

    while (true) {
        int ret = queue_get_wait (pQueue, (void **) &key); /* Dangerous explicit type pointer conversion: (void * *) & key. */
        if (ret != 0) {
            continue;
        }
        memset (&Obj, 0, sizeof (Obj));
        Obj.dpid = 199;
        Obj.type = PROP_STR;
        Obj.value.dp_str = (char *) "0";
        tuya_user_iot_report_obj_dp (NULL, &Obj, 1);
        if (tuya_user_iot_report_obj_dp (NULL, &Obj, 1) == 0) {
            logDbg ( COLOR_YELLOW "Report gateway pos to cloud success" COLOR_CLEAR);
        }
        usleep (1000 * 50);

        void *iter = NULL;
        DEV_DESC_IF_S *dev_if = tuya_user_iot_misc_dev_traversal (&iter);
        while (dev_if) {
            memset (&Obj, 0, sizeof (Obj));
            Obj.value.dp_str = dev_if->id + 13;// primary key
            Obj.type = PROP_STR;
            Obj.dpid = 199;
            if (tuya_user_iot_report_obj_dp (dev_if->id, &Obj, 1) == 0) {
                logDbg ( COLOR_YELLOW "Report sub-device(%s) pos to cloud success" COLOR_CLEAR, dev_if->id);
            }

            listSetMatchMethod (pKnxDevList, KnxDevIdMatch);// is knx device?
            listNode *pKnxNode = listSearchKey (pKnxDevList, dev_if->id);
            if (!pKnxNode) {
                goto report_position_next;
            }
            KnxBase_t *pKnx = (KnxBase_t *) pKnxNode->value;
            if ((pKnx->element.primaryType >= KNX_MISC_PANEL_ONE_KEY) && (pKnx->element.primaryType <= KNX_MISC_PANEL_FOUR_KEY)) {
                KnxSmartPanel_t *pPanel = (KnxSmartPanel_t *) pKnxNode->value;
                for (int i = 0; i < (int) pPanel->keySum; i++) {
                    memset (&Obj, 0, sizeof (Obj));

                    Obj.value.dp_enum = pPanel->chl[i].mode.point.value.u8;
                    Obj.type = PROP_ENUM;
                    Obj.dpid = (uint8_t) pPanel->chl[i].mode.point.id;

                    if (tuya_user_iot_report_obj_dp (pPanel->base.element.pPrimaryKey, &Obj, 1) != OPRT_OK) {
                        logErr ( "[knx smart panel] report pPrimaryKey=%s, dpid=%d, mode=%s to cloud failed!", pPanel->base.element.pPrimaryKey, Obj.dpid, (pPanel->chl[i].mode.point.value.u8 ? "scene" : "switch"));
                    } else {
                        logDbg ( COLOR_YELLOW "[knx smart panel] report pPrimaryKey=%s, dpid=%d, mode=%s to cloud success!" COLOR_CLEAR, pPanel->base.element.pPrimaryKey, Obj.dpid, (pPanel->chl[i].mode.point.value.u8 ? "scene" : "switch"));
                    }
                }
            }
        report_position_next:
            usleep (1000 * 500);
            dev_if = tuya_user_iot_misc_dev_traversal (&iter);
        }
    }
}

void ReportBindPosThread (void *param) {
    queue_t *pQueue = (queue_t *) param;
    DEV_DESC_IF_S *dev_if = NULL;
    TY_OBJ_DP_S Obj;

    while (true) {
        if (queue_get_wait (pQueue, (void **) &dev_if) != 0) {
            continue;
        }
        memset (&Obj, 0, sizeof (Obj));
        Obj.value.dp_str = dev_if->id + 13;// primary key
        Obj.type = PROP_STR;
        Obj.dpid = 199;
        if (tuya_user_iot_report_obj_dp (dev_if->id, &Obj, 1) == 0) {
            logDbg ( COLOR_YELLOW "Report bound sub-device(%s) pos to cloud success" COLOR_CLEAR, dev_if->id);
        }

        listSetMatchMethod (pKnxDevList, KnxDevIdMatch);// is knx device?
        listNode *pKnxNode = listSearchKey (pKnxDevList, dev_if->id);
        if (!pKnxNode) {
            continue;
        }
        KnxBase_t *pKnx = (KnxBase_t *) pKnxNode->value;
        if ((pKnx->element.primaryType >= KNX_MISC_PANEL_ONE_KEY) && (pKnx->element.primaryType <= KNX_MISC_PANEL_FOUR_KEY)) {
            KnxSmartPanel_t *pPanel = (KnxSmartPanel_t *) pKnxNode->value;
            for (int i = 0; i < (int) pPanel->keySum; i++) {
                memset (&Obj, 0, sizeof (Obj));

                Obj.value.dp_enum = pPanel->chl[i].mode.point.value.u8;
                Obj.type = PROP_ENUM;
                Obj.dpid = (uint8_t) pPanel->chl[i].mode.point.id;

                if (tuya_user_iot_report_obj_dp (pPanel->base.element.pPrimaryKey, &Obj, 1) != OPRT_OK) {
                    logErr ( "[knx smart panel] report pPrimaryKey=%s, dpid=%d, mode=%s to cloud failed!", pPanel->base.element.pPrimaryKey, Obj.dpid, (pPanel->chl[i].mode.point.value.u8 ? "scene" : "switch"));
                } else {
                    logDbg ( "[knx smart panel] report pPrimaryKey=%s, dpid=%d, mode=%s to cloud success!", pPanel->base.element.pPrimaryKey, Obj.dpid, (pPanel->chl[i].mode.point.value.u8 ? "scene" : "switch"));
                }
            }
        }
        usleep (1000 * 100);
    }
}

void di_scan (void *param) {
    (void) param;
    listIter iter;
    listNode *pNode;
    char buf[256] = {'\0'};
    struct timeval delay;

    if (!g_bOnline) {
        sleep (5);
    }

    while (true) {
        listRewind (pDioDevList, &iter);
        while ((pNode = listNext (&iter)) != NULL) {
            if (!pNode->value) continue;
            DiDev_t *pDi = (DiDev_t *) pNode->value;

            FILE *fd = popen (pDi->cmd, "r");
            memset (buf, 0, sizeof (buf));
            fgets (buf, sizeof (buf) - 1, fd);
            pclose (fd);
            errno = 0;
            pDi->value = strtol (buf, NULL, 10);
            if (errno != 0) {
                logErr ( "strtol error! %s, value=%d", strerror (errno), pDi->value);
                continue;
            }

            if (pDi->eLevel == pDi->value) {
                if (pDi->retryCnt < pDi->retryNum) {
                    pDi->retryCnt = pDi->retryNum;
                } else if (pDi->retryCnt < pDi->retryNum * 2) {
                    pDi->retryCnt++;
                } else {
                    if (pDi->status != DI_LEVEL_LOW) {
                        pDi->status = DI_LEVEL_LOW;
                        TY_OBJ_DP_S Obj;
                        Obj.dpid = pDi->dataPoint;
                        Obj.value.dp_enum = DI_LEVEL_LOW;
                        Obj.type = PROP_ENUM;
                        if (g_bActive) {
                            zlog_info (pLogCat, "DI%d %s status: %d", pDi->channel, pDi->element.pPrimaryKey,
                                       pDi->status);
                            int ret = tuya_user_iot_report_obj_dp (pDi->element.pPrimaryKey, &Obj, 1);
                            if (ret != 0) {
                                logErr (
                                            "report di state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d",
                                            pDi->element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                                continue;
                            } else {
                                logDbg (
                                            "report di state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d",
                                            pDi->element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                            }
                        }
                    }
                }
            } else {
                if (pDi->retryCnt > pDi->retryNum) {
                    pDi->retryCnt = pDi->retryNum;
                } else if (pDi->retryCnt != 0) {
                    pDi->retryCnt--;
                } else {
                    if (pDi->status != DI_LEVEL_HIGH) {
                        pDi->status = DI_LEVEL_HIGH;
                        TY_OBJ_DP_S Obj;
                        Obj.dpid = pDi->dataPoint;
                        Obj.value.dp_enum = DI_LEVEL_HIGH;
                        Obj.type = PROP_ENUM;
                        if (g_bActive) {
                            zlog_info (pLogCat, "DI%d %s status: %d", pDi->channel, pDi->element.pPrimaryKey,
                                       pDi->status);
                            int ret = tuya_user_iot_report_obj_dp (pDi->element.pPrimaryKey, &Obj, 1);
                            if (ret != 0) {
                                logErr (
                                            "report di state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d",
                                            pDi->element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                            } else {
                                logDbg (
                                            "report di state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d",
                                            pDi->element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                            }
                        }
                    }
                }
            }
        }
        delay.tv_sec = 1;
        delay.tv_usec = 0;
        select (0, NULL, NULL, NULL, &delay);
    }

    logErr ( "di_scan thread crashed");
}

void RunLedThread (void *param) {
    (void) param;
    struct timespec tim, tim1, tim2;
    tim1.tv_sec = 0;
    tim1.tv_nsec = (1000 * 1000 * 300);
    tim2.tv_sec = 1;
    tim2.tv_nsec = (1000 * 1000 * 200);
    while (true) {
        RunLed (true);
        nanosleep (&tim1, &tim);
        RunLed (false);
        nanosleep (&tim2, &tim);
    }
}

void DumpProcessMap (void) {
    char cmd[128] = {0};
    char buf[1024] = {0};

    snprintf (cmd, sizeof (cmd), "cat /proc/%d/maps", getpid ());
    zlog_fatal (pLogCat, COLOR_RED "++++++++++++++++++++++++++++++process(%d) map begin++++++++++++++++++++++++++++++++++++" COLOR_CLEAR, getpid ());
    FILE *stream = popen (cmd, "r");
    while (fgets (buf, sizeof (buf), stream) != NULL) {
        zlog_fatal (pLogCat, "%s", buf);
    }
    pclose (stream);
    zlog_fatal (pLogCat, COLOR_RED "++++++++++++++++++++++++++++++process map end++++++++++++++++++++++++++++++++++++\n" COLOR_CLEAR);
}

void ExceptionHandler (void) {
#define BT_BUF_SIZE 200
    void *buffer[BT_BUF_SIZE];

    DumpProcessMap ();
    int nptrs = backtrace (buffer, BT_BUF_SIZE);
    zlog_fatal (pLogCat, COLOR_RED "------------------------------Exception begin------------------------------------" COLOR_CLEAR);

    zlog_fatal (pLogCat, COLOR_RED "backtrace() returned %d addresses" COLOR_CLEAR, nptrs);

#if 1
    char **symbols = backtrace_symbols (buffer, nptrs);
    if (!symbols) {
        zlog_fatal (pLogCat, "backtrace_symbols");
        exit (EXIT_FAILURE);
    }

    for (int i = 0; i < nptrs; i++) {
        zlog_fatal (pLogCat, COLOR_RED "%s" COLOR_CLEAR, symbols[i]);
    }
    free (symbols);
#elif 0
    int fd = open ("/opt/smarthome/log/crash/minicore", O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        printf ("open %s error!\n", "/opt/smarthome/log/crash/minicore");
        exit (0);
    }
    backtrace_symbols_fd (buffer, nptrs, fd);
    close (fd);
#endif

    zlog_fatal (pLogCat, COLOR_RED "------------------------------Exception end------------------------------------\n" COLOR_CLEAR);
}

char *DumpDiDevInfo (const char *pDevId) {
    listSetMatchMethod (pDioDevList, DioDevIdMatch);
    listNode *pNode = listSearchKey (pDioDevList, (char *) pDevId);
    if (!pNode) {
        logErr ( "Dio %s device not exist!", pDevId);
        return NULL;
    }
    DiDev_t *pDi = (DiDev_t *) pNode->value;
    char *pBuf = (char *) calloc (1, 256);
    if (!pBuf) return NULL;
    snprintf (pBuf, 255,
              "DevId:%s\nPId:%s\nPoint:%d\nTriggerLevel:%d\nChannel:%d\nStatus:%d\nRetryNum:%d\nRetryCnt:%d\nValue:%d\n",
              pDi->element.pPrimaryKey, pDi->element.pProductId, pDi->dataPoint, pDi->eLevel, pDi->channel, pDi->status,
              pDi->retryNum,
              pDi->retryCnt, pDi->value);

    return pBuf;
}

char *DumpMbusDevInfo (const char *pDevId) {
    listSetMatchMethod (pMbusDevList, ModbusDevIdMatch);
    listNode *pNode = listSearchKey (pMbusDevList, (char *) pDevId);
    if (!pNode) {
        logErr ( "Mbus %s device not exist!", pDevId);
        return NULL;
    }
    MbusBase_t **ppBase = (MbusBase_t **) pNode->value;
    if ((!ppBase) || (!*ppBase)) {
        logErr ( COLOR_RED "Mbus ppBase is NULL!" COLOR_CLEAR);
        return NULL;
    }

    char *pBuf = (char *) calloc (1, 256);
    if (!pBuf) return NULL;
    if ((*ppBase)->element.primaryType == MODBUS_DEVICE_VENTILATION) {
        MbusVent_t *pVent = (MbusVent_t *) pNode->value;
        snprintf (pBuf, 255, "DevId:%s\n"
                             "PId:%s\n"
                             "devAddr:%d\n"
                             "port:%d\n"
                             "baudrate:%d\n"
                             "databits:%d\n"
                             "parity:%d\n"
                             "stopbits:%d\n"
                             "\t[switch]\n"
                             "\tdataPoint:%d\n"
                             "\tvalue:%u\n"
                             "\t[speed]\n"
                             "\tdataPoint:%d\n"
                             "\tvalue:%u\n",
                  (*ppBase)->element.pPrimaryKey,
                  (*ppBase)->element.pProductId,
                  (*ppBase)->devAddr,
                  (*ppBase)->pSerial->getPort ((*ppBase)->pSerial),
                  (*ppBase)->pSerial->getBaudrate ((*ppBase)->pSerial),
                  (*ppBase)->pSerial->getDatabits ((*ppBase)->pSerial),
                  (*ppBase)->pSerial->getParity ((*ppBase)->pSerial),
                  (*ppBase)->pSerial->getStopbits ((*ppBase)->pSerial),
                  pVent->pSwitch->cloudPoint,
                  pVent->pSwitch->value,
                  pVent->pSpeed->cloudPoint,
                  pVent->pSpeed->value);
    }

    return pBuf;
}
