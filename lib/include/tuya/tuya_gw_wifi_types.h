#ifndef TUYA_GW_WIFI_TYPES_H
#define TUYA_GW_WIFI_TYPES_H

#include <stdint.h>

#include "tuya_gw_base_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSSID_MAX_LEN   6
#define SSID_MAX_LEN    32
#define KEY_MAX_LEN     64

typedef SNIFFER_CALLBACK sniffer_callback;

typedef AP_IF_S          ap_scan_info_s;
typedef WF_AP_CFG_IF_S   ap_cfg_info_s;

typedef WF_IF_E wifi_type_t;
#define STATION             WF_STATION        ///< station type
#define AP                  WF_AP             ///< ap type

typedef WF_WK_MD_E wifi_work_mode_t;
#define WMODE_LOWPOWER      WWM_LOWPOWER      ///< wifi work in lowpower mode
#define WMODE_SNIFFER       WWM_SNIFFER       ///< wifi work in sniffer mode
#define WMODE_STATION       WWM_STATION       ///< wifi work in station mode
#define WMODE_SOFTAP        WWM_SOFTAP        ///< wifi work in ap mode
#define WMODE_STATIONAP     WWM_STATIONAP     ///< wifi work in station+ap mode
#define WMODE_UNKNOWN       WWM_UNKNOWN       ///< wifi work in unknown mode

typedef WF_STATION_STAT_E station_conn_stat_t;
#define STAT_IDLE           WSS_IDLE          ///< not connected
#define STAT_CONNECTING     WSS_CONNECTING    ///< connecting wifi
#define STAT_PASSWD_WRONG   WSS_PASSWD_WRONG  ///< passwd not match
#define STAT_NO_AP_FOUND    WSS_NO_AP_FOUND   ///< ap is not found
#define STAT_CONN_FAIL      WSS_CONN_FAIL     ///< connect fail
#define STAT_CONN_SUCCESS   WSS_CONN_SUCCESS  ///< connect wifi success
#define STAT_GOT_IP         WSS_GOT_IP        ///< get ip success

typedef WF_AP_AUTH_MODE_E ap_encryption_type_t;
#define TYPE_OPEN           WAAM_OPEN         ///< open
#define TYPE_WEP            WAAM_WEP          ///< WEP
#define TYPE_WPA_PSK        WAAM_WPA_PSK      ///< WPA—PSK
#define TYPE_WPA2_PSK       WAAM_WPA2_PSK     ///< WPA2—PSK
#define TYPE_WPA_WPA2_PSK   WAAM_WPA_WPA2_PSK ///< WPA/WPA2

#ifdef __cplusplus
}
#endif
#endif
