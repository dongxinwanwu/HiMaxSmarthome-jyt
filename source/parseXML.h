#ifndef __CONFIG_XML__
#define __CONFIG_XML__
#include <stdint.h>
#include <stdio.h>
#include "list.h"
#include "misc_dev_plugins.h"

int ParseZ3Cfg(const char *pDocPath, list_t *pList);
int ParseMbusHeatCfg(const char *pDocPath, list_t *pList);
int ParseMbusVentCfg(const char *pDocPath, list_t *pList);
int ParseMbusAcdCfg(const char *pDocPath, list_t *pList);
int ParseMbusAqiCfg(const char *pDocPath, list_t *pList);
int ParseModbusHumidifierCfg(const char *pDocPath, list_t *pList);
int ParseModbusPurifierCfg(const char *pDocPath, list_t *pList);
int ParseMbusPanelCfg(const char *pDocPath, list_t *pList);
int ParseKnxAcdCfg(const char *pDocPath, list_t *pList);
int ParseKnxVentCfg(const char *pDocPath, list_t *pList);
int ParseKnxHeatCfg(const char *pDocPath, list_t *pList);
Gateway_t * ParseGatewayCfg (const char *pDocPath);

int ParseKnxPanelCfg(const char *pDocPath, list_t *pList);
int ParseKnxSmartPanelCfg (const char *pDocPath, list_t *pList);
int ParseKnxDimmerCfg (const char *pDocPath, list_t *pList);
int ParseDeviceElement(const char *pDocPath, DeviceElement_t *pElement);
int ParseKnxSceneCfg(const char *pDocPath, list_t *pList);
int ParseMbusCurtainCfg(const char *pDocPath, list_t *pList);
int ParseDiCfg(const char *pDocPath, list_t *pList);
int ParseDoCfg(const char *pDocPath, list_t *pList);
int UpdateKnxSmartPanelMode (const char *pUrl, int priKey, int chl, const uint8_t *pValue);

int ParseGroupCfg(const char *pDocPath, const char *pPath, list_t *pList);
int ParseZigbeeCfg (const char *pDocPath, bool *pEnabled);
int ParseKnxCurtainCfg (const char *pDocPath, list_t *pList);
int ParseKnxCfg (const char *pDocPath, bool *pEnabled);

int UpdateKnxVentSpeedCtrlGrp (const char *pUrl, const uint8_t *pValueOld, const uint8_t *pValueNew);
int UpdateKnxDimmerGrp (const char *pUrl, int priKey, const uint8_t *pSCValue, const uint8_t *pSSValue, const uint8_t *pBCValue, const uint8_t *pBSValue);

#endif
