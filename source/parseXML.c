#include "parseXML.h"
#include "device.h"
#include "knxHandle.h"
#include "libxml/parser.h"
#include "libxml/xpath.h"
#include "list.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "point.h"
#include "serialHandle.h"
#include <assert.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DI1 "/sys/class/dry-interface/di1/value"
#define DI2 "/sys/class/dry-interface/di2/value"
#define DI3 "/sys/class/dry-interface/di3/value"
#define DI4 "/sys/class/dry-interface/di4/value"

static const int KEYWORD_ZIGBEE_LEN = 6;
static const int KEYWORD_MODBUS_LEN = 6;
static const int KEYWORD_KNX_LEN = 3;
static const int KEYWORD_RF433_LEN = 5;
static const int KEYWORD_OPEN_LEN = 4;
static const int KEYWORD_CLOSE_LEN = 5;
static const int KEYWORD_LOW_LEN = 3;
static const int KEYWORD_HIGH_LEN = 4;

xmlXPathObjectPtr XmlGetNode (xmlDocPtr pDoc, const xmlChar *xpath) {
    if (!pDoc || !xpath) {
        logErr ("pUrl or xpath is NULL");
        return NULL;
    }

    xmlXPathContextPtr pXpathCtx = xmlXPathNewContext (pDoc);
    if (pXpathCtx == NULL) {
        logErr ("context is NULL");
        return NULL;
    }

    xmlXPathObjectPtr pXpathObj = xmlXPathEvalExpression (xpath, pXpathCtx);
    xmlXPathFreeContext (pXpathCtx);
    if (!pXpathObj) {
        logErr ("xmlXPathEvalExpression return NULL");
        return NULL;
    }

    if (xmlXPathNodeSetIsEmpty (pXpathObj->nodesetval)) {
        xmlXPathFreeObject (pXpathObj);
        //        logDbg ( "XML Node \"%s\" not exist!", xpath);  /* cause a panic */
        return NULL;
    }

    return pXpathObj;
}

int XmlUpdateNode (xmlXPathObjectPtr pXpathObj, const xmlChar *value) {
    if (!pXpathObj || !value) {
        logErr ("pUrl or xpath or value is NULL");
        return -1;
    }

    if (xmlXPathNodeSetIsEmpty (pXpathObj->nodesetval)) {
        logErr ("XML Node \"%s\" not exist!", value);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    logTrc ("pNodeset->nodeNr: %d, value: %s", pNodeset->nodeNr, value);

    xmlNodeSetContent (pNodeset->nodeTab[0], value);
    if (pNodeset->nodeTab[0]->type != XML_NAMESPACE_DECL) {
        pNodeset->nodeTab[0] = NULL;
    }

    return 0;
}

int ParseDeviceElement (const char *pDocPath, DeviceElement_t *pElement) {
    xmlChar *pXpath = BAD_CAST ("/Device/Gateway");
    xmlNodePtr pNode = NULL;

    if (!pDocPath || !pElement) {
        return -1;
    }

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (!pDoc) {
        logErr ("XML Document \"%s\" not parsed successful.", pDocPath);
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        while (pNode != NULL) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "UUID")) {
                char *pUUID = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pUUID) {
                    pElement->szUUID = (char *) calloc (1, strlen (pUUID) + PREFIX_SIZE);
                    if (!pElement->szUUID) {
                        logErr ("calloc szUUID failed");
                        continue;
                    }
                    strcpy ((char *) pElement->szUUID, pUUID);
                    logDbg ("szUUID: %s", pElement->szUUID);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "AuthKey")) {
                char *pKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pKey) {
                    pElement->szAuthKey = (char *) calloc (1, strlen (pKey) + PREFIX_SIZE);
                    if (!pElement->szAuthKey) {
                        logErr ("calloc szAuthKey failed");
                        continue;
                    }
                    strcpy ((char *) pElement->szAuthKey, pKey);
                    logDbg ("szAuthKey: %s", pElement->szAuthKey);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pElement->pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pElement->pProductId) {
                        logErr ("calloc Product ID failed");
                        exit (EXIT_FAILURE);
                    }
                    strcpy ((char *) pElement->pProductId, pProduct);
                    logErr ("[gateway] Product ID:%s", pElement->pProductId);
                }
            }

            pNode = pNode->next;
        }
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

Gateway_t *ParseGatewayCfg (const char *pDocPath) {
    xmlChar *pXpath = BAD_CAST ("/Device/Gateway");

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (!pDoc) {
        logErr ("XML Document \"%s\" not parsed successful.", pDocPath);
        return NULL;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return NULL;
    }
    Gateway_t *pGw = (Gateway_t *) calloc (1, sizeof (Gateway_t));

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    xmlNodePtr pNode = pNodeset->nodeTab[0];
    pNode = pNode->xmlChildrenNode;
    while (pNode != NULL) {
        if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
            char *pPid = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
            if (pPid) {
                pGw->pProductId = (char *) calloc (1, strlen (pPid) + 1);
                strcpy (pGw->pProductId, pPid);
            }
        } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "state")) {
            char *pState = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
            if (pState) {
                pGw->pState = (Point_t *) calloc (1, sizeof (Point_t));
                errno = 0;
                pGw->pState->id = strtol (pState, NULL, 10);
                if (errno != 0) {
                    logErr ("strtol error");
                }
            }
        } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "reset")) {
            char *pReset = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
            if (pReset) {
                pGw->pReset = (Point_t *) calloc (1, sizeof (Point_t));
                errno = 0;
                pGw->pReset->id = strtol (pReset, NULL, 10);
                if (errno != 0) {
                    logErr ("strtol error");
                }
            }
        } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "alarm")) {
            char *pAlarm = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
            if (pAlarm) {
                pGw->pAlarm = (Point_t *) calloc (1, sizeof (Point_t));
                errno = 0;
                pGw->pAlarm->id = strtol (pAlarm, NULL, 10);
                if (errno != 0) {
                    logErr ("strtol error");
                }
            }
        } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "scene")) {
            char *pScene = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
            if (pScene) {
                pGw->pScene = (Point_t *) calloc (1, sizeof (Point_t));
                errno = 0;
                pGw->pScene->id = strtol (pScene, NULL, 10);
                if (errno != 0) {
                    logErr ("strtol error");
                    exit (EXIT_FAILURE);
                }
            }
        } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "fault")) {
            char *pFault = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
            if (pFault) {
                pGw->pReset = (Point_t *) calloc (1, sizeof (Point_t));
                errno = 0;
                pGw->pReset->id = strtol (pFault, NULL, 10);
                if (errno != 0) {
                    logErr ("strtol error");
                }
            }
        } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "pos")) {
            char *pPos = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
            if (pPos) {
                pGw->pPos = (Point_t *) calloc (1, sizeof (Point_t));
                errno = 0;
                pGw->pPos->id = strtol (pPos, NULL, 10);
                if (errno != 0) {
                    logErr ("strtol error");
                    exit (EXIT_FAILURE);
                }
            }
        }
        pNode = pNode->next;
    }
    xmlXPathFreeObject (pXpathObj);

    return pGw;
}

int ParseZ3Cfg (const char *pDocPath, list_t *pList) {
    xmlDocPtr pDoc;
    xmlChar *pXpath = BAD_CAST ("/Device/Zigbee/Cell");
    //    xmlChar * value = NULL;
    xmlXPathObjectPtr pXpathObj = NULL;
    xmlNodeSetPtr pNodeset = NULL;
    xmlNodePtr pNode = NULL;

    if (!pDocPath || !pList) {
        return -1;
    }

    pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (!pDoc) {
        logErr ("XML Document \"%s\" not parsed successful.", pDocPath);
        return -1;
    }

    pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        Z3Dev_t *pZ3Dev = (Z3Dev_t *) calloc (1, sizeof (Z3Dev_t));
        assert (pZ3Dev != NULL);
        while (pNode != NULL) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *pPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPlugin) {
                    pZ3Dev->element.szPlugin = (char *) calloc (1, strlen (pPlugin) + PREFIX_SIZE);
                    if (!pZ3Dev->element.szPlugin) {
                        logErr ("calloc Z3 plugin failed");
                        continue;
                    }
                    strcpy ((char *) pZ3Dev->element.szPlugin, pPlugin);
                    logDbg ("Z3 szPlugin: %s", pZ3Dev->element.szPlugin);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "UUID")) {
                char *pUUID = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pUUID) {
                    pZ3Dev->element.szUUID = (char *) calloc (1, strlen (pUUID) + PREFIX_SIZE);
                    if (!pZ3Dev->element.szUUID) {
                        logErr ("calloc szUUID failed");
                        continue;
                    }
                    strcpy ((char *) pZ3Dev->element.szUUID, pUUID);
                    logErr ("[Zigbee 3.0] szUUID:%s", pZ3Dev->element.szUUID);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "AuthKey")) {
                char *pKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pKey) {
                    pZ3Dev->element.szAuthKey = (char *) calloc (1, strlen (pKey) + PREFIX_SIZE);
                    if (!pZ3Dev->element.szAuthKey) {
                        logErr ("calloc Z3 szAuthKey failed");
                        continue;
                    }
                    strcpy ((char *) pZ3Dev->element.szAuthKey, pKey);
                    logDbg ("Z3 szAuthKey: %s", pZ3Dev->element.szAuthKey);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pZ3Dev->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    strcpy ((char *) pZ3Dev->element.pProductId, pProduct);
                    logErr ("[Zigbee 3.0] Product ID:%s", pZ3Dev->element.pProductId);
                }
            }

            pNode = pNode->next;
        }
        pZ3Dev->uddd = 0x8000200;// zigbee 设备
        listAddNodeTail (pList, (void *) pZ3Dev);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

int ParseZigbeeCfg (const char *pDocPath, bool *pEnabled) {
    xmlChar *pXpath = BAD_CAST ("/Device/Zigbee");
    if (!pDocPath) return -1;

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        fprintf (stderr, "Document not parsed successful!");
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        xmlAttrPtr pAttr = pNode->properties;
        while (pAttr) {
            if (!xmlStrcasecmp (pAttr->name, BAD_CAST "module")) {
                xmlChar *pType = xmlGetProp (pNode, (xmlChar *) "module");
                if (pType) {
                    if (!xmlStrcasecmp (pType, BAD_CAST "pass")) {
                        *pEnabled = true;
                        logDbg ("zigbee module enabled");
                    } else if (!xmlStrcasecmp (pType, BAD_CAST "bad")) {
                        *pEnabled = false;
                        logDbg ("zigbee module disabled");
                    } else {
                        logErr ("zigbee module(\"%s\") not supported!", pType);
                    }
                    xmlFree (pType);
                }
            }
            pAttr = pAttr->next;
        }
        pNode = pNode->next;
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

int ParseKnxCfg (const char *pDocPath, bool *pEnabled) {
    xmlChar *pXpath = BAD_CAST ("/Device/KNX");
    if (!pDocPath) return -1;

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        fprintf (stderr, "Document not parsed successful!");
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        xmlAttrPtr pAttr = pNode->properties;
        while (pAttr) {
            if (!xmlStrcasecmp (pAttr->name, BAD_CAST "autoAck")) {
                xmlChar *pType = xmlGetProp (pNode, (xmlChar *) "autoAck");
                if (pType) {
                    if (!xmlStrcasecmp (pType, BAD_CAST "enabled")) {
                        *pEnabled = true;
                        logDbg ("knx auto ack enabled");
                    } else if (!xmlStrcasecmp (pType, BAD_CAST "disabled")) {
                        *pEnabled = false;
                        logDbg ("knx auto ack disabled");
                    } else {
                        logErr ("knx auto ack %s illegal!", pType);
                    }
                    xmlFree (pType);
                }
            }
            pAttr = pAttr->next;
        }
        pNode = pNode->next;
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

/* pMbusAcd 外机 */
static int ParseMbusAcdKey (const xmlNodePtr pItem, MbusAcd_t *pMbusAcd, list_t *pList) {
    if (!pItem || !pMbusAcd || !pMbusAcd->pBase || !pList) return -1;
    xmlNodePtr pCell = pItem->xmlChildrenNode;

    while (pCell) {
        if (!xmlStrcasecmp (pCell->name, (const xmlChar *) "Cell")) {
            char *pForeignKey = NULL;
            MbusAcd_t *pAir = (MbusAcd_t *) calloc (1, sizeof (MbusAcd_t));
            if (!pAir) {
                logErr ("MbusAcd_t calloc fail!\n");
                continue;
            }

            /* deep copy */
            if (pMbusAcd->pSwitch) {
                pAir->pSwitch = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pAir->pSwitch) continue;
                memcpy (pAir->pSwitch, pMbusAcd->pSwitch, sizeof (MbusObj_t));
            }
            if (pMbusAcd->pSpeed) {
                pAir->pSpeed = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pAir->pSpeed) continue;
                memcpy (pAir->pSpeed, pMbusAcd->pSpeed, sizeof (MbusObj_t));
            }
            if (pMbusAcd->pMode) {
                pAir->pMode = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pAir->pMode) continue;
                memcpy (pAir->pMode, pMbusAcd->pMode, sizeof (MbusObj_t));
            }
            if (pMbusAcd->pTemp) {
                pAir->pTemp = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pAir->pTemp) continue;
                memcpy (pAir->pTemp, pMbusAcd->pTemp, sizeof (MbusObj_t));
            }
            if (pMbusAcd->pEnvTemp) {
                pAir->pEnvTemp = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pAir->pEnvTemp) continue;
                memcpy (pAir->pEnvTemp, pMbusAcd->pEnvTemp, sizeof (MbusObj_t));
            }
            if (pMbusAcd->pErrorCode) {
                pAir->pErrorCode = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pAir->pErrorCode) continue;
                memcpy (pAir->pErrorCode, pMbusAcd->pErrorCode, sizeof (MbusObj_t));
            }
            pAir->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
            if (!pAir->pBase) continue;
            memcpy (pAir->pBase, pMbusAcd->pBase, sizeof (MbusBase_t));

            pAir->pBase->pSerial = (Serial_t *) calloc (1, sizeof (Serial_t));
            if (!pAir->pBase->pSerial) continue;
            memcpy (pAir->pBase->pSerial, pMbusAcd->pBase->pSerial, sizeof (Serial_t));
            if (pAir->pBase->bFullDuplex) {
                pAir->pBase->pSerial->open (pAir->pBase->pSerial);
                pAir->pBase->pSerial->recv (&pAir->pBase->pSerial);
            }
            xmlChar *pGroup = xmlGetProp (pCell, (xmlChar *) "group");
            if (pGroup) {
                pAir->pBase->bGroupMember = true;
                xmlFree (pGroup);
            }
            /* deep copy end */

            xmlNodePtr pKey = pCell->xmlChildrenNode;
            while (pKey) {
                if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "PrimaryKey")) {
                    char *pPrimaryKey = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (pPrimaryKey) {
                        pAir->pBase->element.primaryType = MODBUS_DEVICE_AIRCONDITION;
                        pAir->pBase->element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                        if (!pAir->pBase->element.pPrimaryKey) continue;
                        snprintf ((char *) pAir->pBase->element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_MODBUS, pPrimaryKey);
                        pAir->pBase->element.devId = strtol ((char *) pAir->pBase->element.pPrimaryKey + 12, NULL, 10);
                        logDbg ("mbus acd primary key =%s, devId=%d", pAir->pBase->element.pPrimaryKey, pAir->pBase->element.devId);
                    } else {
                        logErr ("mbus acd primary key is NULL!");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "ForeignKey")) {
                    pForeignKey = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pForeignKey) {
                        logErr ("mbus acd foreign key is NULL!");
                        continue;
                    }
                    pAir->pBase->element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pAir->pBase->element.pForeignKey) {
                        logErr ("mbus acd foreign key calloc failed!");
                        continue;
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "ForeignType")) {
                    char *pTmp = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                        pAir->pBase->element.foreignType = DEVICE_TYPE_ZIGBEE;
                    } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                        pAir->pBase->element.foreignType = DEVICE_TYPE_MODBUS;
                    } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                        pAir->pBase->element.foreignType = DEVICE_TYPE_KNX;
                    } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                        pAir->pBase->element.foreignType = DEVICE_TYPE_RF433;
                    } else {
                        pAir->pBase->element.foreignType = DEVICE_TYPE_UNKNOWN;
                        logErr ("mbus vent foreign type is illegal!");
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "RoomNo")) {
                    char *pTmp = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pTmp) continue;
                    errno = 0;
                    pAir->pBase->regAddr = strtol (pTmp, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus acd regAddr invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus acd RoomNo is %d", pAir->pBase->regAddr);
                }
                pKey = pKey->next;
            }
            if (pForeignKey && (pAir->pBase->element.foreignType != DEVICE_TYPE_UNKNOWN)) {
                snprintf ((char *) pAir->pBase->element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, pAir->pBase->element.foreignType, pForeignKey);
                logDbg ("mbus acd foreign key is %s", pAir->pBase->element.pForeignKey);
            }
            listAddNodeTail (pList, (void *) pAir);
        }
        pCell = pCell->next;
    }

    return 0;
}

int ParseMbusAcdCfg (const char *pDocPath, list_t *pList) {
    xmlChar *pXpath = BAD_CAST ("/Device/Modbus/AirCondition/Cell");
    if (!pDocPath || !pList) return -1;

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        fprintf (stderr, "Document not parsed successful!");
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) { /* <AirCondition> <cell> */
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        xmlNodePtr pItem = NULL;
        MbusAcd_t *pAir = (MbusAcd_t *) calloc (1, sizeof (MbusAcd_t));
        if (!pAir) {
            logErr ("MbusAcd_t calloc fail!");
            continue;
        }
        pAir->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
        if (!pAir->pBase) {
            logErr ("MbusBase_t calloc fail!");
            continue;
        }
        pAir->pBase->pNext = NULL;
        pAir->pBase->pSerial = SerialInit ();
        if (!pAir->pBase->pSerial) {
            logErr ("SerialInit fail!");
            continue;
        }
        pAir->pBase->element.szName = "AirCondition";
        pAir->pBase->element.primaryType = MODBUS_DEVICE_AIRCONDITION;
        pAir->pBase->element.bMaster = false;
        pAir->pBase->bFeedback = true;
        pAir->pBase->bOnlineCheck = true;
        pAir->pBase->bFullDuplex = false;
        pAir->pBase->bGroupMember = false;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!szPlugin) {
                    logErr ("mbus acd plugin is NULL");
                    exit (EXIT_FAILURE); /* force exit */
                }
                pAir->pBase->element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                if (!pAir->pBase->element.szPlugin) {
                    logErr ("calloc mbus acd plugin failed");
                    continue;
                }
                strcpy ((char *) pAir->pBase->element.szPlugin, szPlugin);
                //                    pAir->pBase->element.bMaster = true;
                logDbg ("mbus acd plugin is %s", pAir->pBase->element.szPlugin);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pAir->pBase->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pAir->pBase->element.pProductId) {
                        logErr ("calloc mbus acd product id failed");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    strcpy ((char *) pAir->pBase->element.pProductId, pProduct);
                    pAir->pBase->element.bMaster = true;
                    logDbg ("mbus acd product id is %s.", pAir->pBase->element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAddr")) {
                char *pDeviceAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDeviceAddr) {
                    errno = 0;
                    pAir->pBase->devAddr = (uint16_t) strtol (pDeviceAddr, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus acd com id invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus acd dev id:%d", pAir->pBase->devAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Port")) {
                char *pPort = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPort) {
                    errno = 0;
                    int port = strtol (pPort, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus acd com port invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pAir->pBase->pSerial->setPort (pAir->pBase->pSerial, port);
                    if (ret != 0) {
                        logErr ("mbus acd com port(%d) invalid", port);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus acd dev com port:%d", port);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Baudrate")) {
                char *pBaudrate = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pBaudrate) {
                    errno = 0;
                    int baudrate = strtol (pBaudrate, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus acd com baudrate invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pAir->pBase->pSerial->setBaudrate (pAir->pBase->pSerial, baudrate);
                    if (ret != 0) {
                        logErr ("mbus acd com baudrate(%s) invalid", pBaudrate);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus acd dev baudrate:%d", baudrate);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Databit")) {
                char *pDatabit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDatabit) {
                    errno = 0;
                    int databit = strtol (pDatabit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus acd com pDatabit invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pAir->pBase->pSerial->setDatabits (pAir->pBase->pSerial, databit);
                    if (ret != 0) {
                        logErr ("mbus acd com databit(%d) invalid", databit);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus acd dev com databit:%d", databit);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Parity")) {
                char *pParity = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pParity) {
                    int ret = pAir->pBase->pSerial->setParity (pAir->pBase->pSerial, *pParity);
                    if (ret != 0) {
                        logErr ("mbus acd parity:%s illegal！\n", pParity);
                        exit (EXIT_FAILURE); /* force exit */
                    } else {
                        logDbg ("mbus acd parity:%s.", pParity);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Stopbit")) {
                char *pStopbit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pStopbit) {
                    errno = 0;
                    int stopbits = strtol (pStopbit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus acd com stopbits invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    int ret = pAir->pBase->pSerial->setStopbits (pAir->pBase->pSerial, stopbits);
                    if (ret != 0) {
                        logErr ("mbus acd com stopbits(%d) invalid", stopbits);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus acd dev com stopbits:%d", stopbits);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pAir->pBase->bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pAir->pBase->bOnlineCheck = false;
                    logDbg ("rs485 acd online check is disabled!");
                } else if (pCmd && ((*pCmd == 'Y') || (*pCmd == 'y'))) {
                    pAir->pBase->bOnlineCheck = true;
                    logDbg ("rs485 acd online check is enabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "FullDuplex")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'Y') || (*pCmd == 'y'))) {
                    pAir->pBase->bFullDuplex = true;
                    pAir->pBase->pSerial->setFullDuplex (pAir->pBase->pSerial, true);
                    logDbg ("Note: rs485 air is full duplex!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Delay")) {
                char *pDelay = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDelay) {
                    errno = 0;
                    pAir->pBase->delay = strtol (pDelay, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus acd com delay invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus acd dev com delay:%d", pAir->pBase->delay);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Switch")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (!pDataPoint) {
                            logErr ("mbus acd switch dataPoint is NULL!");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        pAir->pSwitch = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                        if (!pAir->pSwitch) {
                            logErr ("mbus acd calloc fail!\n");
                            exit (EXIT_FAILURE); /* force to exit */
                        }
                        pAir->pSwitch->localPoint = ACD_SWITCH;
                        pAir->pSwitch->type = MODBUS_DATA_TYPE_BOOL;
                        pAir->pSwitch->valueOld = (uint32_t) -1;
                        pAir->pSwitch->forceSync = 0;
                        errno = 0;
                        pAir->pSwitch->cloudPoint = strtol (pDataPoint, NULL, 10);
                        if (errno != 0) {
                            logErr ("mbus acd switch dataPoint(%s) invalid", pDataPoint);
                            exit (EXIT_FAILURE); /* force exit */
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus acd switch dataPoint:%d", pAir->pSwitch->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Speed")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (!pDataPoint) {
                            logErr ("mbus acd speed dataPoint is NULL!");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        pAir->pSpeed = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                        if (!pAir->pSpeed) {
                            logErr ("mbus acd calloc fail!\n");
                            exit (EXIT_FAILURE); /* force to exit */
                        }
                        pAir->pSpeed->localPoint = ACD_SPEED;
                        pAir->pSpeed->type = MODBUS_DATA_TYPE_ENUM;
                        pAir->pSpeed->valueOld = (uint32_t) -1;
                        pAir->pSpeed->forceSync = 0;
                        errno = 0;
                        pAir->pSpeed->cloudPoint = strtol (pDataPoint, NULL, 10);
                        if (errno != 0) {
                            logErr ("mbus acd speed dataPoint(%s) invalid", pDataPoint);
                            exit (EXIT_FAILURE); /* force exit */
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus acd speed dataPoint:%d", pAir->pSpeed->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Mode")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (!pDataPoint) {
                            logErr ("mbus acd mode dataPoint is NULL!");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        pAir->pMode = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                        if (!pAir->pMode) {
                            logErr ("mbus acd calloc fail!\n");
                            exit (EXIT_FAILURE); /* force to exit */
                        }
                        pAir->pMode->localPoint = ACD_MODE;
                        pAir->pMode->type = MODBUS_DATA_TYPE_ENUM;
                        pAir->pMode->valueOld = (uint32_t) -1;
                        pAir->pMode->forceSync = 0;
                        errno = 0;
                        pAir->pMode->cloudPoint = strtol (pDataPoint, NULL, 10);
                        if (errno != 0) {
                            logErr ("mbus acd mode dataPoint(%s) invalid", pDataPoint);
                            exit (EXIT_FAILURE); /* force exit */
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus acd mode dataPoint:%d", pAir->pMode->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Temp")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (!pDataPoint) {
                            logErr ("mbus acd temp dataPoint is NULL!");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        pAir->pTemp = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                        if (!pAir->pTemp) {
                            logErr ("mbus acd calloc fail!\n");
                            exit (EXIT_FAILURE); /* force to exit */
                        }
                        pAir->pTemp->localPoint = ACD_TEMP;
                        pAir->pTemp->type = MODBUS_DATA_TYPE_VALUE;
                        pAir->pTemp->valueOld = (uint32_t) -1;
                        pAir->pTemp->forceSync = 0;
                        errno = 0;
                        pAir->pTemp->cloudPoint = strtol (pDataPoint, NULL, 10);
                        if (errno != 0) {
                            logErr ("mbus acd temp dataPoint(%s) invalid", pDataPoint);
                            exit (EXIT_FAILURE); /* force exit */
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus acd temp dataPoint:%d", pAir->pTemp->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "EnvTemp")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pAir->pEnvTemp = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pAir->pEnvTemp) {
                                logErr ("mbus acd calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pAir->pEnvTemp->localPoint = ACD_ENV_TEMP;
                            pAir->pEnvTemp->type = MODBUS_DATA_TYPE_VALUE;
                            pAir->pEnvTemp->valueOld = (uint32_t) -1;
                            pAir->pEnvTemp->forceSync = 0;
                            errno = 0;
                            pAir->pEnvTemp->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus acd real-time temp dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus acd env temp dataPoint:%d", pAir->pEnvTemp->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Fault")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pAir->pErrorCode = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pAir->pErrorCode) {
                                logErr ("mbus acd calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pAir->pErrorCode->localPoint = ACD_ERROR_CODE;
                            pAir->pErrorCode->type = MODBUS_DATA_TYPE_STR;
                            pAir->pErrorCode->valueOld = (uint32_t) -1;
                            pAir->pErrorCode->forceSync = 0;
                            errno = 0;
                            pAir->pErrorCode->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus acd error-code dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus acd error-code dataPoint:%d", pAir->pErrorCode->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Item")) {
                pItem = pNode;
            }
            pNode = pNode->next;
        }

        if (pItem) {
            int ret = ParseMbusAcdKey (pItem, pAir, pList);
            if (ret != -1) {
                if (pAir->pBase) {
                    if (pAir->pBase->pSerial) {
                        free (pAir->pBase->pSerial);
                    }
                    free (pAir->pBase);
                }
                free (pAir);
            }
        }
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

static int ParseMbusVentKey (const xmlNodePtr pItem, MbusVent_t *pModbusVent, list_t *pList) {
    if (!pItem || !pModbusVent || !pModbusVent->pBase || !pList) return -1;
    xmlNodePtr pCell = pItem->xmlChildrenNode;

    while (pCell) {
        if (!xmlStrcasecmp (pCell->name, (const xmlChar *) "Cell")) {
            char *pForeignKey = NULL;
            MbusVent_t *pVent = (MbusVent_t *) calloc (1, sizeof (MbusVent_t));
            if (!pVent) {
                logErr ("mbus vent MbusVent_t calloc fail!\n");
                continue;
            }

            /* deep copy */
            if (pModbusVent->pSwitch) {
                pVent->pSwitch = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pVent->pSwitch) continue;
                memcpy (pVent->pSwitch, pModbusVent->pSwitch, sizeof (MbusObj_t));
            }
            if (pModbusVent->pSpeed) {
                pVent->pSpeed = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pVent->pSpeed) continue;
                memcpy (pVent->pSpeed, pModbusVent->pSpeed, sizeof (MbusObj_t));
            }
            if (pModbusVent->pErrorCode) {
                pVent->pErrorCode = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pVent->pErrorCode) continue;
                memcpy (pVent->pErrorCode, pModbusVent->pErrorCode, sizeof (MbusObj_t));
            }
            pVent->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
            if (!pVent->pBase) continue;
            memcpy (pVent->pBase, pModbusVent->pBase, sizeof (MbusBase_t));
            pVent->pBase->pSerial = (Serial_t *) calloc (1, sizeof (Serial_t));
            if (!pVent->pBase->pSerial) continue;
            memcpy (pVent->pBase->pSerial, pModbusVent->pBase->pSerial, sizeof (Serial_t));
            if (pVent->pBase->bFullDuplex) {
                pVent->pBase->pSerial->open (pVent->pBase->pSerial);
                pVent->pBase->pSerial->recv (&pVent->pBase->pSerial);
            }
            xmlChar *pGroup = xmlGetProp (pCell, (xmlChar *) "group");
            if (pGroup) {
                pVent->pBase->bGroupMember = true;
                xmlFree (pGroup);
            }
            /* deep copy end */
            xmlNodePtr pKey = pCell->xmlChildrenNode;
            while (pKey) {
                if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "PrimaryKey")) {
                    char *pPrimaryKey = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (pPrimaryKey) {
                        pVent->pBase->element.primaryType = MODBUS_DEVICE_VENTILATION;
                        pVent->pBase->element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                        if (!pVent->pBase->element.pPrimaryKey) continue;
                        snprintf ((char *) pVent->pBase->element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_MODBUS, pPrimaryKey);
                        pVent->pBase->element.devId = strtol ((char *) pVent->pBase->element.pPrimaryKey + 12, NULL, 10);
                        logDbg ("mbus vent primary key:%s, devId=%d", pVent->pBase->element.pPrimaryKey, pVent->pBase->element.devId);
                    } else {
                        logErr ("mbus vent primary key is NULL!");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "ForeignKey")) {
                    pForeignKey = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pForeignKey) {
                        logErr ("mbus vent foreign key is NULL!");
                        continue;
                    }
                    pVent->pBase->element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pVent->pBase->element.pForeignKey) {
                        logErr ("mbus vent foreign key calloc failed!");
                        continue;
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "ForeignType")) {
                    char *pTmp = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pTmp) {
                        logErr ("get vent foreign type failed");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                        pVent->pBase->element.foreignType = DEVICE_TYPE_ZIGBEE;
                    } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                        pVent->pBase->element.foreignType = DEVICE_TYPE_MODBUS;
                    } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                        pVent->pBase->element.foreignType = DEVICE_TYPE_KNX;
                    } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                        pVent->pBase->element.foreignType = DEVICE_TYPE_RF433;
                    } else {
                        pVent->pBase->element.foreignType = DEVICE_TYPE_UNKNOWN;
                        logErr ("mbus vent foreign type is illegal!");
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "RoomNo")) {
                    char *pTmp = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pTmp) continue;
                    errno = 0;
                    pVent->pBase->regAddr = (uint16_t) strtol (pTmp, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus vent regAddr invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus vent RoomNo is %d", pVent->pBase->regAddr);
                }
                pKey = pKey->next;
            }
            if (pForeignKey && (pVent->pBase->element.foreignType != DEVICE_TYPE_UNKNOWN)) {
                snprintf ((char *) pVent->pBase->element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s",
                          g_Mac, pVent->pBase->element.foreignType, pForeignKey);
                logDbg ("mbus vent foreign key is %s", pVent->pBase->element.pForeignKey);
            }
            listAddNodeTail (pList, (void *) pVent);
        }
        pCell = pCell->next;
    }

    return 0;
}

int ParseMbusVentCfg (const char *pDocPath, list_t *pList) {
    xmlChar *pXpath = BAD_CAST ("/Device/Modbus/Ventilation/Cell");
    if (!pDocPath || !pList) return -1;

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        fprintf (stderr, "Document not parsed successful!");
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) { /* <Ventilation> <cell> */
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        xmlNodePtr pItem = NULL;
        MbusVent_t *pVent = (MbusVent_t *) calloc (1, sizeof (MbusVent_t));
        if (!pVent) {
            logErr ("[mbus Vent] MbusVent_t calloc fail!");
            continue;
        }
        pVent->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
        if (!pVent->pBase) {
            logErr ("[mbus Vent] MbusBase_t calloc fail!");
            continue;
        }
        pVent->pBase->pNext = NULL;
        pVent->pBase->pSerial = SerialInit ();
        if (!pVent->pBase->pSerial) {
            logErr ("[mbus Vent] SerialInit fail!");
            continue;
        }
        pVent->pBase->element.szName = "Ventilation";
        pVent->pBase->element.primaryType = MODBUS_DEVICE_VENTILATION;
        pVent->pBase->element.bMaster = false;
        pVent->pBase->bFeedback = true;
        pVent->pBase->bOnlineCheck = true;
        pVent->pBase->bFullDuplex = false;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pVent->pBase->element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pVent->pBase->element.szPlugin) {
                        logErr ("calloc mbus vent plugin failed");
                        continue;
                    }
                    strcpy ((char *) pVent->pBase->element.szPlugin, szPlugin);
                    //                    pVent->pBase->element.bMaster = true;
                    logDbg ("mbus vent plugin is %s", pVent->pBase->element.szPlugin);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pVent->pBase->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (pVent->pBase->element.pProductId) {
                        strcpy ((char *) pVent->pBase->element.pProductId, pProduct);
                        pVent->pBase->element.bMaster = true;
                        logDbg ("mbus vent product id is %s.", pVent->pBase->element.pProductId);
                    } else {
                        logErr ("mbus vent product id calloc failed");
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAddr")) {
                char *pDeviceAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDeviceAddr) {
                    errno = 0;
                    pVent->pBase->devAddr = (uint16_t) strtol (pDeviceAddr, NULL, 10);
                    if (errno != 0) {
                        pVent->pBase->devAddr = 0;
                        logErr ("mbus vent com id invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus vent dev id:%d", pVent->pBase->devAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Port")) {
                char *pPort = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPort) {
                    errno = 0;
                    int port = strtol (pPort, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus vent com port invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pVent->pBase->pSerial->setPort (pVent->pBase->pSerial, port);
                    if (ret != 0) {
                        logErr ("mbus vent com port(%d) invalid", port);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus vent dev com port:%d", port);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Baudrate")) {
                char *pBaudrate = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pBaudrate) {
                    errno = 0;
                    int baudrate = strtol (pBaudrate, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus vent com baudrate invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pVent->pBase->pSerial->setBaudrate (pVent->pBase->pSerial, baudrate);
                    if (ret != 0) {
                        logErr ("mbus vent com baudrate(%s) invalid", pBaudrate);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus vent dev baudrate:%d", baudrate);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Databit")) {
                char *pDatabit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDatabit) {
                    errno = 0;
                    int databit = strtol (pDatabit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus vent com pDatabit invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pVent->pBase->pSerial->setDatabits (pVent->pBase->pSerial, databit);
                    if (ret != 0) {
                        logErr ("mbus vent com databit(%d) invalid", databit);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus vent dev com databit:%d", databit);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Parity")) {
                char *pParity = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pParity) {
                    int ret = pVent->pBase->pSerial->setParity (pVent->pBase->pSerial, *pParity);
                    if (ret != 0) {
                        logErr ("mbus vent parity:%s illegal！\n", pParity);
                        exit (EXIT_FAILURE); /* force exit */
                    } else {
                        logDbg ("mbus vent parity:%s.", pParity);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Stopbit")) {
                char *pStopbit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pStopbit) {
                    errno = 0;
                    int stopbits = strtol (pStopbit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus vent com stopbits invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    int ret = pVent->pBase->pSerial->setStopbits (pVent->pBase->pSerial, stopbits);
                    if (ret != 0) {
                        logErr ("mbus vent com stopbits(%d) invalid", stopbits);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus vent dev com stopbits:%d", stopbits);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pVent->pBase->bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pVent->pBase->bOnlineCheck = false;
                    logDbg ("rs485 vent online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "FullDuplex")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'Y') || (*pCmd == 'y'))) {
                    pVent->pBase->bFullDuplex = true;
                    pVent->pBase->pSerial->setFullDuplex (pVent->pBase->pSerial, true);
                    logDbg ("Note: rs485 vent is full duplex!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Delay")) {
                char *pDelay = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDelay) {
                    errno = 0;
                    pVent->pBase->delay = strtol (pDelay, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus vent com delay invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Switch")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pVent->pSwitch = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pVent->pSwitch) {
                                logErr ("mbus vent calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pVent->pSwitch->localPoint = VENT_SWITCH;
                            pVent->pSwitch->type = MODBUS_DATA_TYPE_BOOL;
                            pVent->pSwitch->valueOld = (uint32_t) -1;
                            pVent->pSwitch->forceSync = 0;
                            errno = 0;
                            pVent->pSwitch->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus vent switch dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus vent switch dataPoint:%d", pVent->pSwitch->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Speed")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pVent->pSpeed = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pVent->pSpeed) {
                                logErr ("mbus vent calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pVent->pSpeed->localPoint = VENT_SPEED;
                            pVent->pSpeed->type = MODBUS_DATA_TYPE_ENUM;
                            pVent->pSpeed->valueOld = (uint32_t) -1;
                            pVent->pSpeed->forceSync = 0;
                            errno = 0;
                            pVent->pSpeed->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus vent speed dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus vent speed dataPoint:%d", pVent->pSpeed->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Humi")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pVent->pHumi = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pVent->pHumi) {
                                logErr ("mbus vent calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pVent->pHumi->localPoint = VENT_HUMI;
                            pVent->pHumi->type = MODBUS_DATA_TYPE_VALUE;
                            pVent->pHumi->valueOld = (uint32_t) -1;
                            pVent->pHumi->forceSync = 0;
                            errno = 0;
                            pVent->pHumi->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus vent humi dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus vent humi dataPoint:%d", pVent->pSpeed->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Mode")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pVent->pMode = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pVent->pMode) {
                                logErr ("mbus vent calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pVent->pMode->localPoint = VENT_MODE;
                            pVent->pMode->type = MODBUS_DATA_TYPE_ENUM;
                            pVent->pMode->valueOld = (uint32_t) -1;
                            pVent->pMode->forceSync = 0;
                            errno = 0;
                            pVent->pMode->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus vent mode dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus vent mode dataPoint:%d", pVent->pSpeed->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Valve")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pVent->pValve = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pVent->pValve) {
                                logErr ("mbus vent calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pVent->pValve->localPoint = VENT_VALVE;
                            pVent->pValve->type = MODBUS_DATA_TYPE_BOOL;
                            pVent->pValve->valueOld = (uint32_t) -1;
                            pVent->pValve->forceSync = 0;
                            errno = 0;
                            pVent->pValve->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus vent valve dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus vent valve dataPoint:%d", pVent->pSpeed->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "EnvHumi")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pVent->pEnvHumi = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pVent->pEnvHumi) {
                                logErr ("mbus vent calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pVent->pEnvHumi->localPoint = VENT_ENV_HUMI;
                            pVent->pEnvHumi->type = MODBUS_DATA_TYPE_VALUE;
                            pVent->pEnvHumi->valueOld = (uint32_t) -1;
                            pVent->pEnvHumi->forceSync = 0;
                            errno = 0;
                            pVent->pEnvHumi->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus vent env-humi dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus vent env-humi dataPoint:%d", pVent->pSpeed->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Fault")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pVent->pErrorCode = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pVent->pErrorCode) {
                                logErr ("mbus vent calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pVent->pErrorCode->localPoint = FREATURE_FAULT;
                            pVent->pErrorCode->type = MODBUS_DATA_TYPE_STR;
                            pVent->pErrorCode->valueOld = (uint32_t) -1;
                            pVent->pErrorCode->forceSync = 0;
                            errno = 0;
                            pVent->pErrorCode->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus vent error-code dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus vent speed dataPoint:%d", pVent->pSpeed->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Item")) {
                pItem = pNode;
            }
            pNode = pNode->next;
        }

        if (pItem) {
            int ret = ParseMbusVentKey (pItem, pVent, pList);
            if (ret != -1) {
                if (pVent->pBase) {
                    if (pVent->pBase->pSerial) {
                        free (pVent->pBase->pSerial);
                    }
                    free (pVent->pBase);
                }
                free (pVent);
            }
        }
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

static int ParseMbusHeatKey (const xmlNodePtr pItem, MbusHeat_t *pMbusHeat, list_t *pList) {
    if (!pItem || !pMbusHeat || !pMbusHeat->pBase || !pList) return -1;
    xmlNodePtr pCell = pItem->xmlChildrenNode;

    while (pCell) {
        if (!xmlStrcasecmp (pCell->name, (const xmlChar *) "Cell")) {
            char *pForeignKey = NULL;
            MbusHeat_t *pHeat = (MbusHeat_t *) calloc (1, sizeof (MbusHeat_t));
            if (!pHeat) {
                logErr ("mbus heat MbusHeat_t calloc fail!\n");
                continue;
            }

            /* deep copy */
            if (pMbusHeat->pSwitch) {
                pHeat->pSwitch = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pHeat->pSwitch) continue;
                memcpy (pHeat->pSwitch, pMbusHeat->pSwitch, sizeof (MbusObj_t));
            }
            if (pMbusHeat->pTemp) {
                pHeat->pTemp = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pHeat->pTemp) continue;
                memcpy (pHeat->pTemp, pMbusHeat->pTemp, sizeof (MbusObj_t));
            }
            if (pMbusHeat->pEnvTemp) {
                pHeat->pEnvTemp = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pHeat->pEnvTemp) continue;
                memcpy (pHeat->pEnvTemp, pMbusHeat->pEnvTemp, sizeof (MbusObj_t));
            }
            pHeat->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
            if (!pHeat->pBase) continue;
            memcpy (pHeat->pBase, pMbusHeat->pBase, sizeof (MbusBase_t));
            pHeat->pBase->pSerial = (Serial_t *) calloc (1, sizeof (Serial_t));
            if (!pHeat->pBase->pSerial) continue;
            memcpy (pHeat->pBase->pSerial, pMbusHeat->pBase->pSerial, sizeof (Serial_t));
            if (pHeat->pBase->bFullDuplex) {
                pHeat->pBase->pSerial->open (pHeat->pBase->pSerial);
                pHeat->pBase->pSerial->recv (&pHeat->pBase->pSerial);
            }
            xmlChar *pGroup = xmlGetProp (pCell, (xmlChar *) "group");
            if (pGroup) {
                pHeat->pBase->bGroupMember = true;
                xmlFree (pGroup);
            }
            /* deep copy end */
            xmlNodePtr pKey = pCell->xmlChildrenNode;
            while (pKey) {
                if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "PrimaryKey")) {
                    char *pPrimaryKey = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (pPrimaryKey) {
                        pHeat->pBase->element.primaryType = MODBUS_DEVICE_FLOORHEATING;
                        pHeat->pBase->element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                        if (!pHeat->pBase->element.pPrimaryKey) continue;
                        snprintf ((char *) pHeat->pBase->element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_MODBUS, pPrimaryKey);
                        pHeat->pBase->element.devId = strtol ((char *) pHeat->pBase->element.pPrimaryKey + 12, NULL, 10);
                        logDbg ("mbus heat primary key=%s, devId=%d", pHeat->pBase->element.pPrimaryKey, pHeat->pBase->element.devId);
                    } else {
                        logErr ("mbus heat primary key is NULL!");
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "ForeignKey")) {
                    pForeignKey = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pForeignKey) {
                        logErr ("mbus heat foreign key is NULL!");
                        continue;
                    }
                    pHeat->pBase->element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pHeat->pBase->element.pForeignKey) {
                        logErr ("mbus heat foreign key calloc failed!");
                        continue;
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "ForeignType")) {
                    char *pTmp = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pTmp) {
                        logErr ("get heat foreign type failed");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                        pHeat->pBase->element.foreignType = DEVICE_TYPE_ZIGBEE;
                    } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                        pHeat->pBase->element.foreignType = DEVICE_TYPE_MODBUS;
                    } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                        pHeat->pBase->element.foreignType = DEVICE_TYPE_KNX;
                    } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                        pHeat->pBase->element.foreignType = DEVICE_TYPE_RF433;
                    } else {
                        pHeat->pBase->element.foreignType = DEVICE_TYPE_UNKNOWN;
                        logErr ("mbus heat foreign type is illegal!");
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "RoomNo")) {
                    char *pTmp = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pTmp) continue;
                    errno = 0;
                    pHeat->pBase->regAddr = (uint16_t) strtol (pTmp, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat regAddr invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat RoomNo is %d", pHeat->pBase->regAddr);
                }
                pKey = pKey->next;
            }
            if (pForeignKey && (pHeat->pBase->element.foreignType != DEVICE_TYPE_UNKNOWN)) {
                snprintf ((char *) pHeat->pBase->element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s",
                          g_Mac, pHeat->pBase->element.foreignType, pForeignKey);
                logDbg ("mbus heat foreign key is %s", pHeat->pBase->element.pForeignKey);
            }
            listAddNodeTail (pList, (void *) pHeat);
        }
        pCell = pCell->next;
    }

    return 0;
}

int ParseMbusHeatCfg (const char *pDocPath, list_t *pList) {
    xmlChar *pXpath = BAD_CAST ("/Device/Modbus/FloorHeating/Cell");
    if (!pDocPath || !pList) return -1;

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        fprintf (stderr, "Document not parsed successful!");
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        xmlNodePtr pItem = NULL;
        MbusHeat_t *pHeat = (MbusHeat_t *) calloc (1, sizeof (MbusHeat_t));
        if (!pHeat) {
            logErr ("[mbus heat] MbusHeat_t calloc fail!");
            continue;
        }
        pHeat->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
        if (!pHeat->pBase) {
            logErr ("[mbus heat] MbusBase_t calloc fail!");
            continue;
        }
        pHeat->pBase->pNext = NULL;
        pHeat->pBase->pSerial = SerialInit ();
        if (!pHeat->pBase->pSerial) {
            logErr ("[mbus heat] SerialInit fail!");
            continue;
        }
        pHeat->pBase->element.szName = "FloorHeating";
        pHeat->pBase->element.primaryType = MODBUS_DEVICE_FLOORHEATING;
        pHeat->pBase->element.bMaster = false;
        pHeat->pBase->bFeedback = true;
        pHeat->pBase->bOnlineCheck = true;
        pHeat->pBase->bFullDuplex = false;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pHeat->pBase->element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pHeat->pBase->element.szPlugin) {
                        logErr ("calloc mbus heat plugin failed");
                        continue;
                    }
                    strcpy ((char *) pHeat->pBase->element.szPlugin, szPlugin);
                    //                    pHeat->pBase->element.bMaster = true;
                    logDbg ("mbus heat plugin is %s", pHeat->pBase->element.szPlugin);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pHeat->pBase->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pHeat->pBase->element.pProductId) {
                        logErr ("mbus heat product id calloc failed");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    strcpy ((char *) pHeat->pBase->element.pProductId, pProduct);
                    pHeat->pBase->element.bMaster = true;
                    logDbg ("mbus heat product id is %s.", pHeat->pBase->element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAddr")) {
                char *pDeviceAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDeviceAddr) {
                    errno = 0;
                    pHeat->pBase->devAddr = (uint16_t) strtol (pDeviceAddr, NULL, 10);
                    if (errno != 0) {
                        pHeat->pBase->devAddr = 0;
                        logErr ("mbus heat com id invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat dev id:%d", pHeat->pBase->devAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Port")) {
                char *pPort = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPort) {
                    errno = 0;
                    int port = strtol (pPort, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat com port invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pHeat->pBase->pSerial->setPort (pHeat->pBase->pSerial, port);
                    if (ret != 0) {
                        logErr ("mbus heat com port(%d) invalid", port);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat dev com port:%d", port);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Baudrate")) {
                char *pBaudrate = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pBaudrate) {
                    errno = 0;
                    int baudrate = strtol (pBaudrate, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat com baudrate invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pHeat->pBase->pSerial->setBaudrate (pHeat->pBase->pSerial, baudrate);
                    if (ret != 0) {
                        logErr ("mbus heat com baudrate(%s) invalid", pBaudrate);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat dev baudrate:%d", baudrate);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Databit")) {
                char *pDatabit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDatabit) {
                    errno = 0;
                    int databit = strtol (pDatabit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat com pDatabit invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pHeat->pBase->pSerial->setDatabits (pHeat->pBase->pSerial, databit);
                    if (ret != 0) {
                        logErr ("mbus heat com databit(%d) invalid", databit);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat dev com databit:%d", databit);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Parity")) {
                char *pParity = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pParity) {
                    int ret = pHeat->pBase->pSerial->setParity (pHeat->pBase->pSerial, *pParity);
                    if (ret != 0) {
                        logErr ("mbus heat parity:%s illegal！\n", pParity);
                        exit (EXIT_FAILURE); /* force exit */
                    } else {
                        logDbg ("mbus heat parity:%s.", pParity);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Stopbit")) {
                char *pStopbit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pStopbit) {
                    errno = 0;
                    int stopbits = strtol (pStopbit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat com stopbits invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    int ret = pHeat->pBase->pSerial->setStopbits (pHeat->pBase->pSerial, stopbits);
                    if (ret != 0) {
                        logErr ("mbus heat com stopbits(%d) invalid", stopbits);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat dev com stopbits:%d", stopbits);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pHeat->pBase->bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pHeat->pBase->bOnlineCheck = false;
                    logDbg ("rs485 heat online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "FullDuplex")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'Y') || (*pCmd == 'y'))) {
                    pHeat->pBase->bFullDuplex = true;
                    pHeat->pBase->pSerial->setFullDuplex (pHeat->pBase->pSerial, true);
                    logDbg ("Note: rs485 heat is full duplex!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Delay")) {
                char *pDelay = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDelay) {
                    errno = 0;
                    pHeat->pBase->delay = strtol (pDelay, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat com delay invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Switch")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pHeat->pSwitch = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pHeat->pSwitch) {
                                logErr ("mbus heat calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pHeat->pSwitch->localPoint = HEAT_SWITCH;
                            pHeat->pSwitch->type = MODBUS_DATA_TYPE_BOOL;
                            pHeat->pSwitch->valueOld = (uint32_t) -1;
                            pHeat->pSwitch->forceSync = 0;
                            errno = 0;
                            pHeat->pSwitch->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus heat switch dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus heat switch dataPoint:%d", pHeat->pSwitch->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Temp")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pHeat->pTemp = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pHeat->pTemp) {
                                logErr ("mbus heat calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pHeat->pTemp->localPoint = HEAT_TEMP;
                            pHeat->pTemp->type = MODBUS_DATA_TYPE_VALUE;
                            pHeat->pTemp->valueOld = (uint32_t) -1;
                            pHeat->pTemp->forceSync = 0;
                            errno = 0;
                            pHeat->pTemp->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus heat speed dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus heat temp dataPoint:%d", pHeat->pTemp->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "EnvTemp")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pHeat->pEnvTemp = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pHeat->pEnvTemp) {
                                logErr ("mbus heat calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pHeat->pEnvTemp->localPoint = HEAT_ENV_TEMP;
                            pHeat->pEnvTemp->type = MODBUS_DATA_TYPE_VALUE;
                            pHeat->pEnvTemp->valueOld = (uint32_t) -1;
                            pHeat->pEnvTemp->forceSync = 0;
                            errno = 0;
                            pHeat->pEnvTemp->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus heat speed dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus heat env dataPoint:%d", pHeat->pEnvTemp->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Fault")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pHeat->pErrorCode = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pHeat->pErrorCode) {
                                logErr ("mbus heat calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pHeat->pErrorCode->localPoint = HEAT_ENV_TEMP;
                            pHeat->pErrorCode->type = MODBUS_DATA_TYPE_VALUE;
                            pHeat->pErrorCode->valueOld = (uint32_t) -1;
                            pHeat->pErrorCode->forceSync = 0;
                            errno = 0;
                            pHeat->pErrorCode->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus heat error-code dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus heat env dataPoint:%d", pHeat->pEnvTemp->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Item")) {
                pItem = pNode;
            }
            pNode = pNode->next;
        }

        if (pItem) {
            int ret = ParseMbusHeatKey (pItem, pHeat, pList);
            if (ret != -1) {
                if (pHeat->pBase) {
                    if (pHeat->pBase->pSerial) {
                        free (pHeat->pBase->pSerial);
                    }
                    free (pHeat->pBase);
                }
                free (pHeat);
            }
        }
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

static int ParseMbusAqiKey (const xmlNodePtr pItem, MbusAqi_t *pMbusAqi, list_t *pList) {
    if (!pItem || !pMbusAqi || !pMbusAqi->pBase || !pList) return -1;
    xmlNodePtr pCell = pItem->xmlChildrenNode;

    while (pCell) {
        if (!xmlStrcasecmp (pCell->name, (const xmlChar *) "Cell")) {
            xmlNodePtr pKey = pCell->xmlChildrenNode;
            char *pForeignKey = NULL;
            MbusAqi_t *pAqi = (MbusAqi_t *) calloc (1, sizeof (MbusAqi_t));
            if (!pAqi) {
                logErr ("mbus aqi MbusAqi_t calloc fail!\n");
                continue;
            }

            /* deep copy */
            if (pMbusAqi->pHumi) {
                pAqi->pHumi = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pAqi->pHumi) continue;
                memcpy (pAqi->pHumi, pMbusAqi->pHumi, sizeof (MbusObj_t));
            }
            if (pMbusAqi->pTemp) {
                pAqi->pTemp = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pAqi->pTemp) continue;
                memcpy (pAqi->pTemp, pMbusAqi->pTemp, sizeof (MbusObj_t));
            }
            if (pMbusAqi->pPM25) {
                pAqi->pPM25 = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pAqi->pPM25) continue;
                memcpy (pAqi->pPM25, pMbusAqi->pPM25, sizeof (MbusObj_t));
            }
            if (pMbusAqi->pCO2) {
                pAqi->pCO2 = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pAqi->pCO2) continue;
                memcpy (pAqi->pCO2, pMbusAqi->pCO2, sizeof (MbusObj_t));
            }

            pAqi->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
            if (!pAqi->pBase) continue;
            memcpy (pAqi->pBase, pMbusAqi->pBase, sizeof (MbusBase_t));
            pAqi->pBase->pSerial = (Serial_t *) calloc (1, sizeof (Serial_t));
            if (!pAqi->pBase->pSerial) continue;
            memcpy (pAqi->pBase->pSerial, pMbusAqi->pBase->pSerial, sizeof (Serial_t));
            if (pAqi->pBase->bFullDuplex) {
                pAqi->pBase->pSerial->open (pAqi->pBase->pSerial);
                pAqi->pBase->pSerial->recv (&pAqi->pBase->pSerial);
            }
            /* deep copy end */

            while (pKey) {
                if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "PrimaryKey")) {
                    char *pPrimaryKey = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (pPrimaryKey) {
                        pAqi->pBase->element.primaryType = MODBUS_DEVICE_FLOORHEATING;
                        pAqi->pBase->element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                        if (!pAqi->pBase->element.pPrimaryKey) continue;
                        snprintf ((char *) pAqi->pBase->element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_MODBUS, pPrimaryKey);
                        pAqi->pBase->element.devId = strtol ((char *) pAqi->pBase->element.pPrimaryKey + 12, NULL, 10);
                        logDbg ("mbus heat primary key=%s, devId=%d", pAqi->pBase->element.pPrimaryKey, pAqi->pBase->element.devId);
                    } else {
                        logErr ("mbus heat primary key is NULL!");
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "ForeignKey")) {
                    pForeignKey = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pForeignKey) {
                        logErr ("mbus heat foreign key is NULL!");
                        continue;
                    }
                    pAqi->pBase->element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pAqi->pBase->element.pForeignKey) {
                        logErr ("mbus heat foreign key calloc failed!");
                        continue;
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "ForeignType")) {
                    char *pTmp = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pTmp) {
                        logErr ("get heat foreign type failed");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                        pAqi->pBase->element.foreignType = DEVICE_TYPE_ZIGBEE;
                    } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                        pAqi->pBase->element.foreignType = DEVICE_TYPE_MODBUS;
                    } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                        pAqi->pBase->element.foreignType = DEVICE_TYPE_KNX;
                    } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                        pAqi->pBase->element.foreignType = DEVICE_TYPE_RF433;
                    } else {
                        pAqi->pBase->element.foreignType = DEVICE_TYPE_UNKNOWN;
                        logErr ("mbus heat foreign type is illegal!");
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "RoomNo")) {
                    char *pTmp = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pTmp) continue;
                    errno = 0;
                    pAqi->pBase->regAddr = (uint16_t) strtol (pTmp, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat regAddr invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat RoomNo is %d", pAqi->pBase->regAddr);
                }
                pKey = pKey->next;
            }
            if (pForeignKey && (pAqi->pBase->element.foreignType != DEVICE_TYPE_UNKNOWN)) {
                snprintf ((char *) pAqi->pBase->element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s",
                          g_Mac, pAqi->pBase->element.foreignType, pForeignKey);
                logDbg ("mbus heat foreign key is %s", pAqi->pBase->element.pForeignKey);
            }
            listAddNodeTail (pList, (void *) pAqi);
        }
        pCell = pCell->next;
    }

    return 0;
}

int ParseMbusAqiCfg (const char *pDocPath, list_t *pList) {
    xmlChar *pXpath = BAD_CAST ("/Device/Modbus/AQI/Cell");
    if (!pDocPath || !pList) return -1;

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        fprintf (stderr, "Document not parsed successful!");
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        xmlNodePtr pItem = NULL;
        MbusAqi_t *pAqi = (MbusAqi_t *) calloc (1, sizeof (MbusAqi_t));
        if (!pAqi) {
            logErr ("[mbus aqi] MbusAqi_t calloc fail!");
            continue;
        }

        pAqi->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
        if (!pAqi->pBase) {
            logErr ("[mbus heat] MbusBase_t calloc fail!");
            continue;
        }

        pAqi->pBase->pSerial = SerialInit ();
        if (!pAqi->pBase->pSerial) {
            logErr ("[mbus heat] SerialInit fail!");
            continue;
        }
        pAqi->pBase->element.szName = "AQI";
        pAqi->pBase->element.primaryType = MODBUS_DEVICE_AQI;
        pAqi->pBase->element.bMaster = false;
        pAqi->pBase->bFeedback = true;
        pAqi->pBase->bOnlineCheck = true;
        pAqi->pBase->bFullDuplex = false;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pAqi->pBase->element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pAqi->pBase->element.szPlugin) {
                        logErr ("calloc mbus heat plugin failed");
                        continue;
                    }
                    strcpy ((char *) pAqi->pBase->element.szPlugin, szPlugin);
                    //                    pAqi->pBase->element.bMaster = true;
                    logDbg ("mbus heat plugin is %s", pAqi->pBase->element.szPlugin);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pAqi->pBase->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pAqi->pBase->element.pProductId) {
                        logErr ("mbus heat product id calloc failed");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    strcpy ((char *) pAqi->pBase->element.pProductId, pProduct);
                    pAqi->pBase->element.bMaster = true;
                    logDbg ("mbus heat product id is %s.", pAqi->pBase->element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAddr")) {
                char *pDeviceAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDeviceAddr) {
                    errno = 0;
                    pAqi->pBase->devAddr = (uint16_t) strtol (pDeviceAddr, NULL, 10);
                    if (errno != 0) {
                        pAqi->pBase->devAddr = 0;
                        logErr ("mbus heat com id invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat dev id:%d", pAqi->pBase->devAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Port")) {
                char *pPort = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPort) {
                    errno = 0;
                    int port = strtol (pPort, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat com port invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pAqi->pBase->pSerial->setPort (pAqi->pBase->pSerial, port);
                    if (ret != 0) {
                        logErr ("mbus heat com port(%d) invalid", port);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat dev com port:%d", port);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Baudrate")) {
                char *pBaudrate = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pBaudrate) {
                    errno = 0;
                    int baudrate = strtol (pBaudrate, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat com baudrate invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pAqi->pBase->pSerial->setBaudrate (pAqi->pBase->pSerial, baudrate);
                    if (ret != 0) {
                        logErr ("mbus heat com baudrate(%s) invalid", pBaudrate);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat dev baudrate:%d", baudrate);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Databit")) {
                char *pDatabit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDatabit) {
                    errno = 0;
                    int databit = strtol (pDatabit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat com pDatabit invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pAqi->pBase->pSerial->setDatabits (pAqi->pBase->pSerial, databit);
                    if (ret != 0) {
                        logErr ("mbus heat com databit(%d) invalid", databit);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat dev com databit:%d", databit);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Parity")) {
                char *pParity = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pParity) {
                    int ret = pAqi->pBase->pSerial->setParity (pAqi->pBase->pSerial, *pParity);
                    if (ret != 0) {
                        logErr ("mbus heat parity:%s illegal！\n", pParity);
                        exit (EXIT_FAILURE); /* force exit */
                    } else {
                        logDbg ("mbus heat parity:%s.", pParity);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Stopbit")) {
                char *pStopbit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pStopbit) {
                    errno = 0;
                    int stopbits = strtol (pStopbit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat com stopbits invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    int ret = pAqi->pBase->pSerial->setStopbits (pAqi->pBase->pSerial, stopbits);
                    if (ret != 0) {
                        logErr ("mbus heat com stopbits(%d) invalid", stopbits);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus heat dev com stopbits:%d", stopbits);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pAqi->pBase->bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pAqi->pBase->bOnlineCheck = false;
                    logDbg ("rs485 heat online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "FullDuplex")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'Y') || (*pCmd == 'y'))) {
                    pAqi->pBase->bFullDuplex = true;
                    pAqi->pBase->pSerial->setFullDuplex (pAqi->pBase->pSerial, true);
                    pAqi->pBase->pSerial->open (pAqi->pBase->pSerial);
                    pAqi->pBase->pSerial->recv (&pAqi->pBase->pSerial);
                    logDbg ("Note: rs485 heat is full duplex!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Delay")) {
                char *pDelay = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDelay) {
                    errno = 0;
                    pAqi->pBase->delay = strtol (pDelay, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus heat com delay invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Humi")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pAqi->pHumi = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pAqi->pHumi) {
                                logErr ("mbus heat calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pAqi->pHumi->localPoint = AQI_HUMI;
                            pAqi->pHumi->type = MODBUS_DATA_TYPE_VALUE;
                            pAqi->pHumi->valueOld = (uint32_t) -1;
                            pAqi->pHumi->forceSync = 0;
                            errno = 0;
                            pAqi->pHumi->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus heat switch dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus aqi humi dataPoint:%d", pAqi->pHumi->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Temp")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pAqi->pTemp = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pAqi->pTemp) {
                                logErr ("mbus heat calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pAqi->pTemp->localPoint = AQI_TEMP;
                            pAqi->pTemp->type = MODBUS_DATA_TYPE_VALUE;
                            pAqi->pTemp->valueOld = (uint32_t) -1;
                            pAqi->pTemp->forceSync = 0;
                            errno = 0;
                            pAqi->pTemp->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus heat speed dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus heat temp dataPoint:%d", pAqi->pTemp->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PM25")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pAqi->pPM25 = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pAqi->pPM25) {
                                logErr ("mbus heat calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pAqi->pPM25->localPoint = AQI_PM25;
                            pAqi->pPM25->type = MODBUS_DATA_TYPE_VALUE;
                            pAqi->pPM25->valueOld = (uint32_t) -1;
                            pAqi->pPM25->forceSync = 0;
                            errno = 0;
                            pAqi->pPM25->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus heat speed dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus heat env dataPoint:%d", pAqi->pPM25->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "CO2")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pAqi->pCO2 = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pAqi->pCO2) {
                                logErr ("mbus heat calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pAqi->pCO2->localPoint = AQI_CO2;
                            pAqi->pCO2->type = MODBUS_DATA_TYPE_VALUE;
                            pAqi->pCO2->valueOld = (uint32_t) -1;
                            pAqi->pCO2->forceSync = 0;
                            errno = 0;
                            pAqi->pCO2->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus heat speed dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus heat env dataPoint:%d", pAqi->pCO2->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Item")) {
                pItem = pNode;
            }
            pNode = pNode->next;
        }

        if (pItem) {
            int ret = ParseMbusAqiKey (pItem, pAqi, pList);
            if (ret != -1) {
                if (pAqi->pBase) {
                    if (pAqi->pBase->pSerial) {
                        free (pAqi->pBase->pSerial);
                    }
                    free (pAqi->pBase);
                }
                free (pAqi);
            }
        }
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

int ParseModbusHumidifierCfg (const char *pDocPath, list_t *pList) {
    xmlChar *pXpath = BAD_CAST ("/Device/Modbus/Humidifier/Cell");
    if (!pDocPath || !pList) return -1;

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        fprintf (stderr, "Document not parsed successful.");
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) { /* <Ventilation> <cell> */
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        char *pForeignKey = NULL;
        ModbusHumidifier_t *pHumidifier = (ModbusHumidifier_t *) calloc (1, sizeof (ModbusHumidifier_t));
        if (!pHumidifier) {
            logErr ("[mbus Humidifier] ModbusHumidifier_t calloc fail!");
            continue;
        }

        pHumidifier->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
        if (!pHumidifier->pBase) {
            logErr ("[mbus Humidifier] MbusBase_t calloc fail!");
            continue;
        }

        pHumidifier->pBase->pSerial = SerialInit ();
        if (!pHumidifier->pBase->pSerial) {
            logErr ("[mbus Humidifier] SerialInit fail!");
            continue;
        }
        pHumidifier->pBase->element.szName = "Humidifier";
        pHumidifier->pBase->element.primaryType = MODBUS_DEVICE_HUMIDIFIER;
        pHumidifier->pBase->element.bMaster = false;
        pHumidifier->pBase->bFeedback = true;
        pHumidifier->pBase->bOnlineCheck = true;
        pHumidifier->pBase->bFullDuplex = false;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pHumidifier->pBase->element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pHumidifier->pBase->element.szPlugin) {
                        logErr ("calloc mbus humidifier plugin failed");
                        continue;
                    }
                    strcpy ((char *) pHumidifier->pBase->element.szPlugin, szPlugin);
                    logDbg ("mbus humidifier plugin is %s", pHumidifier->pBase->element.szPlugin);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pHumidifier->pBase->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pHumidifier->pBase->element.pProductId) {
                        logErr ("calloc mbus humidifier product id failed");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    strcpy ((char *) pHumidifier->pBase->element.pProductId, pProduct);
                    pHumidifier->pBase->element.bMaster = true;
                    logDbg ("mbus humidifier product id is %s.", pHumidifier->pBase->element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                char *pPrimaryKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPrimaryKey) {
                    pHumidifier->pBase->element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pHumidifier->pBase->element.pPrimaryKey) {
                        logErr ("calloc mbus humidifier primary key failed");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    snprintf ((char *) pHumidifier->pBase->element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_MODBUS, pPrimaryKey);
                    pHumidifier->pBase->element.devId = strtol ((char *) pHumidifier->pBase->element.pPrimaryKey + 12, NULL, 10);
                    logDbg ("mbus humidifier primary key is %s", pHumidifier->pBase->element.pPrimaryKey);
                } else {
                    logErr ("mbus humidifier primary key is NULL!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pForeignKey) {
                    logErr ("mbus humidifier foreign key is NULL!");
                    continue;
                }
                pHumidifier->pBase->element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pHumidifier->pBase->element.pForeignKey) {
                    logErr ("mbus humidifier foreign key calloc failed!");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get humidifier foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pHumidifier->pBase->element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pHumidifier->pBase->element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pHumidifier->pBase->element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pHumidifier->pBase->element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    pHumidifier->pBase->element.foreignType = DEVICE_TYPE_UNKNOWN;
                    logErr ("mbus humidifier foreign type is illegal!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAddr")) {
                char *pDeviceAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDeviceAddr) {
                    errno = 0;
                    pHumidifier->pBase->devAddr = (uint16_t) strtol (pDeviceAddr, NULL, 10);
                    if (errno != 0) {
                        pHumidifier->pBase->devAddr = 0;
                        logErr ("mbus humidifier com id invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus humidifier dev id:%d", pHumidifier->pBase->devAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Port")) {
                char *pPort = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPort) {
                    errno = 0;
                    int port = strtol (pPort, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus humidifier com port invalid, rang[1 3]");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pHumidifier->pBase->pSerial->setPort (pHumidifier->pBase->pSerial, port);
                    if (ret != 0) {
                        logErr ("mbus humidifier com port(%d) invalid, rang[1 3]", port);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus humidifier dev com port:%d", port);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Baudrate")) {
                char *pBaudRate = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pBaudRate) {
                    errno = 0;
                    int baudRate = strtol (pBaudRate, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus humidifier com baudRate invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pHumidifier->pBase->pSerial->setBaudrate (pHumidifier->pBase->pSerial, baudRate);
                    if (ret != 0) {
                        logErr ("mbus humidifier com baudRate(%s) invalid", pBaudRate);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus humidifier dev baudRate:%d", baudRate);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Databit")) {
                char *pDataBit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDataBit) {
                    errno = 0;
                    int dataBit = strtol (pDataBit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus humidifier com pDataBit invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pHumidifier->pBase->pSerial->setDatabits (pHumidifier->pBase->pSerial, dataBit);
                    if (ret != 0) {
                        logErr ("mbus humidifier com dataBit(%d) invalid", dataBit);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus humidifier dev com dataBit:%d", dataBit);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Parity")) {
                char *pParity = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pParity) {
                    int ret = pHumidifier->pBase->pSerial->setParity (pHumidifier->pBase->pSerial, *pParity);
                    if (ret != 0) {
                        logErr ("mbus humidifier parity:%s illegal！\n", pParity);
                        exit (EXIT_FAILURE); /* force exit */
                    } else {
                        logDbg ("mbus humidifier parity:%s.", pParity);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Stopbit")) {
                char *pStopBit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pStopBit) {
                    errno = 0;
                    int stopBits = strtol (pStopBit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus humidifier com stopBits invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    int ret = pHumidifier->pBase->pSerial->setStopbits (pHumidifier->pBase->pSerial, stopBits);
                    if (ret != 0) {
                        logErr ("mbus humidifier com stopBits(%d) invalid", stopBits);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus humidifier dev com stopBits:%d", stopBits);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pHumidifier->pBase->bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pHumidifier->pBase->bOnlineCheck = false;
                    logDbg ("rs485 humidifier online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "FullDuplex")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'Y') || (*pCmd == 'y'))) {
                    pHumidifier->pBase->bFullDuplex = true;
                    pHumidifier->pBase->pSerial->setFullDuplex (pHumidifier->pBase->pSerial, true);
                    logDbg ("Note: rs485 humidifier don't support the full duplex mode!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Delay")) {
                char *pDelay = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDelay) {
                    errno = 0;
                    pHumidifier->pBase->delay = (uint16_t) strtol (pDelay, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus humidifier com delay invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Switch")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pHumidifier->pSwitch = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pHumidifier->pSwitch) {
                                logErr ("mbus humidifier calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pHumidifier->pSwitch->localPoint = HUMIDIFIER_SWITCH;
                            pHumidifier->pSwitch->type = MODBUS_DATA_TYPE_BOOL;
                            pHumidifier->pSwitch->valueOld = (uint32_t) -1;
                            pHumidifier->pSwitch->forceSync = 0;
                            errno = 0;
                            pHumidifier->pSwitch->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus humidifier switch dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus humidifier switch dataPoint:%d", pHumidifier->pSwitch->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Humidity")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pHumidifier->pHumidity = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pHumidifier->pHumidity) {
                                logErr ("mbus humidifier calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pHumidifier->pHumidity->localPoint = HUMIDIFIER_HUMIDITY_SET;
                            pHumidifier->pHumidity->type = MODBUS_DATA_TYPE_VALUE;
                            pHumidifier->pHumidity->valueOld = (uint32_t) -1;
                            pHumidifier->pHumidity->forceSync = 0;
                            errno = 0;
                            pHumidifier->pHumidity->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus humidifier humidity dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus humidifier humidity dataPoint:%d", pHumidifier->pHumidity->cloudPoint);
            }
            pNode = pNode->next;
        }

        if (pForeignKey && (pHumidifier->pBase->element.foreignType != DEVICE_TYPE_UNKNOWN)) {
            snprintf ((char *) pHumidifier->pBase->element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s",
                      g_Mac, pHumidifier->pBase->element.foreignType, pForeignKey);
            logDbg ("mbus humidifier foreign key is %s", pHumidifier->pBase->element.pForeignKey);
        }
        listAddNodeTail (pList, (void *) pHumidifier);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

int ParseModbusPurifierCfg (const char *pDocPath, list_t *pList) {
    xmlChar *pXpath = BAD_CAST ("/Device/Modbus/Purifier/Cell");
    if (!pDocPath || !pList) return -1;

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        fprintf (stderr, "Document not parsed successful.");
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) { /* <Ventilation> <cell> */
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        char *pForeignKey = NULL;
        ModbusPurifier_t *pPurifier = (ModbusPurifier_t *) calloc (1, sizeof (ModbusPurifier_t));
        if (!pPurifier) {
            logErr ("[mbus Purifier] ModbusPurifier_t calloc fail!");
            continue;
        }

        pPurifier->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
        if (!pPurifier->pBase) {
            logErr ("[mbus Purifier] MbusBase_t calloc fail!");
            continue;
        }

        pPurifier->pBase->pSerial = SerialInit ();
        if (!pPurifier->pBase->pSerial) {
            logErr ("[mbus Purifier] SerialInit fail!");
            continue;
        }
        pPurifier->pBase->element.szName = "Purifier";
        pPurifier->pBase->element.primaryType = MODBUS_DEVICE_PURIFIER;
        pPurifier->pBase->element.bMaster = false;
        pPurifier->pBase->bFeedback = true;
        pPurifier->pBase->bOnlineCheck = true;
        pPurifier->pBase->bFullDuplex = false;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pPurifier->pBase->element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pPurifier->pBase->element.szPlugin) {
                        logErr ("calloc mbus purifier plugin failed");
                        continue;
                    }
                    strcpy ((char *) pPurifier->pBase->element.szPlugin, szPlugin);
                    logDbg ("mbus purifier plugin is %s", pPurifier->pBase->element.szPlugin);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pPurifier->pBase->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pPurifier->pBase->element.pProductId) {
                        logErr ("calloc mbus purifier product id failed");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    strcpy ((char *) pPurifier->pBase->element.pProductId, pProduct);
                    pPurifier->pBase->element.bMaster = true;
                    logDbg ("mbus purifier product id is %s.", pPurifier->pBase->element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                char *pPrimaryKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPrimaryKey) {
                    pPurifier->pBase->element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pPurifier->pBase->element.pPrimaryKey) continue;
                    snprintf ((char *) pPurifier->pBase->element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_MODBUS, pPrimaryKey);
                    pPurifier->pBase->element.devId = strtol ((char *) pPurifier->pBase->element.pPrimaryKey + 12, NULL, 10);
                    logDbg ("mbus purifier primary key is %s", pPurifier->pBase->element.pPrimaryKey);
                } else {
                    logErr ("mbus purifier primary key is NULL!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pForeignKey) {
                    logErr ("mbus purifier foreign key is NULL!");
                    continue;
                }
                pPurifier->pBase->element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pPurifier->pBase->element.pForeignKey) {
                    logErr ("mbus purifier foreign key calloc failed!");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get purifier foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pPurifier->pBase->element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pPurifier->pBase->element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pPurifier->pBase->element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pPurifier->pBase->element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    pPurifier->pBase->element.foreignType = DEVICE_TYPE_UNKNOWN;
                    logErr ("mbus purifier foreign type is illegal!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAddr")) {
                char *pDeviceAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDeviceAddr) {
                    errno = 0;
                    pPurifier->pBase->devAddr = (uint16_t) strtol (pDeviceAddr, NULL, 10);
                    if (errno != 0) {
                        pPurifier->pBase->devAddr = 0;
                        logErr ("mbus purifier com id invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus purifier dev id:%d", pPurifier->pBase->devAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Port")) {
                char *pPort = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPort) {
                    errno = 0;
                    int port = strtol (pPort, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus purifier com port invalid, rang[1 3]");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pPurifier->pBase->pSerial->setPort (pPurifier->pBase->pSerial, port);
                    if (ret != 0) {
                        logErr ("mbus purifier com port(%d) invalid, rang[1 3]", port);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus purifier dev com port:%d", port);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Baudrate")) {
                char *pBaudRate = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pBaudRate) {
                    errno = 0;
                    int baudRate = strtol (pBaudRate, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus purifier com baudRate invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pPurifier->pBase->pSerial->setBaudrate (pPurifier->pBase->pSerial, baudRate);
                    if (ret != 0) {
                        logErr ("mbus purifier com baudRate(%s) invalid", pBaudRate);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus purifier dev baudRate:%d", baudRate);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Databit")) {
                char *pDataBit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDataBit) {
                    errno = 0;
                    int dataBit = strtol (pDataBit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus purifier com pDataBit invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pPurifier->pBase->pSerial->setDatabits (pPurifier->pBase->pSerial, dataBit);
                    if (ret != 0) {
                        logErr ("mbus purifier com dataBit(%d) invalid", dataBit);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus purifier dev com dataBit:%d", dataBit);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Parity")) {
                char *pParity = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pParity) {
                    int ret = pPurifier->pBase->pSerial->setParity (pPurifier->pBase->pSerial, *pParity);
                    if (ret != 0) {
                        logErr ("mbus purifier parity:%s illegal！\n", pParity);
                        exit (EXIT_FAILURE); /* force exit */
                    } else {
                        logDbg ("mbus purifier parity:%s.", pParity);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Stopbit")) {
                char *pStopBit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pStopBit) {
                    errno = 0;
                    int stopBits = strtol (pStopBit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus purifier com stopBits invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    int ret = pPurifier->pBase->pSerial->setStopbits (pPurifier->pBase->pSerial, stopBits);
                    if (ret != 0) {
                        logErr ("mbus purifier com stopBits(%d) invalid", stopBits);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus purifier dev com stopBits:%d", stopBits);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pPurifier->pBase->bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pPurifier->pBase->bOnlineCheck = false;
                    logDbg ("rs485 purifier online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "FullDuplex")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'Y') || (*pCmd == 'y'))) {
                    pPurifier->pBase->bFullDuplex = true;
                    pPurifier->pBase->pSerial->setFullDuplex (pPurifier->pBase->pSerial, true);
                    logDbg ("Note: rs485 purifier don't support the full duplex mode!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Delay")) {
                char *pDelay = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDelay) {
                    errno = 0;
                    pPurifier->pBase->delay = (uint16_t) strtol (pDelay, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus purifier com delay invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Flow")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pPurifier->pFlow = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pPurifier->pFlow) {
                                logErr ("mbus purifier calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pPurifier->pFlow->localPoint = PURIFIER_FLOW;
                            pPurifier->pFlow->type = MODBUS_DATA_TYPE_VALUE;
                            pPurifier->pFlow->valueOld = (uint32_t) -1;
                            pPurifier->pFlow->forceSync = 0;
                            errno = 0;
                            pPurifier->pFlow->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus purifier flow dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus purifier flow dataPoint:%d", pPurifier->pFlow->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "TDS")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pPurifier->pTDS = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pPurifier->pTDS) {
                                logErr ("mbus purifier calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pPurifier->pTDS->localPoint = PURIFIER_TDS;
                            pPurifier->pTDS->type = MODBUS_DATA_TYPE_VALUE;
                            pPurifier->pTDS->valueOld = (uint32_t) -1;
                            pPurifier->pTDS->forceSync = 0;
                            errno = 0;
                            pPurifier->pTDS->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus purifier TDS dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus purifier TDS dataPoint:%d", pPurifier->pTDS->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "FilterTime")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pPurifier->pFilterTime = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pPurifier->pFilterTime) {
                                logErr ("mbus purifier calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pPurifier->pFilterTime->localPoint = PURIFIER_PP_FILTER_TIME;
                            pPurifier->pFilterTime->type = MODBUS_DATA_TYPE_VALUE;
                            pPurifier->pFilterTime->valueOld = (uint32_t) -1;
                            pPurifier->pFilterTime->forceSync = 0;
                            errno = 0;
                            pPurifier->pFilterTime->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus purifier filter time dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus purifier filter time dataPoint:%d", pPurifier->pFilterTime->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Reset")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pPurifier->pReset = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pPurifier->pReset) {
                                logErr ("mbus purifier calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pPurifier->pReset->localPoint = PURIFIER_RESET;
                            pPurifier->pReset->type = MODBUS_DATA_TYPE_BOOL;
                            pPurifier->pReset->valueOld = (uint32_t) -1;
                            pPurifier->pReset->forceSync = 0;
                            errno = 0;
                            pPurifier->pReset->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus purifier reset dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus purifier reset time dataPoint:%d", pPurifier->pReset->cloudPoint);
            }
            pNode = pNode->next;
        }

        if (pForeignKey && (pPurifier->pBase->element.foreignType != DEVICE_TYPE_UNKNOWN)) {
            snprintf ((char *) pPurifier->pBase->element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s",
                      g_Mac, pPurifier->pBase->element.foreignType, pForeignKey);
            logDbg ("mbus humidifier foreign key is %s", pPurifier->pBase->element.pForeignKey);
        }
        listAddNodeTail (pList, (void *) pPurifier);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

static int ParseMbusPanelKey (const xmlNodePtr pItem, MbusPanel_t *pMbusPanel, list_t *pList) {
    if (!pItem || !pMbusPanel || !pMbusPanel->pBase || !pList) return -1;
    xmlNodePtr pCell = pItem->xmlChildrenNode;

    while (pCell) {
        if (!xmlStrcasecmp (pCell->name, (const xmlChar *) "Cell")) {
            xmlNodePtr pKey = pCell->xmlChildrenNode;
            char *pForeignKey = NULL;
            MbusPanel_t *pPanel = (MbusPanel_t *) calloc (1, sizeof (MbusPanel_t));
            if (!pPanel) {
                logErr ("mbus Panel MbusPanel_t calloc fail!\n");
                continue;
            }

            /* deep copy */
            if (pMbusPanel->pSwitch) {
                pPanel->pSwitch = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                if (!pPanel->pSwitch) continue;
                memcpy (pPanel->pSwitch, pMbusPanel->pSwitch, sizeof (MbusObj_t));
            }

            pPanel->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
            if (!pPanel->pBase) continue;
            memcpy (pPanel->pBase, pMbusPanel->pBase, sizeof (MbusBase_t));
            pPanel->pBase->pSerial = (Serial_t *) calloc (1, sizeof (Serial_t));
            if (!pPanel->pBase->pSerial) continue;
            memcpy (pPanel->pBase->pSerial, pMbusPanel->pBase->pSerial, sizeof (Serial_t));
            if (pPanel->pBase->bFullDuplex) {
                pPanel->pBase->pSerial->open (pPanel->pBase->pSerial);
                pPanel->pBase->pSerial->recv (&pPanel->pBase->pSerial);
            }
            /* deep copy end */

            while (pKey) {
                if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "PrimaryKey")) {
                    char *pPrimaryKey = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (pPrimaryKey) {
                        pPanel->pBase->element.primaryType = MODBUS_DEVICE_VENTILATION;
                        pPanel->pBase->element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                        if (!pPanel->pBase->element.pPrimaryKey) continue;
                        snprintf ((char *) pPanel->pBase->element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_MODBUS, pPrimaryKey);
                        pPanel->pBase->element.devId = strtol ((char *) pPanel->pBase->element.pPrimaryKey + 12, NULL, 10);
                        logDbg ("mbus panel primary key is %s", pPanel->pBase->element.pPrimaryKey);
                    } else {
                        logErr ("mbus panel primary key is NULL!");
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "ForeignKey")) {
                    pForeignKey = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pForeignKey) {
                        logErr ("mbus panel foreign key is NULL!");
                        continue;
                    }
                    pPanel->pBase->element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pPanel->pBase->element.pForeignKey) {
                        logErr ("mbus panel foreign key calloc failed!");
                        continue;
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "ForeignType")) {
                    char *pTmp = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pTmp) {
                        logErr ("get panel foreign type failed");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                        pPanel->pBase->element.foreignType = DEVICE_TYPE_ZIGBEE;
                    } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                        pPanel->pBase->element.foreignType = DEVICE_TYPE_MODBUS;
                    } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                        pPanel->pBase->element.foreignType = DEVICE_TYPE_KNX;
                    } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                        pPanel->pBase->element.foreignType = DEVICE_TYPE_RF433;
                    } else {
                        pPanel->pBase->element.foreignType = DEVICE_TYPE_UNKNOWN;
                        logErr ("mbus panel foreign type is illegal!");
                    }
                } else if (!xmlStrcasecmp (pKey->name, (const xmlChar *) "RoomNo")) {
                    char *pTmp = (char *) XML_GET_CONTENT (pKey->xmlChildrenNode);
                    if (!pTmp) continue;
                    errno = 0;
                    pPanel->pBase->regAddr = (uint16_t) strtol (pTmp, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus panel regAddr invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus panel RoomNo is %d", pPanel->pBase->regAddr);
                }
                pKey = pKey->next;
            }
            if (pForeignKey && (pPanel->pBase->element.foreignType != DEVICE_TYPE_UNKNOWN)) {
                snprintf ((char *) pPanel->pBase->element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s",
                          g_Mac, pPanel->pBase->element.foreignType, pForeignKey);
                logDbg ("mbus panel foreign key is %s", pPanel->pBase->element.pForeignKey);
            }
            listAddNodeTail (pList, (void *) pPanel);
        }
        pCell = pCell->next;
    }

    return 0;
}

int ParseMbusPanelCfg (const char *pDocPath, list_t *pList) {
    xmlChar *pXpath = BAD_CAST ("/Device/Modbus/Panel/Cell");
    if (!pDocPath || !pList) return -1;

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        fprintf (stderr, "Document not parsed successful!");
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) { /* <Panel> <cell> */
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        xmlNodePtr pItem = NULL;
        MbusPanel_t *pPanel = (MbusPanel_t *) calloc (1, sizeof (MbusPanel_t));
        if (!pPanel) {
            logErr ("[mbus Panel] MbusPanel_t calloc fail!");
            continue;
        }

        pPanel->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
        if (!pPanel->pBase) {
            logErr ("[mbus Panel] MbusBase_t calloc fail!");
            continue;
        }

        pPanel->pBase->pSerial = SerialInit ();
        if (!pPanel->pBase->pSerial) {
            logErr ("[mbus Panel] SerialInit fail!");
            continue;
        }
        pPanel->pBase->element.szName = "Panel";
        pPanel->pBase->element.primaryType = MODBUS_DEVICE_PANNEL;
        pPanel->pBase->element.bMaster = false;
        pPanel->pBase->bFeedback = true;
        pPanel->pBase->bOnlineCheck = true;
        pPanel->pBase->bFullDuplex = false;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pPanel->pBase->element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pPanel->pBase->element.szPlugin) {
                        logErr ("calloc mbus Panel plugin failed");
                        continue;
                    }
                    strcpy ((char *) pPanel->pBase->element.szPlugin, szPlugin);
                    //                    pPanel->pBase->element.bMaster = true;
                    logDbg ("mbus Panel plugin is %s", pPanel->pBase->element.szPlugin);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pPanel->pBase->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pPanel->pBase->element.pProductId) {
                        logErr ("calloc mbus Panel product id failed");
                        exit (EXIT_FAILURE);
                    }
                    strcpy ((char *) pPanel->pBase->element.pProductId, pProduct);
                    pPanel->pBase->element.bMaster = true;
                    logDbg ("mbus Panel product id is %s.", pPanel->pBase->element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAddr")) {
                char *pDeviceAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDeviceAddr) {
                    errno = 0;
                    pPanel->pBase->devAddr = (uint16_t) strtol (pDeviceAddr, NULL, 10);
                    if (errno != 0) {
                        pPanel->pBase->devAddr = 0;
                        logErr ("mbus Panel com id invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus Panel dev id:%d", pPanel->pBase->devAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Port")) {
                char *pPort = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPort) {
                    errno = 0;
                    int port = strtol (pPort, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus Panel com port invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pPanel->pBase->pSerial->setPort (pPanel->pBase->pSerial, port);
                    if (ret != 0) {
                        logErr ("mbus Panel com port(%d) invalid", port);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus Panel dev com port:%d", port);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Baudrate")) {
                char *pBaudrate = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pBaudrate) {
                    errno = 0;
                    int baudrate = strtol (pBaudrate, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus Panel com baudrate invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pPanel->pBase->pSerial->setBaudrate (pPanel->pBase->pSerial, baudrate);
                    if (ret != 0) {
                        logErr ("mbus Panel com baudrate(%s) invalid", pBaudrate);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus Panel dev baudrate:%d", baudrate);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Databit")) {
                char *pDatabit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDatabit) {
                    errno = 0;
                    int databit = strtol (pDatabit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus Panel com pDatabit invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pPanel->pBase->pSerial->setDatabits (pPanel->pBase->pSerial, databit);
                    if (ret != 0) {
                        logErr ("mbus Panel com databit(%d) invalid", databit);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus Panel dev com databit:%d", databit);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Parity")) {
                char *pParity = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pParity) {
                    int ret = pPanel->pBase->pSerial->setParity (pPanel->pBase->pSerial, *pParity);
                    if (ret != 0) {
                        logErr ("mbus Panel parity:%s illegal！\n", pParity);
                        exit (EXIT_FAILURE); /* force exit */
                    } else {
                        logDbg ("mbus Panel parity:%s.", pParity);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Stopbit")) {
                char *pStopbit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pStopbit) {
                    errno = 0;
                    int stopbits = strtol (pStopbit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus Panel com stopbits invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    int ret = pPanel->pBase->pSerial->setStopbits (pPanel->pBase->pSerial, stopbits);
                    if (ret != 0) {
                        logErr ("mbus Panel com stopbits(%d) invalid", stopbits);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus Panel dev com stopbits:%d", stopbits);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pPanel->pBase->bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pPanel->pBase->bOnlineCheck = false;
                    logDbg ("rs485 Panel online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "FullDuplex")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'Y') || (*pCmd == 'y'))) {
                    pPanel->pBase->bFullDuplex = true;
                    pPanel->pBase->pSerial->setFullDuplex (pPanel->pBase->pSerial, true);
                    logDbg ("Note: rs485 Panel is full duplex!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Delay")) {
                char *pDelay = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDelay) {
                    errno = 0;
                    pPanel->pBase->delay = strtol (pDelay, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus Panel com delay invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Switch")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pPanel->pSwitch = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pPanel->pSwitch) {
                                logErr ("mbus Panel calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pPanel->pSwitch->localPoint = PANEL_SWITCH;
                            pPanel->pSwitch->type = MODBUS_DATA_TYPE_BOOL;
                            pPanel->pSwitch->valueOld = (uint32_t) -1;
                            pPanel->pSwitch->forceSync = 0;
                            errno = 0;
                            pPanel->pSwitch->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus Panel switch dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus Panel switch dataPoint:%d", pPanel->pSwitch->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Item")) {
                pItem = pNode;
            }
            pNode = pNode->next;
        }

        if (pItem) {
            int ret = ParseMbusPanelKey (pItem, pPanel, pList);
            if (ret != -1) {
                if (pPanel->pBase) {
                    if (pPanel->pBase->pSerial) {
                        free (pPanel->pBase->pSerial);
                    }
                    free (pPanel->pBase);
                }
                free (pPanel);
            }
        }
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

int ParseMbusCurtainCfg (const char *pDocPath, list_t *pList) {
    xmlChar *pXpath = BAD_CAST ("/Device/Modbus/Curtain/Cell");
    if (!pDocPath || !pList) return -1;

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        fprintf (stderr, "Document not parsed successful.");
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) { /* <Ventilation> <cell> */
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        char *pForeignKey = NULL;
        MbusCurtain_t *pCurtain = (MbusCurtain_t *) calloc (1, sizeof (MbusCurtain_t));
        if (!pCurtain) {
            logErr ("[mbus Vent] MbusCurtain_t calloc fail!");
            continue;
        }

        pCurtain->pBase = (MbusBase_t *) calloc (1, sizeof (MbusBase_t));
        if (!pCurtain->pBase) {
            logErr ("[mbus Vent] MbusBase_t calloc fail!");
            continue;
        }

        pCurtain->pBase->pSerial = SerialInit ();
        if (!pCurtain->pBase->pSerial) {
            logErr ("[mbus Vent] SerialInit fail!");
            continue;
        }
        pCurtain->pBase->element.szName = "Curtain";
        pCurtain->pBase->element.primaryType = MODBUS_DEVICE_CURTAIN;
        pCurtain->pBase->element.bMaster = false;
        pCurtain->pBase->bFeedback = true;
        pCurtain->pBase->bOnlineCheck = true;
        pCurtain->pBase->bFullDuplex = false;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pCurtain->pBase->element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pCurtain->pBase->element.szPlugin) {
                        logErr ("calloc mbus vent plugin failed");
                        continue;
                    }
                    strcpy ((char *) pCurtain->pBase->element.szPlugin, szPlugin);
                    logDbg ("mbus curtain plugin is %s", pCurtain->pBase->element.szPlugin);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pCurtain->pBase->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pCurtain->pBase->element.pProductId) {
                        logErr ("calloc mbus curtain product id failed");
                        exit (EXIT_FAILURE);
                    }
                    strcpy ((char *) pCurtain->pBase->element.pProductId, pProduct);
                    pCurtain->pBase->element.bMaster = true;
                    logDbg ("mbus curtain product id is %s.", pCurtain->pBase->element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                char *pPrimaryKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPrimaryKey) {
                    pCurtain->pBase->element.primaryType = MODBUS_DEVICE_CURTAIN;
                    pCurtain->pBase->element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pCurtain->pBase->element.pPrimaryKey) continue;
                    snprintf ((char *) pCurtain->pBase->element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_MODBUS, pPrimaryKey);
                    pCurtain->pBase->element.devId = strtol (pCurtain->pBase->element.pPrimaryKey + 12, NULL, 10);
                    logDbg ("mbus curtain primary key=%s, devId=%d", pCurtain->pBase->element.pPrimaryKey, pCurtain->pBase->element.devId);
                } else {
                    logErr ("mbus curtain primary key is NULL!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pForeignKey) {
                    logErr ("mbus curtain foreign key is NULL!");
                    continue;
                }
                pCurtain->pBase->element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pCurtain->pBase->element.pForeignKey) {
                    logErr ("mbus curtain foreign key calloc failed!");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get curtain foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pCurtain->pBase->element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pCurtain->pBase->element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pCurtain->pBase->element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pCurtain->pBase->element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    pCurtain->pBase->element.foreignType = DEVICE_TYPE_UNKNOWN;
                    logErr ("mbus curtain foreign type is illegal!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAddr")) {
                char *pDeviceAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDeviceAddr) {
                    errno = 0;
                    pCurtain->pBase->devAddr = (uint16_t) strtol (pDeviceAddr, NULL, 10);
                    if (errno != 0) {
                        pCurtain->pBase->devAddr = 0;
                        logErr ("mbus curtain com id invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus curtain dev id:%d", pCurtain->pBase->devAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Port")) {
                char *pPort = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPort) {
                    errno = 0;
                    int port = strtol (pPort, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus curtain com port invalid, rang[1 3]");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pCurtain->pBase->pSerial->setPort (pCurtain->pBase->pSerial, port);
                    if (ret != 0) {
                        logErr ("mbus curtain com port(%d) invalid, rang[1 3]", port);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus curtain dev com port:%d", port);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Baudrate")) {
                char *pBaudrate = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pBaudrate) {
                    errno = 0;
                    int baudrate = strtol (pBaudrate, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus curtain com baudrate invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pCurtain->pBase->pSerial->setBaudrate (pCurtain->pBase->pSerial, baudrate);
                    if (ret != 0) {
                        logErr ("mbus curtain com baudrate(%s) invalid", pBaudrate);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus curtain dev baudrate:%d", baudrate);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Databit")) {
                char *pDatabit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDatabit) {
                    errno = 0;
                    int databit = strtol (pDatabit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus curtain com pDatabit invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }

                    int ret = pCurtain->pBase->pSerial->setDatabits (pCurtain->pBase->pSerial, databit);
                    if (ret != 0) {
                        logErr ("mbus curtain com databit(%d) invalid", databit);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus curtain dev com databit:%d", databit);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Parity")) {
                char *pParity = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pParity) {
                    int ret = pCurtain->pBase->pSerial->setParity (pCurtain->pBase->pSerial, *pParity);
                    if (ret != 0) {
                        logErr ("mbus curtain parity:%s illegal！\n", pParity);
                        exit (EXIT_FAILURE); /* force exit */
                    } else {
                        logDbg ("mbus curtain parity:%s.", pParity);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Stopbit")) {
                char *pStopbit = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pStopbit) {
                    errno = 0;
                    int stopbits = strtol (pStopbit, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus curtain com stopbits invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    int ret = pCurtain->pBase->pSerial->setStopbits (pCurtain->pBase->pSerial, stopbits);
                    if (ret != 0) {
                        logErr ("mbus curtain com stopbits(%d) invalid", stopbits);
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    logDbg ("mbus curtain dev com stopbits:%d", stopbits);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pCurtain->pBase->bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'N') || (*pCmd == 'n'))) {
                    pCurtain->pBase->bOnlineCheck = false;
                    logDbg ("rs485 curtain online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "FullDuplex")) {
                char *pCmd = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pCmd && ((*pCmd == 'Y') || (*pCmd == 'y'))) {
                    pCurtain->pBase->bFullDuplex = true;
                    pCurtain->pBase->pSerial->setFullDuplex (pCurtain->pBase->pSerial, true);
                    logDbg ("Note: rs485 curtain don't support the fullduplex mode!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Delay")) {
                char *pDelay = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pDelay) {
                    errno = 0;
                    pCurtain->pBase->delay = (uint16_t) strtol (pDelay, NULL, 10);
                    if (errno != 0) {
                        logErr ("mbus curtain com delay invalid");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Switch")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pCurtain->pSwitch = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pCurtain->pSwitch) {
                                logErr ("mbus curtain calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pCurtain->pSwitch->localPoint = CURTAIN_SWITCH;
                            pCurtain->pSwitch->type = MODBUS_DATA_TYPE_ENUM;
                            pCurtain->pSwitch->valueOld = (uint32_t) -1;
                            pCurtain->pSwitch->forceSync = 0;
                            errno = 0;
                            pCurtain->pSwitch->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus curtain switch dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus curtain switch dataPoint:%d", pCurtain->pSwitch->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Fault")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pCurtain->pFault = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pCurtain->pFault) {
                                logErr ("mbus curtain calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pCurtain->pFault->localPoint = FREATURE_FAULT;
                            pCurtain->pFault->type = MODBUS_DATA_TYPE_VALUE;
                            pCurtain->pFault->valueOld = (uint32_t) -1;
                            pCurtain->pFault->forceSync = 0;
                            errno = 0;
                            pCurtain->pFault->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus curtain fault dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus curtain fault dataPoint:%d", pCurtain->pFault->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Direction")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pCurtain->pDirec = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pCurtain->pDirec) {
                                logErr ("mbus curtain calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pCurtain->pDirec->localPoint = CURTAIN_DIRECTION;
                            pCurtain->pDirec->type = MODBUS_DATA_TYPE_BOOL;
                            pCurtain->pDirec->valueOld = 0;
                            pCurtain->pDirec->forceSync = 0;
                            errno = 0;
                            pCurtain->pDirec->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus curtain direction dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus curtain switch dataPoint:%d", pCurtain->pDirec->cloudPoint);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Position")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            pCurtain->pPos = (MbusObj_t *) calloc (1, sizeof (MbusObj_t));
                            if (!pCurtain->pPos) {
                                logErr ("mbus curtain calloc fail!\n");
                                exit (EXIT_FAILURE); /* force to exit */
                            }
                            pCurtain->pPos->localPoint = FEATURE_POS;
                            pCurtain->pPos->type = MODBUS_DATA_TYPE_STR;
                            pCurtain->pPos->valueOld = (uint32_t) -1;
                            pCurtain->pPos->forceSync = 0;
                            errno = 0;
                            pCurtain->pPos->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("mbus curtain positon dataPoint(%s) invalid", pDataPoint);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }

                    pChild = pChild->next;
                }
                logDbg ("mbus curtain switch dataPoint:%d", pCurtain->pPos->cloudPoint);
            }
            pNode = pNode->next;
        }

        if (pForeignKey && (pCurtain->pBase->element.foreignType != DEVICE_TYPE_UNKNOWN)) {
            snprintf ((char *) pCurtain->pBase->element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s",
                      g_Mac, pCurtain->pBase->element.foreignType, pForeignKey);
            logDbg ("mbus curtain foreign key is %s", pCurtain->pBase->element.pForeignKey);
        }
        listAddNodeTail (pList, (void *) pCurtain);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

int ParseKnxAcdCfg (const char *pDocPath, list_t *pList) {
    xmlDocPtr pDoc;
    xmlChar *pXpath = BAD_CAST ("/Device/KNX/AirCondition/Cell");
    xmlXPathObjectPtr pXpathObj = NULL;
    xmlNodeSetPtr pNodeset = NULL;
    xmlNodePtr pNode = NULL;
    int ret = -1;

    if (!pDocPath || !pList) {
        logErr ("ParseKnxAcdCfg invalid param!");
        return -1;
    }
    pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }
    pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }
    pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        char *pForeignKey = NULL;
        KnxAcd_t *pAcd = (KnxAcd_t *) calloc (1, sizeof (KnxAcd_t));
        if (!pAcd) {
            logErr ("ParseKnxAcdCfg calloc fail!");
            continue;
        }
        pAcd->base.element.szName = "Aircondition";
        pAcd->base.element.primaryType = KNX_DEVICE_AIRCONDITION;
        pAcd->base.element.bMaster = false;
        pAcd->base.bFeedback = true;
        pAcd->base.bOnlineCheck = true;
        pAcd->base.element.attri = DEVICE_ATTRI_CONTROLLER;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pAcd->base.element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pAcd->base.element.szPlugin) {
                        logErr ("calloc mbus acd plugin failed");
                        continue;
                    }
                    strcpy ((char *) pAcd->base.element.szPlugin, szPlugin);
                    logDbg ("knx acd plugin:%s", pAcd->base.element.szPlugin);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pAcd->base.element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pAcd->base.element.pProductId) {
                        logErr ("calloc knx acd product id failed");
                        exit (EXIT_FAILURE);
                    }
                    strcpy ((char *) pAcd->base.element.pProductId, pProduct);
                    pAcd->base.element.bMaster = true;
                    pAcd->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx acd product id:%s", pAcd->base.element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PhyAddr")) {
                char *pPhyAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPhyAddr) {
                    strncpy (pAcd->base.phyAddr, pPhyAddr, MIN (EIB_ADDR, strlen (pPhyAddr)));
                    pAcd->base.phy = KnxHwAddr (pAcd->base.phyAddr);
                    logDbg ("knx acd phy addr:%s", pAcd->base.phyAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                char *pItem = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pItem) {
                    pAcd->base.element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pAcd->base.element.pPrimaryKey) {
                        logErr ("ParseKnxAcdCfg calloc fail!");
                        continue;
                    }
                    snprintf ((char *) pAcd->base.element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_KNX, pItem);
                    pAcd->base.element.devId = strtol (pAcd->base.element.pPrimaryKey + 12, NULL, 10);
                    logDbg ("knx acd primary key=%s, devId=%d", pAcd->base.element.pPrimaryKey, pAcd->base.element.devId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pForeignKey) {
                    logErr ("knx acd foreign key is null!");
                    continue;
                }
                pAcd->base.element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pAcd->base.element.pForeignKey) {
                    logErr ("knx acd foreign key calloc failed!");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get acd foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pAcd->base.element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pAcd->base.element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pAcd->base.element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pAcd->base.element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    logErr ("knx Acd ForeignType:%s is illegal!", pTmp);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pFB = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pFB && ((*pFB == 'N') || (*pFB == 'n'))) {
                    pAcd->base.bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pOnline = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pOnline && ((*pOnline == 'N') || (*pOnline == 'n'))) {
                    pAcd->base.bOnlineCheck = false;
                    logDbg ("knx acd online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAttr")) {
                char *pAttri = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pAttri && ((*pAttri == 'C') || (*pAttri == 'c'))) {
                    pAcd->base.element.attri = DEVICE_ATTRI_CONTROLLER;
                    logDbg ("knx acd attribute is controller!");
                } else if (pAttri && ((*pAttri == 'A') || (*pAttri == 'a'))) {
                    pAcd->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx acd attribute is actuator!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Switch")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pSwitch = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pSwitch) {
                            ret = isKnxGroupAddr (pSwitch);
                            if (ret != 0) {
                                logErr ("knx acd switch ctrl group is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pAcd->pSwitch = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pAcd->pSwitch) {
                                logErr ("knx acd switch ctrl calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pAcd->pSwitch->addr, pSwitch, MIN (strlen (pSwitch), EIB_ADDR));
                            pAcd->pSwitch->localPoint = ACD_SWITCH;
                            pAcd->pSwitch->type = KNX_DATA_TYPE_1BIT;
                            logDbg ("knx acd switch ctrl: group=%s, localPoint=%d, type=%d", pAcd->pSwitch->addr, pAcd->pSwitch->localPoint, pAcd->pSwitch->type);
                        } else {
                            logErr ("knx acd switch ctrl group not exist!");
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Status")) {
                        char *pStatus = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pStatus) {
                            ret = isKnxGroupAddr (pStatus);
                            if (ret != 0) {
                                logErr ("knx acd switch status group is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pAcd->pSwitchStatus = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pAcd->pSwitchStatus) {
                                logErr ("knx acd switch status calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pAcd->pSwitchStatus->addr, pStatus, MIN (strlen (pStatus), EIB_ADDR));
                            pAcd->pSwitchStatus->localPoint = ACD_SWITCH_STATUS;
                            pAcd->pSwitchStatus->type = KNX_DATA_TYPE_1BIT;
                            pAcd->pSwitchStatus->threshold = 500;
#if 0
                            clock_gettime (CLOCK_MONOTONIC, &pAcd->pSwitchStatus->timestamp);
                            kv_set(pAcd->pSwitchStatus->addr, NULL, 500);
#endif
                            logDbg ("knx acd switch status: group=%s, localPoint=%d, type=%d", pAcd->pSwitchStatus->addr, pAcd->pSwitchStatus->localPoint, pAcd->pSwitchStatus->type);
                        } else {
                            logErr ("knx acd switch status group not exist!");
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx acd switch dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }

                if ((dp > 0) && (dp < 200)) {
                    if (pAcd->pSwitch) {
                        pAcd->pSwitch->cloudPoint = dp;
                    }
                    if (pAcd->pSwitchStatus) {
                        pAcd->pSwitchStatus->cloudPoint = dp;
                    }
                } else {
                    if (pAcd->pSwitch) {
                        free (pAcd->pSwitch);
                        pAcd->pSwitch = NULL;
                    }
                    if (pAcd->pSwitchStatus) {
                        free (pAcd->pSwitchStatus);
                        pAcd->pSwitchStatus = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Speed")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pSpeed = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pSpeed) {
                            ret = isKnxGroupAddr (pSpeed);
                            if (ret != 0) {
                                logErr ("knx acd speed ctrl group is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }

                            pAcd->pSpeed = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pAcd->pSpeed) {
                                logErr ("knx acd speed ctrl calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pAcd->pSpeed->addr, pSpeed, MIN (strlen (pSpeed), EIB_ADDR));
                            pAcd->pSpeed->localPoint = ACD_SPEED;
                            pAcd->pSpeed->type = KNX_DATA_TYPE_1BYTE;
                            logDbg ("knx acd Speed ctrl:group=%s, localPoint=%d, type=%d",
                                    pAcd->pSpeed->addr, pAcd->pSpeed->localPoint, pAcd->pSpeed->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Status")) {
                        char *pStatus = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pStatus) {
                            ret = isKnxGroupAddr (pStatus);
                            if (ret != 0) {
                                logErr ("knx acd speed status group is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pAcd->pSpeedStatus = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pAcd->pSpeedStatus) {
                                logErr ("knx acd speed status calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pAcd->pSpeedStatus->addr, pStatus, MIN (strlen (pStatus), EIB_ADDR));
                            pAcd->pSpeedStatus->localPoint = ACD_SPEED_STATUS;
                            pAcd->pSpeedStatus->type = KNX_DATA_TYPE_1BYTE;
                            pAcd->pSpeedStatus->threshold = 500;
#if 0
                            kv_set(pAcd->pSpeedStatus->addr, NULL, 500);
                            clock_gettime (CLOCK_MONOTONIC, &pAcd->pSpeedStatus->timestamp);
#endif
                            logDbg ("knx acd Speed status:group=%s, localPoint=%d, type=%d",
                                    pAcd->pSpeedStatus->addr, pAcd->pSpeedStatus->localPoint,
                                    pAcd->pSpeedStatus->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx acd switch dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }

                if ((dp > 0) && (dp < 200)) {
                    if (pAcd->pSpeed) {
                        pAcd->pSpeed->cloudPoint = dp;
                    }
                    if (pAcd->pSpeedStatus) {
                        pAcd->pSpeedStatus->cloudPoint = dp;
                    }
                } else {
                    if (pAcd->pSpeed) {
                        free (pAcd->pSpeed);
                        pAcd->pSpeed = NULL;
                    }
                    if (pAcd->pSpeedStatus) {
                        free (pAcd->pSpeedStatus);
                        pAcd->pSpeedStatus = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Mode")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pMode = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pMode) {
                            ret = isKnxGroupAddr (pMode);
                            if (ret != 0) {
                                logErr ("knx acd mode ctrl group is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pAcd->pMode = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pAcd->pMode) {
                                logErr ("knx acd mode status calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pAcd->pMode->addr, pMode, MIN (strlen (pMode), EIB_ADDR));
                            pAcd->pMode->localPoint = ACD_MODE;
                            pAcd->pMode->type = KNX_DATA_TYPE_1BYTE;
                            logDbg ("knx acd mode ctrl: group:%s, localPoint:%d, type:%d",
                                    pAcd->pMode->addr, pAcd->pMode->localPoint, pAcd->pMode->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Status")) {
                        char *pModeStatus = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pModeStatus) {
                            ret = isKnxGroupAddr (pModeStatus);
                            if (ret != 0) {
                                logErr ("knx acd mode status group is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pAcd->pModeStatus = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pAcd->pModeStatus) {
                                logErr ("knx acd mode status calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pAcd->pModeStatus->addr, pModeStatus, MIN (strlen (pModeStatus), EIB_ADDR));
                            pAcd->pModeStatus->localPoint = ACD_MODE_STATUS;
                            pAcd->pModeStatus->type = KNX_DATA_TYPE_1BYTE;
                            pAcd->pModeStatus->threshold = 3000;
#if 0
                            kv_set(pAcd->pModeStatus->addr, NULL, 3000);
                            clock_gettime (CLOCK_MONOTONIC, &pAcd->pModeStatus->timestamp);
#endif
                            logDbg ("knx acd mode status: group:%s, localPoint:%d, type:%d",
                                    pAcd->pModeStatus->addr, pAcd->pModeStatus->localPoint, pAcd->pModeStatus->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx acd mode dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }
                if ((dp > 0) && (dp < 200)) {
                    if (pAcd->pMode) {
                        pAcd->pMode->cloudPoint = dp;
                    }
                    if (pAcd->pModeStatus) {
                        pAcd->pModeStatus->cloudPoint = dp;
                    }
                } else {
                    if (pAcd->pMode) {
                        free (pAcd->pMode);
                        pAcd->pMode = NULL;
                    }
                    if (pAcd->pModeStatus) {
                        free (pAcd->pModeStatus);
                        pAcd->pModeStatus = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Temp")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pTemp = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pTemp) {
                            ret = isKnxGroupAddr (pTemp);
                            if (ret != 0) {
                                logErr ("knx acd temp ctrl group is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pAcd->pTemp = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pAcd->pTemp) {
                                logErr ("knx acd temp ctrl group obj calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pAcd->pTemp->addr, pTemp, MIN (strlen (pTemp), EIB_ADDR));
                            pAcd->pTemp->localPoint = ACD_TEMP;
                            pAcd->pTemp->type = KNX_DATA_TYPE_2BYTE;
                            logDbg ("knx acd temp ctrl: group:%s, localPoint:%d, type:%d",
                                    pAcd->pTemp->addr, pAcd->pTemp->localPoint, pAcd->pTemp->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Status")) {
                        char *pTempStatus = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pTempStatus) {
                            ret = isKnxGroupAddr (pTempStatus);
                            if (ret != 0) {
                                logErr ("knx acd temp status group is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pAcd->pTempStatus = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pAcd->pTempStatus) {
                                logErr ("knx acd temp status group obj calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pAcd->pTempStatus->addr, pTempStatus, MIN (strlen (pTempStatus), EIB_ADDR));
                            pAcd->pTempStatus->localPoint = ACD_TEMP_STATUS;
                            pAcd->pTempStatus->type = KNX_DATA_TYPE_2BYTE;
                            pAcd->pTempStatus->threshold = 500;
#if 0
                            kv_set(pAcd->pTempStatus->addr, NULL, 500);
                            clock_gettime (CLOCK_MONOTONIC, &pAcd->pTempStatus->timestamp);
#endif
                            logDbg ("knx acd temp status: group:%s, localPoint:%d, type:%d",
                                    pAcd->pTempStatus->addr, pAcd->pTempStatus->localPoint, pAcd->pTempStatus->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx acd temp dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }
                if ((dp > 0) && (dp < 200)) {
                    if (pAcd->pTemp) {
                        pAcd->pTemp->cloudPoint = dp;
                    }
                    if (pAcd->pTempStatus) {
                        pAcd->pTempStatus->cloudPoint = dp;
                    }
                } else {
                    if (pAcd->pTemp) {
                        free (pAcd->pTemp);
                        pAcd->pTemp = NULL;
                    }
                    if (pAcd->pTempStatus) {
                        free (pAcd->pTempStatus);
                        pAcd->pTempStatus = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "EnvTemp")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Temp")) {
                        char *pTemp = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pTemp) {
                            ret = isKnxGroupAddr (pTemp);
                            if (ret != 0) {
                                logErr ("knx acd envtemp group obj calloc fail! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }

                            pAcd->pEnvTemp = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pAcd->pEnvTemp) {
                                logErr ("knx acd envtemp group obj calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pAcd->pEnvTemp->addr, pTemp, MIN (strlen (pTemp), EIB_ADDR));
                            pAcd->pEnvTemp->localPoint = ACD_ENV_TEMP;
                            pAcd->pEnvTemp->type = KNX_DATA_TYPE_2BYTE;
                            pAcd->pEnvTemp->threshold = 1000;
                            clock_gettime (CLOCK_MONOTONIC, &pAcd->pEnvTemp->timestamp);
                            logDbg ("knx acd envtemp status: addr:%s, localPoint:%d, type:%d",
                                    pAcd->pEnvTemp->addr, pAcd->pEnvTemp->localPoint, pAcd->pEnvTemp->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx acd env temp dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }
                if ((dp > 0) && (dp < 200)) {
                    if (pAcd->pEnvTemp) {
                        pAcd->pEnvTemp->cloudPoint = dp;
                    }
                } else {
                    if (pAcd->pEnvTemp) {
                        free (pAcd->pEnvTemp);
                        pAcd->pEnvTemp = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Fault")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pErrorCode = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pErrorCode) {
                            ret = isKnxGroupAddr (pErrorCode);
                            if (ret != 0) {
                                logErr ("knx acd errorcode group obj calloc fail! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }

                            pAcd->pErrorCode = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pAcd->pErrorCode) {
                                logErr ("knx acd errorcode group obj calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pAcd->pErrorCode->addr, pErrorCode, MIN (strlen (pErrorCode), EIB_ADDR));

                            pAcd->pErrorCode->localPoint = FREATURE_FAULT;
                            pAcd->pErrorCode->type = KNX_DATA_TYPE_1BYTE;
                            logDbg ("knx acd errorcode group obj: addr:%s, localPoint:%d, type:%d", pAcd->pErrorCode->addr, pAcd->pErrorCode->localPoint, pAcd->pErrorCode->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx acd errorcode dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }
                if ((dp > 0) && (dp < 200)) {
                    if (pAcd->pErrorCode) {
                        pAcd->pErrorCode->cloudPoint = dp;
                    }
                } else {
                    if (pAcd->pErrorCode) {
                        free (pAcd->pErrorCode);
                        pAcd->pErrorCode = NULL;
                    }
                }
            }

            pNode = pNode->next;
        }

        if (pForeignKey && pAcd->base.element.bMaster) {
            snprintf ((char *) pAcd->base.element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, pAcd->base.element.foreignType, pForeignKey);
            logDbg ("knx acd foreign key is %s", pAcd->base.element.pForeignKey);
        }
        listAddNodeTail (pList, (void *) pAcd);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

int ParseKnxVentCfg (const char *pDocPath, list_t *pList) {
    xmlChar *pXpath = BAD_CAST ("/Device/KNX/Ventilation/Cell");

    if (!pDocPath || !pList) {
        logErr ("ParseKnxVentCfg invalid param!");
        return -1;
    }

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        xmlNodePtr pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        char *pForeignKey = NULL;
        KnxVent_t *pVent = (KnxVent_t *) calloc (1, sizeof (KnxVent_t));
        if (!pVent) {
            logErr ("KnxVent_t calloc fail!");
            continue;
        }
        pVent->base.element.szName = "Ventilation";
        pVent->base.element.primaryType = KNX_DEVICE_VENTILATION;
        pVent->base.element.bMaster = false;
        pVent->base.bFeedback = true;
        pVent->base.bOnlineCheck = true;
        pVent->base.element.attri = DEVICE_ATTRI_CONTROLLER;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pVent->base.element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pVent->base.element.szPlugin) {
                        logErr ("calloc knx vent plugin failed");
                        continue;
                    }
                    strcpy ((char *) pVent->base.element.szPlugin, szPlugin);
                    logDbg ("knx vent plugin:%s", pVent->base.element.szPlugin);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pVent->base.element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pVent->base.element.pProductId) {
                        logErr ("knx vent pProductId calloc fail!");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    strcpy ((char *) pVent->base.element.pProductId, pProduct);
                    pVent->base.element.bMaster = true;
                    pVent->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx vent product id:%s", pVent->base.element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PhyAddr")) {
                char *pPhyAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPhyAddr) {
                    strncpy (pVent->base.phyAddr, pPhyAddr, MIN (EIB_ADDR, strlen (pPhyAddr)));
                    logDbg ("knx vent phy addr:%s", pVent->base.phyAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                char *pItem = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pItem) {
                    pVent->base.element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pVent->base.element.pPrimaryKey) {
                        continue;
                    }
                    snprintf ((char *) pVent->base.element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_KNX, pItem);
                    pVent->base.element.devId = strtol (pVent->base.element.pPrimaryKey + 12, NULL, 10);
                    logDbg ("knx vent primary key=%s, devId=%d", pVent->base.element.pPrimaryKey, pVent->base.element.devId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pForeignKey) {
                    pVent->base.element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pVent->base.element.pForeignKey) {
                        logErr ("knx vent pForeignKey calloc fail!");
                        continue;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get vent foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pVent->base.element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pVent->base.element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pVent->base.element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pVent->base.element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    logErr ("knx vent foreign type is invalid!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pFB = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pFB && ((*pFB == 'N') || (*pFB == 'n'))) {
                    pVent->base.bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pOnline = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pOnline && ((*pOnline == 'N') || (*pOnline == 'n'))) {
                    pVent->base.bOnlineCheck = false;
                    logDbg ("knx vent online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAttr")) {
                char *pAttri = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pAttri && ((*pAttri == 'C') || (*pAttri == 'c'))) {
                    pVent->base.element.attri = DEVICE_ATTRI_CONTROLLER;
                    logDbg ("knx vent attribute is controller!");
                } else if (pAttri && ((*pAttri == 'A') || (*pAttri == 'a'))) {
                    pVent->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx vent attribute is actuator!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Switch")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pSwitch = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pSwitch) {
                            int ret = isKnxGroupAddr (pSwitch);
                            if (ret != 0) {
                                logErr ("knx vent switch group obj addr is invalid!  ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pVent->pSwitch = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pVent->pSwitch) {
                                logErr ("knx vent switch ctrl calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pVent->pSwitch->addr, pSwitch, MIN (strlen (pSwitch), EIB_ADDR));
                            pVent->pSwitch->localPoint = VENT_SWITCH;
                            pVent->pSwitch->type = KNX_DATA_TYPE_1BIT;

                            logDbg ("knx vent switch ctrl: group=%s, localPoint=%d, type=%d", pVent->pSwitch->addr, pVent->pSwitch->localPoint, pVent->pSwitch->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Status")) {
                        char *pStatus = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pStatus) {
                            int ret = isKnxGroupAddr (pStatus);
                            if (ret != 0) {
                                logErr ("knx vent switch status group is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pVent->pSwitchStatus = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pVent->pSwitchStatus) {
                                logErr ("knx vent switch status calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pVent->pSwitchStatus->addr, pStatus, MIN (strlen (pStatus), EIB_ADDR));
                            pVent->pSwitchStatus->localPoint = VENT_SWITCH_STATUS;
                            pVent->pSwitchStatus->type = KNX_DATA_TYPE_1BIT;
                            pVent->pSwitchStatus->threshold = 500;
                            clock_gettime (CLOCK_MONOTONIC, &pVent->pSwitchStatus->timestamp);
                            logDbg ("knx vent switch status: group=%s, localPoint=%d, type=%d", pVent->pSwitchStatus->addr, pVent->pSwitchStatus->localPoint, pVent->pSwitchStatus->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx vent Switch dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }

                if ((dp > 0) && (dp < 200)) {
                    if (pVent->pSwitch) {
                        pVent->pSwitch->cloudPoint = dp;
                    }
                    if (pVent->pSwitchStatus) {
                        pVent->pSwitchStatus->cloudPoint = dp;
                    }
                } else {
                    if (pVent->pSwitch) {
                        free (pVent->pSwitch);
                        pVent->pSwitch = NULL;
                    }
                    if (pVent->pSwitchStatus) {
                        free (pVent->pSwitchStatus);
                        pVent->pSwitchStatus = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Speed")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pSpeed = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pSpeed) {
                            int ret = isKnxGroupAddr (pSpeed);
                            if (ret != 0) {
                                logErr ("knx vent speed ctrl group is invalid ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }

                            pVent->pSpeed = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pVent->pSpeed) {
                                logErr ("knx vent speed ctrl group obj calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pVent->pSpeed->addr, pSpeed, MIN (strlen (pSpeed), EIB_ADDR));
                            pVent->pSpeed->localPoint = VENT_SPEED;
                            pVent->pSpeed->type = KNX_DATA_TYPE_1BYTE;

                            logDbg ("knx vent speed ctrl: group=%s, localPoint=%d, type=%d", pVent->pSpeed->addr, pVent->pSpeed->localPoint, pVent->pSpeed->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Status")) {
                        char *pStatus = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pStatus) {
                            int ret = isKnxGroupAddr (pStatus);
                            if (ret != 0) {
                                logErr ("knx vent speed status group is invalid ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pVent->pSpeedStatus = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pVent->pSpeedStatus) {
                                logErr ("knx vent speed status group obj calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pVent->pSpeedStatus->addr, pStatus, MIN (strlen (pStatus), EIB_ADDR));
                            pVent->pSpeedStatus->localPoint = VENT_SPEED_STATUS;
                            pVent->pSpeedStatus->type = KNX_DATA_TYPE_1BYTE;
                            pVent->pSpeedStatus->threshold = 500;
                            clock_gettime (CLOCK_MONOTONIC, &pVent->pSpeedStatus->timestamp);
                            logDbg ("knx vent speed status: group=%s, localPoint=%d, type=%d", pVent->pSpeedStatus->addr, pVent->pSpeedStatus->localPoint, pVent->pSpeedStatus->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx vent speed dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }

                if ((dp > 0) && (dp < 200)) {
                    if (pVent->pSpeed) {
                        pVent->pSpeed->cloudPoint = dp;
                    }
                    if (pVent->pSpeedStatus) {
                        pVent->pSpeedStatus->cloudPoint = dp;
                    }
                } else {
                    if (pVent->pSpeed) {
                        free (pVent->pSpeed);
                        pVent->pSpeed = NULL;
                    }
                    if (pVent->pSpeedStatus) {
                        free (pVent->pSpeedStatus);
                        pVent->pSpeedStatus = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "EnvTemp")) {
                xmlNodePtr pEnvTempNode = pNode->xmlChildrenNode;
                pVent->pEnvTemp = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                if (!pVent->pEnvTemp) {
                    logErr ("knx vent home temp group obj calloc fail!");
                    exit (EXIT_FAILURE); /* force exit */
                }
                while (pEnvTempNode) {
                    if (!xmlStrcasecmp (pEnvTempNode->name, (const xmlChar *) "Temp")) {
                        char *pTemp = (char *) XML_GET_CONTENT (pEnvTempNode->xmlChildrenNode);
                        if (!pTemp) {
                            logErr ("knx vent env temp group content is null");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        int ret = isKnxGroupAddr (pTemp);
                        if (ret != 0) {
                            logErr ("knx vent env temp group is invalid ret=%d", ret);
                            exit (EXIT_FAILURE); /* force exit */
                        }

                        strncpy (pVent->pEnvTemp->addr, pTemp, MIN (strlen (pTemp), EIB_ADDR));
                        pVent->pEnvTemp->localPoint = VENT_ENV_TEMP;
                        pVent->pEnvTemp->type = KNX_DATA_TYPE_1BYTE;
                        pVent->pEnvTemp->threshold = 1000;
                        clock_gettime (CLOCK_MONOTONIC, &pVent->pEnvTemp->timestamp);
                        logDbg ("knx vent home temp: group=%s, localPoint=%s", pVent->pEnvTemp->addr, GetPointFeature (pVent->pEnvTemp->localPoint));
                    } else if (!xmlStrcasecmp (pEnvTempNode->name, (const xmlChar *) "DataPoint")) {
                        char *pDp = (char *) XML_GET_CONTENT (pEnvTempNode->xmlChildrenNode);
                        if (!pDp) {
                            logErr ("knx vent env temp dataPoint is null");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        errno = 0;
                        pVent->pEnvTemp->cloudPoint = strtol (pDp, NULL, 10);
                        if (errno != 0) {
                            logErr ("knx vent env temp dataPoint(%d) is invalid!", pVent->pEnvTemp->cloudPoint);
                            exit (EXIT_FAILURE); /* force exit */
                        }
                    }
                    pEnvTempNode = pEnvTempNode->next;
                }

                if ((pVent->pEnvTemp->localPoint == FEATURE_UNKNOWN) || (pVent->pEnvTemp->cloudPoint > 200)) {
                    free (pVent->pEnvTemp);
                    pVent->pEnvTemp = NULL;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Fault")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                pVent->pErrorCode = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                if (!pVent->pErrorCode) {
                    logErr ("knx acd error-code group obj calloc fail!");
                    goto KNX_VENT_NEXT_NODE;
                }

                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pErrorCode = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pErrorCode) {
                            int ret = isKnxGroupAddr (pErrorCode);
                            if (ret != 0) {
                                logErr ("knx vent error-code group obj calloc fail! ret=%d", ret);
                                goto KNX_VENT_FAULT;
                            }

                            strncpy (pVent->pErrorCode->addr, pErrorCode, MIN (strlen (pErrorCode), EIB_ADDR));
                            pVent->pErrorCode->localPoint = FREATURE_FAULT;
                            pVent->pErrorCode->type = KNX_DATA_TYPE_1BYTE;
                            pVent->pErrorCode->threshold = 1000;
                            clock_gettime (CLOCK_MONOTONIC, &pVent->pErrorCode->timestamp);
                            logDbg ("knx vent error-code group obj: addr:%s, localPoint:%d, type:%d", pVent->pErrorCode->addr, pVent->pErrorCode->localPoint, pVent->pErrorCode->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            pVent->pErrorCode->cloudPoint = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx vent error-code dataPoint(%s) is invalid!", pDataPoint);
                                goto KNX_VENT_FAULT;
                            }
                        }
                    }
                KNX_VENT_FAULT:
                    pChild = pChild->next;
                }

                if ((pVent->pErrorCode->localPoint == FEATURE_UNKNOWN) || (pVent->pErrorCode->cloudPoint > 200)) {
                    free (pVent->pErrorCode);
                    pVent->pErrorCode = NULL;
                }
            }
        KNX_VENT_NEXT_NODE:
            pNode = pNode->next;
        }

        if (pForeignKey && pVent->base.element.bMaster) {
            snprintf ((char *) pVent->base.element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, pVent->base.element.foreignType, pForeignKey);
            logDbg ("knx vent foreignKey is %s", pVent->base.element.pForeignKey);
        }
        listAddNodeTail (pList, (void *) pVent);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

int ParseKnxHeatCfg (const char *pDocPath, list_t *pList) {
    xmlDocPtr pDoc;
    xmlChar *pXpath = BAD_CAST ("/Device/KNX/FloorHeating/Cell");
    xmlXPathObjectPtr pXpathObj = NULL;
    xmlNodeSetPtr pNodeset = NULL;
    xmlNodePtr pNode = NULL;
    int ret = -1;

    if (!pDocPath || !pList) {
        logErr ("ParseKnxHeatCfg invalid param!");
        return -1;
    }

    pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }

    pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        char *pForeignKey = NULL;
        KnxHeat_t *pHeat = (KnxHeat_t *) calloc (1, sizeof (KnxHeat_t));
        if (!pHeat) {
            logErr ("KnxHeat_t calloc fail!");
            continue;
        }
        pHeat->base.element.szName = "FloorHeating";
        pHeat->base.element.primaryType = KNX_DEVICE_FLOORHEATING;
        pHeat->base.element.bMaster = false;
        pHeat->base.bFeedback = true;
        pHeat->base.bOnlineCheck = true;
        pHeat->base.element.attri = DEVICE_ATTRI_CONTROLLER;

        while (pNode != NULL) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pHeat->base.element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pHeat->base.element.szPlugin) {
                        logErr ("calloc knx heat plugin failed");
                        continue;
                    }
                    strcpy ((char *) pHeat->base.element.szPlugin, szPlugin);
                    logDbg ("knx heat plugin:%s", pHeat->base.element.szPlugin);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pHeat->base.element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pHeat->base.element.pProductId) {
                        logErr ("knx heat pProductId calloc fail!");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    strcpy ((char *) pHeat->base.element.pProductId, pProduct);
                    pHeat->base.element.bMaster = true;
                    pHeat->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx heat product id:%s", pHeat->base.element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PhyAddr")) {
                char *pPhyAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pPhyAddr) {
                    strncpy (pHeat->base.phyAddr, pPhyAddr, MIN (EIB_ADDR, strlen (pPhyAddr)));
                    logDbg ("knx heat phy addr:%s", pHeat->base.phyAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                char *pItem = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pItem) {
                    pHeat->base.element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pHeat->base.element.pPrimaryKey) {
                        continue;
                    }
                    snprintf ((char *) pHeat->base.element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_KNX, pItem);
                    pHeat->base.element.devId = strtol (pHeat->base.element.pPrimaryKey + 12, NULL, 10);
                    logDbg ("knx heat primary key=%s, devId=%d", pHeat->base.element.pPrimaryKey, pHeat->base.element.devId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pForeignKey) {
                    pHeat->base.element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pHeat->base.element.pForeignKey) {
                        logErr ("calloc knx heat foreign key failed");
                        continue;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pHeat->base.element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pHeat->base.element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pHeat->base.element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pHeat->base.element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    logErr ("knx heat foreign type is invalid!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Feedback")) {
                char *pFB = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pFB && ((*pFB == 'N') || (*pFB == 'n'))) {
                    pHeat->base.bFeedback = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pOnline = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pOnline && ((*pOnline == 'N') || (*pOnline == 'n'))) {
                    pHeat->base.bOnlineCheck = false;
                    logDbg ("knx heat online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAttr")) {
                char *pAttri = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pAttri && ((*pAttri == 'C') || (*pAttri == 'c'))) {
                    pHeat->base.element.attri = DEVICE_ATTRI_CONTROLLER;
                    logDbg ("knx heat attribute is controller!");
                } else if (pAttri && ((*pAttri == 'A') || (*pAttri == 'a'))) {
                    pHeat->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx heat attribute is actuator!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Switch")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pSwitch = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pSwitch) {
                            ret = isKnxGroupAddr (pSwitch);
                            if (ret != 0) {
                                logErr ("knx heat group switch ctrl addr is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pHeat->pSwitch = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pHeat->pSwitch) {
                                logErr ("knx heat group calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pHeat->pSwitch->addr, pSwitch, MIN (strlen (pSwitch), EIB_ADDR));
                            pHeat->pSwitch->localPoint = HEAT_SWITCH;
                            pHeat->pSwitch->type = KNX_DATA_TYPE_1BIT;

                            logDbg ("knx heat ctrl: group=%s, localPoint=%d, type=%d", pHeat->pSwitch->addr, pHeat->pSwitch->localPoint, pHeat->pSwitch->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Status")) {
                        char *pStatus = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pStatus) {
                            ret = isKnxGroupAddr (pStatus);
                            if (ret != 0) {
                                logErr ("knx heat group obj addr is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pHeat->pSwitchStatus = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pHeat->pSwitchStatus) {
                                logErr ("knx heat group calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pHeat->pSwitchStatus->addr, pStatus, MIN (strlen (pStatus), EIB_ADDR));
                            pHeat->pSwitchStatus->localPoint = HEAT_SWITCH_STATUS;
                            pHeat->pSwitchStatus->type = KNX_DATA_TYPE_1BIT;
                            pHeat->pSwitchStatus->threshold = 500;
                            clock_gettime (CLOCK_MONOTONIC, &pHeat->pSwitchStatus->timestamp);
                            logDbg ("knx heat ctrl status: group=%s, localPoint=%d, type=%d", pHeat->pSwitchStatus->addr, pHeat->pSwitchStatus->localPoint, pHeat->pSwitchStatus->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx heat dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }

                if ((dp > 0) && (dp < 200)) {
                    if (pHeat->pSwitch) {
                        pHeat->pSwitch->cloudPoint = dp;
                    }
                    if (pHeat->pSwitchStatus) {
                        pHeat->pSwitchStatus->cloudPoint = dp;
                    }
                } else {
                    if (pHeat->pSwitch) {
                        free (pHeat->pSwitch);
                        pHeat->pSwitch = NULL;
                    }
                    if (pHeat->pSwitchStatus) {
                        free (pHeat->pSwitchStatus);
                        pHeat->pSwitchStatus = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Temp")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pTemp = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pTemp) {
                            ret = isKnxGroupAddr (pTemp);
                            if (ret != 0) {
                                logErr ("knx heat group temp ctrl addr is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pHeat->pTemp = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pHeat->pTemp) {
                                logErr ("knx heat group temp ctrl calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pHeat->pTemp->addr, pTemp, MIN (strlen (pTemp), EIB_ADDR));
                            pHeat->pTemp->localPoint = HEAT_TEMP;
                            pHeat->pTemp->type = KNX_DATA_TYPE_2BYTE;

                            logDbg ("knx heat temp ctrl: group=%s, localPoint=%d, type=%d", pHeat->pTemp->addr, pHeat->pTemp->localPoint, pHeat->pTemp->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Status")) {
                        char *pTempStatus = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pTempStatus) {
                            ret = isKnxGroupAddr (pTempStatus);
                            if (ret != 0) {
                                logErr ("knx heat group temp status addr is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            pHeat->pTempStatus = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pHeat->pTempStatus) {
                                logErr ("knx heat group temp status calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pHeat->pTempStatus->addr, pTempStatus, MIN (strlen (pTempStatus), EIB_ADDR));
                            pHeat->pTempStatus->localPoint = HEAT_TEMP_STATUS;
                            pHeat->pTempStatus->type = KNX_DATA_TYPE_2BYTE;
                            pHeat->pTempStatus->threshold = 500;
                            clock_gettime (CLOCK_MONOTONIC, &pHeat->pTempStatus->timestamp);
                            logDbg ("knx heat temp status: group=%s, localPoint=%d, type=%d", pHeat->pTempStatus->addr, pHeat->pTempStatus->localPoint, pHeat->pTempStatus->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx heat temp dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }
                if ((dp > 0) && (dp < 200)) {
                    if (pHeat->pTemp) {
                        pHeat->pTemp->cloudPoint = dp;
                    }
                    if (pHeat->pTempStatus) {
                        pHeat->pTempStatus->cloudPoint = dp;
                    }
                } else {
                    if (pHeat->pTemp) {
                        free (pHeat->pTemp);
                        pHeat->pTemp = NULL;
                    }
                    if (pHeat->pTempStatus) {
                        free (pHeat->pTempStatus);
                        pHeat->pTempStatus = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Valve")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Status")) {
                        char *pValve = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pValve) {
                            ret = isKnxGroupAddr (pValve);
                            if (ret != 0) {
                                logErr ("knx heat group valve obj addr is invalid! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }

                            pHeat->pValve = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pHeat->pValve) {
                                logErr ("[Knx->FloorHeating->Valve] calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pHeat->pValve->addr, pValve, MIN (strlen (pValve), EIB_ADDR));
                            pHeat->pValve->localPoint = HEAT_VALVE_STATUS;
                            pHeat->pValve->type = KNX_DATA_TYPE_1BIT;
                            logDbg ("[Knx->FloorHeating->Valve] addr:%s, localPoint:%d, type:%d",
                                    pHeat->pValve->addr, pHeat->pValve->localPoint, pHeat->pValve->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx heat valve dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }
                if ((dp > 0) && (dp < 200)) {
                    if (pHeat->pValve) {
                        pHeat->pValve->cloudPoint = dp;
                    }
                } else {
                    if (pHeat->pValve) {
                        free (pHeat->pValve);
                        pHeat->pValve = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "EnvTemp")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Temp")) {
                        char *pEvn = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pEvn) {
                            ret = isKnxGroupAddr (pEvn);
                            if (ret != 0) {
                                logErr ("[Knx->FloorHeating->EnvTemp] illegal group addr! ret=%d", ret);
                                exit (EXIT_FAILURE); /* force exit */
                            }

                            pHeat->pEnvTemp = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pHeat->pEnvTemp) {
                                logErr ("[Knx->FloorHeating->EnvTemp] calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pHeat->pEnvTemp->addr, pEvn, MIN (strlen (pEvn), EIB_ADDR));
                            pHeat->pEnvTemp->localPoint = HEAT_ENV_TEMP;
                            pHeat->pEnvTemp->type = KNX_DATA_TYPE_2BYTE;
                            pHeat->pEnvTemp->threshold = 1000;
                            clock_gettime (CLOCK_MONOTONIC, &pHeat->pEnvTemp->timestamp);
                            logDbg ("[Knx->FloorHeating->EnvTemp] addr:%s, localPoint:%d, type:%d",
                                    pHeat->pEnvTemp->addr, pHeat->pEnvTemp->localPoint, pHeat->pEnvTemp->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx heat env temp dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }
                if ((dp > 0) && (dp < 200)) {
                    if (pHeat->pEnvTemp) {
                        pHeat->pEnvTemp->cloudPoint = dp;
                    }
                } else {
                    if (pHeat->pEnvTemp) {
                        free (pHeat->pEnvTemp);
                        pHeat->pEnvTemp = NULL;
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Fault")) {
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                int dp = 0;
                while (pChild) {
                    if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Ctrl")) {
                        char *pErrorCode = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pErrorCode) {
                            if (isKnxGroupAddr (pErrorCode) != 0) {
                                logErr ("knx heat error-code group obj calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }

                            pHeat->pErrorCode = (KnxObj_t *) calloc (1, sizeof (KnxObj_t));
                            if (!pHeat->pErrorCode) {
                                logErr ("knx heat error-code group obj calloc fail!");
                                exit (EXIT_FAILURE); /* force exit */
                            }
                            strncpy (pHeat->pErrorCode->addr, pErrorCode, MIN (strlen (pErrorCode), EIB_ADDR));

                            pHeat->pErrorCode->localPoint = FREATURE_FAULT;
                            pHeat->pErrorCode->type = KNX_DATA_TYPE_1BYTE;
                            pHeat->pErrorCode->threshold = 1000;
                            clock_gettime (CLOCK_MONOTONIC, &pHeat->pErrorCode->timestamp);
                            logDbg ("knx heat error-code group obj: addr:%s, localPoint:%d, type:%d", pHeat->pErrorCode->addr, pHeat->pErrorCode->localPoint, pHeat->pErrorCode->type);
                        }
                    } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                        if (pDataPoint) {
                            errno = 0;
                            dp = strtol (pDataPoint, NULL, 10);
                            if (errno != 0) {
                                logErr ("knx heat errorcode dataPoint(%d) is invalid!", dp);
                                exit (EXIT_FAILURE); /* force exit */
                            }
                        }
                    }
                    pChild = pChild->next;
                }
                if ((dp > 0) && (dp < 200)) {
                    if (pHeat->pErrorCode) {
                        pHeat->pErrorCode->cloudPoint = dp;
                    }
                } else {
                    if (pHeat->pErrorCode) {
                        free (pHeat->pErrorCode);
                        pHeat->pErrorCode = NULL;
                    }
                }
            }
            pNode = pNode->next;
        }

        if (pForeignKey && pHeat->base.element.bMaster) {
            snprintf ((char *) pHeat->base.element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, pHeat->base.element.foreignType, pForeignKey);
            logDbg ("knx heat foreign Key is %s", pHeat->base.element.pForeignKey);
        }
        listAddNodeTail (pList, (void *) pHeat);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

/* KNX panel */
int ParseKnxPanelCfg (const char *pDocPath, list_t *pList) {
    xmlDocPtr pDoc;
    xmlChar *pXpath = BAD_CAST ("/Device/KNX/Panel/Cell");
    xmlXPathObjectPtr pXpathObj = NULL;
    xmlNodeSetPtr pNodeset = NULL;
    xmlNodePtr pNode = NULL;
    char *pPrimaryKey = NULL;
    char *pForeignKey = NULL;

    if (!pDocPath || !pList) {
        logErr ("ParseKnxPanelCfg invalid param!");
        return -1;
    }

    pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }

    pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        KnxPanel_t *pKey = (KnxPanel_t *) calloc (1, sizeof (KnxPanel_t));
        if (!pKey) {
            logErr ("KnxPanel_t calloc fail!");
            continue;
        }
        pKey->base.element.szName = "Panel";
        pKey->keySum = 0;
        pKey->base.element.bMaster = false;
        pKey->base.bOnlineCheck = true;
        pKey->base.element.attri = DEVICE_ATTRI_CONTROLLER;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (szPlugin) {
                    pKey->base.element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pKey->base.element.szPlugin) {
                        logErr ("calloc knx pannel plugin failed");
                        continue;
                    }
                    strcpy ((char *) pKey->base.element.szPlugin, szPlugin);
                    logDbg ("knx panel plugin:%s", pKey->base.element.szPlugin);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pKey->base.element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pKey->base.element.pProductId) {
                        logErr ("knx panel pProductId calloc fail!");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    strcpy ((char *) pKey->base.element.pProductId, pProduct);
                    pKey->base.element.bMaster = true;
                    pKey->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx panel product id:%s", pKey->base.element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PhyAddr")) {
                char *PhyAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (PhyAddr) {
                    strncpy (pKey->base.phyAddr, PhyAddr, MIN (EIB_ADDR, strlen (PhyAddr)));
                    logDbg ("knx panel PhyAddr:%s", pKey->base.phyAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                pPrimaryKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pPrimaryKey) {
                    logErr ("get knx panel primary key failed");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pForeignKey) {
                    logErr ("get knx panel foreign key failed");
                    continue;
                }
                pKey->base.element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pKey->base.element.pForeignKey) {
                    logErr ("calloc knx pannel foreign Key failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get pannel foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pKey->base.element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pKey->base.element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pKey->base.element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pKey->base.element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    logErr ("knx panel foreign type is invalid!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pOnline = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pOnline && ((*pOnline == 'N') || (*pOnline == 'n'))) {
                    pKey->base.bOnlineCheck = false;
                    logDbg ("knx panel online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAttr")) {
                char *pAttri = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pAttri && ((*pAttri == 'C') || (*pAttri == 'c'))) {
                    pKey->base.element.attri = DEVICE_ATTRI_CONTROLLER;
                    logDbg ("knx key attribute is controller!");
                } else if (pAttri && ((*pAttri == 'A') || (*pAttri == 'a'))) {
                    pKey->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx key attribute is actuator!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "channel")) {
                KnxPanel_t *pPanel = realloc (pKey, sizeof (KnxPanel_t) + (pKey->keySum + 1) * sizeof (KnxPoints_t));
                if (!pPanel) {
                    logErr ("realloc knx panel failed");
                    exit (1);
                }

                pKey = pPanel;
                xmlAttrPtr pAttr = pNode->properties;
                while (pAttr) {
                    if (!xmlStrcasecmp (pAttr->name, BAD_CAST "mode")) {
                        xmlChar *pMode = xmlGetProp (pNode, (xmlChar *) "mode");
                        if (pMode) {
                            if (!xmlStrcasecmp (pMode, BAD_CAST "rd")) {
                                pKey->keys[pKey->keySum].point.mode = POINT_ATTR_READ_ONLY;
                                logDbg ("knx pannel channel%d read only mode.", pKey->keySum + 1);
                            } else if (!xmlStrcasecmp (pMode, BAD_CAST "wr")) {
                                pKey->keys[pKey->keySum].point.mode = POINT_ATTR_WRITE_ONLY;
                                logDbg ("knx pannel channel%d write only mode.", pKey->keySum + 1);
                            } else {
                                pKey->keys[pKey->keySum].point.mode = POINT_ATTR_READ_WRITE;
                                logErr ("knx pannel channel%d mode illegal!", pKey->keySum + 1);
                            }
                            xmlFree (pMode);
                        }
                    }
                    pAttr = pAttr->next;
                }
                xmlNodePtr pChild = pNode->xmlChildrenNode;
                if (pChild) {
                    int dp = 0;
                    while (pChild) {
                        if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Switch")) {
                            char *pCtrl = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                            if (pCtrl) {
                                pKey->keys[pKey->keySum].pCtrl = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                                if (!pKey->keys[pKey->keySum].pCtrl) {
                                    logErr ("calloc pKey->keys[no].pCtrl error!");
                                    exit (EXIT_FAILURE); /* force exit */
                                }
                                strncpy (pKey->keys[pKey->keySum].pCtrl->addr, pCtrl, MIN (strlen (pCtrl), EIB_ADDR));
                                pKey->keys[pKey->keySum].point.feature = PANEL_SWITCH;
                                pKey->keys[pKey->keySum].point.type = POINT_TYPE_BOOL;
                                pKey->keys[pKey->keySum].point.oldValue.b1 = false;

                                logDbg ("[knx panel]key%d, ctrl addr:%s, feature:%s", pKey->keySum, pKey->keys[pKey->keySum].pCtrl->addr, GetPointFeature (pKey->keys[pKey->keySum].point.feature));
                            }
                        } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "Status")) {
                            char *pStatus = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                            if (pStatus) {
                                pKey->keys[pKey->keySum].pFeedback = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                                if (!pKey->keys[pKey->keySum].pFeedback) {
                                    logErr ("calloc pKey->keys[no].pFeedback error!");
                                    exit (EXIT_FAILURE); /* force exit */
                                }
                                strncpy (pKey->keys[pKey->keySum].pFeedback->addr, pStatus, MIN (strlen (pStatus), EIB_ADDR));
                                pKey->keys[pKey->keySum].point.feature = PANEL_SWITCH_STATUS;
                                pKey->keys[pKey->keySum].point.type = POINT_TYPE_BOOL;
                                pKey->keys[pKey->keySum].point.oldValue.b1 = false;
                                pKey->keys[pKey->keySum].point.threshold = 500;
                                clock_gettime (CLOCK_MONOTONIC, &pKey->keys[pKey->keySum].point.timestamp);

                                logDbg ("[knx panel]key%d, ctrl addr:%s, feature:%s", pKey->keySum, pKey->keys[pKey->keySum].pFeedback->addr, GetPointFeature (pKey->keys[pKey->keySum].point.feature));
                            }
                        } else if (!xmlStrcasecmp (pChild->name, (const xmlChar *) "DataPoint")) {
                            char *pDataPoint = (char *) XML_GET_CONTENT (pChild->xmlChildrenNode);
                            if (pDataPoint) {
                                errno = 0;
                                dp = strtol (pDataPoint, NULL, 10);
                                if (errno != 0) {
                                    logErr ("knx panel switch%d dp(%s) is invalid!", pKey->keySum, pDataPoint);
                                }
                                pKey->keys[pKey->keySum].point.id = dp;
                            }
                        }
                        pChild = pChild->next;
                    }
                    pKey->keySum++;
                }
            }
            pNode = pNode->next;
        }
        if (pKey) {
            pKey->base.element.primaryType = KNX_PANEL_ONE_KEY + pKey->keySum - 1;
            if (pPrimaryKey) {
                pKey->base.element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pKey->base.element.pPrimaryKey) {
                    continue;
                }
                snprintf ((char *) pKey->base.element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_KNX, pPrimaryKey);
                pKey->base.element.devId = strtol ((char *) pKey->base.element.pPrimaryKey + 12, NULL, 10);
                logDbg ("knx %d key panel primary key is %s", pKey->keySum, pKey->base.element.pPrimaryKey);
            }
            if (pForeignKey && pKey->base.element.bMaster) {
                snprintf ((char *) pKey->base.element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, pKey->base.element.foreignType, pForeignKey);
                logDbg ("knx %d key panel foreign key is %s", pKey->keySum, pKey->base.element.pForeignKey);
            }
            listAddNodeTail (pList, (void *) pKey);
        }
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

/* KNX smart panel */
int ParseKnxSmartPanelCfg (const char *pDocPath, list_t *pList) {
    xmlDocPtr pDoc;
    xmlChar *pXpath = BAD_CAST ("/Device/KNX/SmartPanel/Cell");
    xmlXPathObjectPtr pXpathObj = NULL;
    xmlNodeSetPtr pNodeset = NULL;
    xmlNodePtr pNode = NULL;
    char *pPrimaryKey = NULL;
    char *pForeignKey = NULL;

    if (!pDocPath || !pList) {
        logErr ("ParseKnxPanelCfg invalid param!");
        return -1;
    }

    pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }

    pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        KnxSmartPanel_t *pKey = (KnxSmartPanel_t *) calloc (1, sizeof (KnxSmartPanel_t));
        if (!pKey) {
            logErr ("KnxSmartPanel_t calloc fail!");
            continue;
        }
        pKey->base.element.szName = "Panel";
        pKey->keySum = 0;
        pKey->base.element.bMaster = false;
        pKey->base.bOnlineCheck = true;
        pKey->base.element.attri = DEVICE_ATTRI_CONTROLLER;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pKey->base.element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pKey->base.element.pProductId) {
                        logErr ("knx smart panel pProductId calloc fail!");
                        exit (EXIT_FAILURE); /* force exit */
                    }
                    strcpy ((char *) pKey->base.element.pProductId, pProduct);
                    pKey->base.element.bMaster = true;
                    pKey->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx smart panel product id:%s", pKey->base.element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PhyAddr")) {
                char *PhyAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (PhyAddr) {
                    strncpy (pKey->base.phyAddr, PhyAddr, MIN (EIB_ADDR, strlen (PhyAddr)));
                    logDbg ("knx smart panel PhyAddr:%s", pKey->base.phyAddr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                pPrimaryKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pPrimaryKey) {
                    logErr ("get knx smart panel primary key failed");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pForeignKey) {
                    logErr ("get knx smart panel foreign key failed");
                    continue;
                }
                pKey->base.element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pKey->base.element.pForeignKey) {
                    logErr ("calloc knx smart pannel foreign Key failed");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get smart pannel foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pKey->base.element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pKey->base.element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pKey->base.element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pKey->base.element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    logErr ("knx smart panel foreign type is invalid!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pOnline = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pOnline && ((*pOnline == 'N') || (*pOnline == 'n'))) {
                    pKey->base.bOnlineCheck = false;
                    logDbg ("knx smart panel online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DeviceAttr")) {
                char *pAttri = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pAttri && ((*pAttri == 'C') || (*pAttri == 'c'))) {
                    pKey->base.element.attri = DEVICE_ATTRI_CONTROLLER;
                    logDbg ("knx key attribute is controller!");
                } else if (pAttri && ((*pAttri == 'A') || (*pAttri == 'a'))) {
                    pKey->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx key attribute is actuator!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "channel")) {
                KnxSmartPanel_t *pPanel = realloc (pKey, sizeof (KnxSmartPanel_t) + (pKey->keySum + 1) * sizeof (KnxSmartPanelPoint_t));
                if (!pPanel) {
                    logErr ("realloc knx smart panel failed");
                    exit (1);
                }
                pKey = pPanel;

                xmlNodePtr pChlChild = pNode->xmlChildrenNode;
                while (pChlChild) {
                    if (!xmlStrcasecmp (pChlChild->name, (const xmlChar *) "switch")) {
                        xmlNodePtr pSwitchChild = pChlChild->xmlChildrenNode;
                        while (pSwitchChild) {
                            if (!xmlStrcasecmp (pSwitchChild->name, (const xmlChar *) "Ctrl")) {
                                char *pCtrl = (char *) XML_GET_CONTENT (pSwitchChild->xmlChildrenNode);
                                if (pCtrl) {
                                    pKey->chl[pKey->keySum].key.pCtrl = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                                    if (!pKey->chl[pKey->keySum].key.pCtrl) {
                                        logErr ("calloc pKey->chl[no].pCtrl error!");
                                    }
                                    strncpy (pKey->chl[pKey->keySum].key.pCtrl->addr, pCtrl, MIN (strlen (pCtrl), EIB_ADDR));
                                    pKey->chl[pKey->keySum].key.point.feature = PANEL_SWITCH;
                                    pKey->chl[pKey->keySum].key.point.type = POINT_TYPE_BOOL;
                                    pKey->chl[pKey->keySum].key.point.oldValue.b1 = false;
                                    logDbg ("[knx smart panel]key%d, ctrl addr:%s, feature:%s", pKey->keySum, pKey->chl[pKey->keySum].key.pCtrl->addr, GetPointFeature (pKey->chl[pKey->keySum].key.point.feature));
                                }
                            } else if (!xmlStrcasecmp (pSwitchChild->name, (const xmlChar *) "Status")) {
                                char *pStatus = (char *) XML_GET_CONTENT (pSwitchChild->xmlChildrenNode);
                                if (pStatus) {
                                    pKey->chl[pKey->keySum].key.pFeedback = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                                    if (!pKey->chl[pKey->keySum].key.pFeedback) {
                                        logErr ("calloc pKey->chl[no].key.pFeedback error!");
                                    }
                                    strncpy (pKey->chl[pKey->keySum].key.pFeedback->addr, pStatus, MIN (strlen (pStatus), EIB_ADDR));
                                    pKey->chl[pKey->keySum].key.point.feature = PANEL_SWITCH_STATUS;
                                    pKey->chl[pKey->keySum].key.point.type = POINT_TYPE_BOOL;
                                    pKey->chl[pKey->keySum].key.point.oldValue.b1 = false;
                                    pKey->chl[pKey->keySum].key.point.threshold = 500;
                                    clock_gettime (CLOCK_MONOTONIC, &pKey->chl[pKey->keySum].key.point.timestamp);
                                    logDbg ("[knx smart panel]key%d, ctrl addr:%s, feature:%s", pKey->keySum, pKey->chl[pKey->keySum].key.pFeedback->addr, GetPointFeature (pKey->chl[pKey->keySum].key.point.feature));
                                }
                            } else if (!xmlStrcasecmp (pSwitchChild->name, (const xmlChar *) "DataPoint")) {
                                char *pDataPoint = (char *) XML_GET_CONTENT (pSwitchChild->xmlChildrenNode);
                                if (pDataPoint) {
                                    errno = 0;
                                    pKey->chl[pKey->keySum].key.point.id = strtol (pDataPoint, NULL, 10);
                                    if (errno != 0) {
                                        logErr ("knx smart panel switch%d dp(%s) is invalid!", pKey->keySum, pDataPoint);
                                    }
                                }
                            }
                            pSwitchChild = pSwitchChild->next;
                        }
                    } else if (!xmlStrcasecmp (pChlChild->name, (const xmlChar *) "scene")) {
                        xmlNodePtr pSceneChild = pChlChild->xmlChildrenNode;
                        while (pSceneChild) {
                            if (!xmlStrcasecmp (pSceneChild->name, (const xmlChar *) "addr")) {
                                char *pAddr = (char *) XML_GET_CONTENT (pSceneChild->xmlChildrenNode);
                                if (pAddr) {
                                    pKey->chl[pKey->keySum].scene.pCtrl = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                                    if (!pKey->chl[pKey->keySum].scene.pCtrl) {
                                        logErr ("calloc pKey->chl[no].scene.pCtrl error!");
                                    }
                                    strncpy (pKey->chl[pKey->keySum].scene.pCtrl->addr, pAddr, MIN (strlen (pAddr), EIB_ADDR));
                                    pKey->chl[pKey->keySum].scene.point.feature = FEATURE_SCENE;
                                    pKey->chl[pKey->keySum].scene.point.type = POINT_TYPE_U8;
                                    pKey->chl[pKey->keySum].scene.point.oldValue.b1 = false;
                                    pKey->chl[pKey->keySum].scene.point.threshold = 500;
                                    clock_gettime (CLOCK_MONOTONIC, &pKey->chl[pKey->keySum].scene.point.timestamp);
                                    logDbg ("[knx smart panel]key%d, ctrl addr:%s, feature:%s", pKey->keySum, pKey->chl[pKey->keySum].scene.pCtrl->addr, GetPointFeature (pKey->chl[pKey->keySum].scene.point.feature));
                                }
                            } else if (!xmlStrcasecmp (pSceneChild->name, (const xmlChar *) "value")) {
                                char *pStatus = (char *) XML_GET_CONTENT (pSceneChild->xmlChildrenNode);
                                if (pStatus) {
                                    errno = 0;
                                    pKey->chl[pKey->keySum].scene.point.type = POINT_TYPE_U8;
                                    pKey->chl[pKey->keySum].scene.point.value.u8 = strtol (pStatus, NULL, 10);
                                    if (errno != 0) {
                                        logErr ("[knx smart panel]switch%d dp(%s) is invalid! errno=%d", pKey->keySum, pStatus, errno);
                                        exit (EXIT_FAILURE); /* force exit */
                                    }
                                }
                            } else if (!xmlStrcasecmp (pSceneChild->name, (const xmlChar *) "DataPoint")) {
                                char *pDataPoint = (char *) XML_GET_CONTENT (pSceneChild->xmlChildrenNode);
                                if (pDataPoint) {
                                    errno = 0;
                                    pKey->chl[pKey->keySum].scene.point.id = strtol (pDataPoint, NULL, 10);
                                    if (errno != 0) {
                                        logErr ("[knx smart panel]switch%d dp(%s) is invalid! errno=%d", pKey->keySum, pDataPoint, errno);
                                        exit (EXIT_FAILURE); /* force exit */
                                    }
                                }
                            }
                            pSceneChild = pSceneChild->next;
                        }
                    } else if (!xmlStrcasecmp (pChlChild->name, (const xmlChar *) "mode")) {
                        xmlNodePtr pModeChild = pChlChild->xmlChildrenNode;
                        while (pModeChild) {
                            if (!xmlStrcasecmp (pModeChild->name, (const xmlChar *) "select")) {
                                char *pSelect = (char *) XML_GET_CONTENT (pModeChild->xmlChildrenNode);
                                if (pSelect) {
                                    pKey->chl[pKey->keySum].mode.point.feature = FEATURE_MODE;
                                    pKey->chl[pKey->keySum].mode.point.type = POINT_TYPE_U8;
                                    if (!xmlStrcasecmp ((const xmlChar *) pSelect, (const xmlChar *) "switch")) {
                                        pKey->chl[pKey->keySum].mode.point.value.u8 = 0;
                                    } else if (!xmlStrcasecmp ((const xmlChar *) pSelect, (const xmlChar *) "scene")) {
                                        pKey->chl[pKey->keySum].mode.point.value.u8 = 1;
                                    } else {
                                        logErr ("knx smart panel switch%d mode(%s) is invalid!", pKey->keySum, pSelect);
                                        exit (EXIT_FAILURE); /* force exit */
                                    }
                                    logDbg ("[knx smart panel]key%d, %s, %s", pKey->keySum, (pKey->chl[pKey->keySum].mode.point.value.u8 ? "scene" : "switch"), GetPointFeature (pKey->chl[pKey->keySum].mode.point.feature));
                                }
                            } else if (!xmlStrcasecmp (pModeChild->name, (const xmlChar *) "DataPoint")) {
                                char *pDataPoint = (char *) XML_GET_CONTENT (pModeChild->xmlChildrenNode);
                                if (pDataPoint) {
                                    errno = 0;
                                    pKey->chl[pKey->keySum].mode.point.id = strtol (pDataPoint, NULL, 10);
                                    if (errno != 0) {
                                        logErr ("knx smart panel switch%d dp(%s) is invalid!", pKey->keySum, pDataPoint);
                                        exit (EXIT_FAILURE); /* force exit */
                                    }
                                    logDbg ("[knx smart panel]key%d, mode dp:%d", pKey->keySum, pKey->chl[pKey->keySum].mode.point.id);
                                }
                            }
                            pModeChild = pModeChild->next;
                        }
                    }
                    pChlChild = pChlChild->next;
                }
                pKey->keySum++;
            }
            pNode = pNode->next;
        }
        if (pKey) {
            pKey->base.element.primaryType = KNX_MISC_PANEL_ONE_KEY + pKey->keySum - 1;

            if (pPrimaryKey) {
                pKey->base.element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pKey->base.element.pPrimaryKey) {
                    continue;
                }
                snprintf ((char *) pKey->base.element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_KNX, pPrimaryKey);
                pKey->base.element.devId = strtol ((char *) pKey->base.element.pPrimaryKey + 12, NULL, 10);
                logDbg ("knx %d key smart panel primary key is %s, id=%d", pKey->keySum, pKey->base.element.pPrimaryKey, pKey->base.element.devId);
            }
            if (pForeignKey && pKey->base.element.bMaster) {
                snprintf ((char *) pKey->base.element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, pKey->base.element.foreignType, pForeignKey);
                logDbg ("knx %d key smart panel foreign key is %s", pKey->keySum, pKey->base.element.pForeignKey);
            }
            listAddNodeTail (pList, (void *) pKey);
        }
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

/*
 * chl: 1 ~ 8
 * */
int UpdateKnxSmartPanelMode (const char *pUrl, int priKey, int chl, const xmlChar *pValue) {
    xmlChar xpath[128] = {0};
    snprintf ((char *) xpath, sizeof (xpath), "/Device/KNX/SmartPanel/Cell[PrimaryKey=%d]/channel[%d]/mode/select", priKey, chl);

    logTrc ("UpdateKnxSmartPanelMode %s", xpath);

    xmlDocPtr pDoc = xmlReadFile (pUrl, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pUrl);
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, xpath);
    if (!pXpathObj) {
        xmlFreeDoc (pDoc);
        return -1;
    }
    XmlUpdateNode (pXpathObj, pValue);
    xmlXPathFreeObject (pXpathObj);
    xmlSaveFileEnc (pUrl, pDoc, "UTF-8");
    xmlFreeDoc (pDoc);

    return 0;
}

int UpdateKnxVentSpeedCtrlGrp (const char *pUrl, const xmlChar *pValueOld, const xmlChar *pValueNew) {
    xmlChar xpath[128] = {0};
    /*
    * [@xxx="val"] → 用于匹配属性。
    * [Child="val"] 或 [Child/text()="val"] → 用于匹配子节点内容。
     */
    snprintf ((char *) xpath, sizeof (xpath), "/Device/KNX/Ventilation/Cell/Speed[Ctrl=\"%s\"]/Ctrl", pValueOld);

    logTrc ("UpdateKnxSmartPanelMode %s", xpath);

    xmlDocPtr pDoc = xmlReadFile (pUrl, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pUrl);
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, xpath);
    if (!pXpathObj) {
        xmlFreeDoc (pDoc);
        return -1;
    }
    XmlUpdateNode (pXpathObj, pValueNew);
    xmlXPathFreeObject (pXpathObj);
    xmlSaveFileEnc (pUrl, pDoc, "UTF-8");
    xmlFreeDoc (pDoc);

    return 0;
}

int UpdateKnxDimmerSwitchCtrlGrp (const char *pUrl, int priKey, const xmlChar *pValue) {
    xmlChar xpath[128] = {0};
    /*
    * [@xxx="val"] → 用于匹配属性。
    * [Child="val"] 或 [Child/text()="val"] → 用于匹配子节点内容。
     */
    snprintf ((char *) xpath, sizeof (xpath), "/Device/KNX/Dimmer/Cell[PrimaryKey=%d]/channel/Switch/Ctrl", priKey);

    logTrc ("UpdateKnxSmartPanelMode %s", xpath);

    xmlDocPtr pDoc = xmlReadFile (pUrl, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pUrl);
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, xpath);
    if (!pXpathObj) {
        xmlFreeDoc (pDoc);
        return -1;
    }
    XmlUpdateNode (pXpathObj, pValue);
    xmlXPathFreeObject (pXpathObj);
    xmlSaveFileEnc (pUrl, pDoc, "UTF-8");
    xmlFreeDoc (pDoc);

    return 0;
}

int UpdateKnxDimmerSwitchStatusGrp (const char *pUrl, int priKey, const xmlChar *pValue) {
    xmlChar xpath[128] = {0};
    /*
    * [@xxx="val"] → 用于匹配属性。
    * [Child="val"] 或 [Child/text()="val"] → 用于匹配子节点内容。
     */
    snprintf ((char *) xpath, sizeof (xpath), "/Device/KNX/Dimmer/Cell[PrimaryKey=%d]/channel/Switch/Status", priKey);

    logTrc ("UpdateKnxSmartPanelMode %s", xpath);

    xmlDocPtr pDoc = xmlReadFile (pUrl, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pUrl);
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, xpath);
    if (!pXpathObj) {
        xmlFreeDoc (pDoc);
        return -1;
    }
    XmlUpdateNode (pXpathObj, pValue);
    xmlXPathFreeObject (pXpathObj);
    xmlSaveFileEnc (pUrl, pDoc, "UTF-8");
    xmlFreeDoc (pDoc);

    return 0;
}

int UpdateKnxDimmerBrightnessCtrlGrp (const char *pUrl, int priKey, const xmlChar *pValue) {
    xmlChar xpath[128] = {0};
    /*
    * [@xxx="val"] → 用于匹配属性。
    * [Child="val"] 或 [Child/text()="val"] → 用于匹配子节点内容。
     */
    snprintf ((char *) xpath, sizeof (xpath), "/Device/KNX/Dimmer/Cell[PrimaryKey=%d]/channel/Brightness/Ctrl", priKey);

    logTrc ("UpdateKnxSmartPanelMode %s", xpath);

    xmlDocPtr pDoc = xmlReadFile (pUrl, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pUrl);
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, xpath);
    if (!pXpathObj) {
        xmlFreeDoc (pDoc);
        return -1;
    }
    XmlUpdateNode (pXpathObj, pValue);
    xmlXPathFreeObject (pXpathObj);
    xmlSaveFileEnc (pUrl, pDoc, "UTF-8");
    xmlFreeDoc (pDoc);

    return 0;
}

int UpdateKnxDimmerBrightnessStatusGrp (const char *pUrl, int priKey, const xmlChar *pValue) {
    xmlChar xpath[128] = {0};
    /*
    * [@xxx="val"] → 用于匹配属性。
    * [Child="val"] 或 [Child/text()="val"] → 用于匹配子节点内容。
     */
    snprintf ((char *) xpath, sizeof (xpath), "/Device/KNX/Dimmer/Cell[PrimaryKey=%d]/channel/Brightness/Status", priKey);

    logTrc ("UpdateKnxSmartPanelMode %s", xpath);

    xmlDocPtr pDoc = xmlReadFile (pUrl, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pUrl);
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, xpath);
    if (!pXpathObj) {
        xmlFreeDoc (pDoc);
        return -1;
    }
    XmlUpdateNode (pXpathObj, pValue);
    xmlXPathFreeObject (pXpathObj);
    xmlSaveFileEnc (pUrl, pDoc, "UTF-8");
    xmlFreeDoc (pDoc);

    return 0;
}

int UpdateKnxDimmerGrp (const char *pUrl, int priKey, const xmlChar *pSCValue, const xmlChar *pSSValue, const xmlChar *pBCValue, const xmlChar *pBSValue) {
    UpdateKnxDimmerSwitchCtrlGrp (pUrl, priKey, pSCValue);
    UpdateKnxDimmerSwitchStatusGrp (pUrl, priKey, pSSValue);
    UpdateKnxDimmerBrightnessCtrlGrp (pUrl, priKey, pBCValue);
    UpdateKnxDimmerBrightnessStatusGrp (pUrl, priKey, pBSValue);

    return 0;
}

/* KNX dimmer */
int ParseKnxDimmerCfg (const char *pDocPath, list_t *pList) {
    xmlDocPtr pDoc;
    xmlChar *pXpath = BAD_CAST ("/Device/KNX/Dimmer/Cell");
    xmlXPathObjectPtr pXpathObj = NULL;
    xmlNodeSetPtr pNodeset = NULL;
    char *pPrimaryKey = NULL;
    char *pForeignKey = NULL;

    if (!pDocPath || !pList) {
        logErr ("ParseKnxPanelCfg invalid param!");
        return -1;
    }

    pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }

    pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        xmlNodePtr pCellChild = pNodeset->nodeTab[i]->xmlChildrenNode;
        KnxDimmer_t *pDimmer = (KnxDimmer_t *) calloc (1, sizeof (KnxDimmer_t));
        if (!pDimmer) {
            logErr ("KnxPanel_t calloc fail!");
            exit (1);
        }
        pDimmer->base.element.szName = "Dimmer";
        pDimmer->base.element.attri = DEVICE_ATTRI_CONTROLLER;
        pDimmer->keySum = 0;
        pDimmer->base.element.bMaster = false;
        pDimmer->base.bOnlineCheck = true;

        while (pCellChild) {
            if (!xmlStrcasecmp (pCellChild->name, (const xmlChar *) "SharedLibrary")) {
                char *szPlugin = (char *) XML_GET_CONTENT (pCellChild->xmlChildrenNode);
                if (szPlugin) {
                    pDimmer->base.element.szPlugin = (char *) calloc (1, strlen (szPlugin) + PREFIX_SIZE);
                    if (!pDimmer->base.element.szPlugin) {
                        logErr ("calloc knx pannel plugin failed");
                        continue;
                    }
                    strcpy ((char *) pDimmer->base.element.szPlugin, szPlugin);
                    logDbg ("knx panel plugin:%s", pDimmer->base.element.szPlugin);
                }
            } else if (!xmlStrcasecmp (pCellChild->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pCellChild->xmlChildrenNode);
                if (pProduct) {
                    pDimmer->base.element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pDimmer->base.element.pProductId) {
                        logErr ("calloc knx pannel product id failed");
                        exit (EXIT_FAILURE);
                    }
                    strcpy ((char *) pDimmer->base.element.pProductId, pProduct);
                    pDimmer->base.element.bMaster = true;
                    pDimmer->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("knx panel product id:%s", pDimmer->base.element.pProductId);
                }
            } else if (!xmlStrcasecmp (pCellChild->name, (const xmlChar *) "PhyAddr")) {
                char *PhyAddr = (char *) XML_GET_CONTENT (pCellChild->xmlChildrenNode);
                if (PhyAddr) {
                    strncpy (pDimmer->base.phyAddr, PhyAddr, MIN (EIB_ADDR, strlen (PhyAddr)));
                    logDbg ("knx panel PhyAddr:%s", pDimmer->base.phyAddr);
                }
            } else if (!xmlStrcasecmp (pCellChild->name, (const xmlChar *) "PrimaryKey")) {
                pPrimaryKey = (char *) XML_GET_CONTENT (pCellChild->xmlChildrenNode);
                if (!pPrimaryKey) {
                    logErr ("get knx panel primary key failed");
                    continue;
                }
            } else if (!xmlStrcasecmp (pCellChild->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pCellChild->xmlChildrenNode);
                if (!pForeignKey) {
                    logErr ("get knx panel foreign key failed");
                    continue;
                }
                pDimmer->base.element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pDimmer->base.element.pForeignKey) {
                    logErr ("calloc knx pannel foreign Key failed");
                    continue;
                }
            } else if (!xmlStrcasecmp (pCellChild->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pCellChild->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get pannel foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pDimmer->base.element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pDimmer->base.element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pDimmer->base.element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pDimmer->base.element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    logErr ("knx panel foreign type is invalid!");
                }
            } else if (!xmlStrcasecmp (pCellChild->name, (const xmlChar *) "OnlineCheck")) {
                char *pOnline = (char *) XML_GET_CONTENT (pCellChild->xmlChildrenNode);
                if (pOnline && ((*pOnline == 'N') || (*pOnline == 'n'))) {
                    pDimmer->base.bOnlineCheck = false;
                    logDbg ("knx panel online check is disabled!");
                }
            } else if (!xmlStrcasecmp (pCellChild->name, (const xmlChar *) "channel")) {
                //              sizeof (KnxDimmer_t)=80, sizeof (KnxDimmerPoint_t)=352
                KnxDimmer_t *pBright = realloc (pDimmer, sizeof (KnxDimmer_t) + (pDimmer->keySum + 1) * sizeof (KnxDimmerPoint_t));
                if (!pBright) {
                    logErr ("realloc knx dimmer failed");
                    exit (1);
                }
                pDimmer = pBright;

                xmlAttrPtr pAttr = pCellChild->properties;
                while (pAttr) {
                    if (!xmlStrcasecmp (pAttr->name, BAD_CAST "mode")) {
                        xmlChar *pMode = xmlGetProp (pCellChild, (xmlChar *) "mode");
                        if (pMode) {
                            if (!xmlStrcasecmp (pMode, BAD_CAST "rd")) {
                                pDimmer->chl[pDimmer->keySum].key.point.mode = POINT_ATTR_READ_ONLY;
                                pDimmer->chl[pDimmer->keySum].bright.point.mode = POINT_ATTR_READ_ONLY;
                                logDbg ("knx dimmer chl%d read only mode.", pDimmer->keySum + 1);
                            } else if (!xmlStrcasecmp (pMode, BAD_CAST "wr")) {
                                pDimmer->chl[pDimmer->keySum].key.point.mode = POINT_ATTR_WRITE_ONLY;
                                pDimmer->chl[pDimmer->keySum].bright.point.mode = POINT_ATTR_WRITE_ONLY;
                                logDbg ("knx dimmer chl%d write only mode.", pDimmer->keySum + 1);
                            } else {
                                pDimmer->chl[pDimmer->keySum].key.point.mode = POINT_ATTR_READ_WRITE;
                                pDimmer->chl[pDimmer->keySum].bright.point.mode = POINT_ATTR_READ_WRITE;
                                logErr ("knx dimmer chl%d read and write mode!", pDimmer->keySum + 1);
                            }
                            xmlFree (pMode);
                        }
                    }
                    pAttr = pAttr->next;
                }
                xmlNodePtr pChlChild = pCellChild->xmlChildrenNode;
                int dp = 0;
                while (pChlChild) {
                    dp = 0;
                    if (!xmlStrcasecmp (pChlChild->name, (const xmlChar *) "Switch")) {
                        xmlNodePtr pSwitchChild = pChlChild->xmlChildrenNode;
                        while (pSwitchChild) {
                            if (!xmlStrcasecmp (pSwitchChild->name, (const xmlChar *) "Ctrl")) {
                                char *pCtrl = (char *) XML_GET_CONTENT (pSwitchChild->xmlChildrenNode);
                                if (pCtrl) {
                                    pDimmer->chl[pDimmer->keySum].key.pCtrl = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                                    if (pDimmer->chl[pDimmer->keySum].key.pCtrl) {
                                        strncpy (pDimmer->chl[pDimmer->keySum].key.pCtrl->addr, pCtrl, MIN (strlen (pCtrl), EIB_ADDR));
                                        pDimmer->chl[pDimmer->keySum].key.point.feature = PANEL_SWITCH;
                                        pDimmer->chl[pDimmer->keySum].key.point.type = POINT_TYPE_BOOL;
                                        pDimmer->chl[pDimmer->keySum].key.point.oldValue.b1 = false;
                                        logDbg ("[knx dimmer]switch%d ctrl addr:%s, feature:%d",
                                                pDimmer->keySum, pDimmer->chl[pDimmer->keySum].key.pCtrl->addr,
                                                pDimmer->chl[pDimmer->keySum].key.point.feature);
                                    }
                                }
                            } else if (!xmlStrcasecmp (pSwitchChild->name, (const xmlChar *) "Status")) {
                                char *pFeedback = (char *) XML_GET_CONTENT (pSwitchChild->xmlChildrenNode);
                                if (pFeedback) {
                                    pDimmer->chl[pDimmer->keySum].key.pFeedback = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                                    if (pDimmer->chl[pDimmer->keySum].key.pFeedback) {
                                        strncpy (pDimmer->chl[pDimmer->keySum].key.pFeedback->addr, pFeedback, MIN (strlen (pFeedback), EIB_ADDR));
                                        pDimmer->chl[pDimmer->keySum].key.point.feature = PANEL_SWITCH_STATUS;
                                        pDimmer->chl[pDimmer->keySum].key.point.type = POINT_TYPE_BOOL;
                                        pDimmer->chl[pDimmer->keySum].key.point.oldValue.b1 = false;
                                        logDbg ("[knx dimmer]switch%d feedback addr:%s, feature:%d",
                                                pDimmer->keySum, pDimmer->chl[pDimmer->keySum].key.pFeedback->addr,
                                                pDimmer->chl[pDimmer->keySum].key.point.feature);
                                    }
                                }
                            } else if (!xmlStrcasecmp (pSwitchChild->name, (const xmlChar *) "DataPoint")) {
                                char *pDataPoint = (char *) XML_GET_CONTENT (pSwitchChild->xmlChildrenNode);
                                if (pDataPoint) {
                                    errno = 0;
                                    dp = strtol (pDataPoint, NULL, 10);
                                    if (errno != 0) {
                                        logErr ("[knx dimmer]switch%d dp(%s) is invalid!", pDimmer->keySum, pDataPoint);
                                        exit (EXIT_FAILURE); /* force exit */
                                    }
                                    pDimmer->chl[pDimmer->keySum].key.point.id = dp;
                                }
                            }
                            pSwitchChild = pSwitchChild->next;
                        }
                    } else if (!xmlStrcasecmp (pChlChild->name, (const xmlChar *) "Brightness")) {
                        xmlAttrPtr pRangAttr = pChlChild->properties;
                        while (pRangAttr) {
                            if (!xmlStrcasecmp (pRangAttr->name, BAD_CAST "max")) {
                                xmlChar *pMax = xmlGetProp (pChlChild, (xmlChar *) "max");
                                if (pMax) {
                                    errno = 0;
                                    int max = strtol ((char *) pMax, NULL, 10);
                                    if (errno != 0) {
                                        logErr ("[knx dimmer]switch%d dp(%s) is invalid!", pDimmer->keySum, pMax);
                                        exit (EXIT_FAILURE); /* force exit */
                                    }
                                    pDimmer->chl[pDimmer->keySum].bright.point.attr.attr.value.max.u16 = max;
                                    logDbg ("[knx dimmer]switch%d bright max=%d", pDimmer->keySum, max);
                                }
                                xmlFree (pMax);
                            } else if (!xmlStrcasecmp (pRangAttr->name, BAD_CAST "min")) {
                                xmlChar *pMin = xmlGetProp (pChlChild, (xmlChar *) "min");
                                if (pMin) {
                                    errno = 0;
                                    int min = strtol ((char *) pMin, NULL, 10);
                                    if (errno != 0) {
                                        logErr ("[knx dimmer]switch%d dp(%s) is invalid!", pDimmer->keySum, pMin);
                                    }
                                    pDimmer->chl[pDimmer->keySum].bright.point.attr.attr.value.min.u16 = min;
                                    logDbg ("[knx dimmer]switch%d bright min=%d", pDimmer->keySum, min);
                                }
                                xmlFree (pMin);
                            } else if (!xmlStrcasecmp (pRangAttr->name, BAD_CAST "step")) {
                                xmlChar *pStep = xmlGetProp (pChlChild, (xmlChar *) "step");
                                if (pStep) {
                                    errno = 0;
                                    int step = strtol ((char *) pStep, NULL, 10);
                                    if (errno != 0) {
                                        logErr ("[knx dimmer]switch%d dp(%s) is invalid!", pDimmer->keySum, pStep);
                                        exit (EXIT_FAILURE); /* force exit */
                                    }
                                    pDimmer->chl[pDimmer->keySum].bright.point.attr.attr.value.step.u16 = step;
                                    logDbg ("[knx dimmer]switch%d bright step=%d", pDimmer->keySum, step);
                                }
                                xmlFree (pStep);
                            } else if (!xmlStrcasecmp (pRangAttr->name, BAD_CAST "scaling")) {
                                xmlChar *pScaling = xmlGetProp (pChlChild, (xmlChar *) "scaling");
                                if (pScaling) {
                                    errno = 0;
                                    int scaling = strtol ((char *) pScaling, NULL, 10);
                                    if (errno != 0) {
                                        logErr ("[knx dimmer]switch%d dp(%s) is invalid!", pDimmer->keySum, pScaling);
                                        exit (EXIT_FAILURE); /* force exit */
                                    }
                                    pDimmer->chl[pDimmer->keySum].bright.point.attr.attr.value.scaling.u16 = scaling;
                                    logDbg ("[knx dimmer]switch%d bright scaling=%d", pDimmer->keySum, scaling);
                                }
                                xmlFree (pScaling);
                            }
                            pRangAttr = pRangAttr->next;
                        }
                        xmlNodePtr pBrightChild = pChlChild->xmlChildrenNode;
                        while (pBrightChild) {
                            if (!xmlStrcasecmp (pBrightChild->name, (const xmlChar *) "Ctrl")) {
                                char *pCtrl = (char *) XML_GET_CONTENT (pBrightChild->xmlChildrenNode);
                                if (pCtrl) {
                                    pDimmer->chl[pDimmer->keySum].bright.pCtrl = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                                    if (pDimmer->chl[pDimmer->keySum].bright.pCtrl) {
                                        strncpy (pDimmer->chl[pDimmer->keySum].bright.pCtrl->addr, pCtrl, MIN (strlen (pCtrl), EIB_ADDR));
                                        pDimmer->chl[pDimmer->keySum].bright.point.feature = DIMMING_BRIGHTNESS;
                                        pDimmer->chl[pDimmer->keySum].bright.point.type = POINT_TYPE_U8;
                                        pDimmer->chl[pDimmer->keySum].bright.point.oldValue.u8 = 0;
                                        logDbg ("[knx dimmer]bright%d ctrl addr:%s, feature:%d", pDimmer->keySum, pDimmer->chl[pDimmer->keySum].bright.pCtrl->addr, pDimmer->chl[pDimmer->keySum].bright.point.feature);
                                    }
                                }
                            } else if (!xmlStrcasecmp (pBrightChild->name, (const xmlChar *) "Status")) {
                                char *pFeedback = (char *) XML_GET_CONTENT (pBrightChild->xmlChildrenNode);
                                if (pFeedback) {
                                    pDimmer->chl[pDimmer->keySum].bright.pFeedback = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                                    if (pDimmer->chl[pDimmer->keySum].bright.pFeedback) {
                                        strncpy (pDimmer->chl[pDimmer->keySum].bright.pFeedback->addr, pFeedback, MIN (strlen (pFeedback), EIB_ADDR));
                                        pDimmer->chl[pDimmer->keySum].bright.point.feature = DIMMING_BRIGHTNESS_STATUS;
                                        pDimmer->chl[pDimmer->keySum].bright.point.type = POINT_TYPE_U8;
                                        pDimmer->chl[pDimmer->keySum].bright.point.oldValue.u8 = 0;
                                        logDbg ("[knx dimmer]bright%d feedback addr:%s, feature:%d", pDimmer->keySum, pDimmer->chl[pDimmer->keySum].bright.pFeedback->addr, pDimmer->chl[pDimmer->keySum].bright.point.feature);
                                    }
                                }
                            } else if (!xmlStrcasecmp (pBrightChild->name, (const xmlChar *) "DataPoint")) {
                                char *pDataPoint = (char *) XML_GET_CONTENT (pBrightChild->xmlChildrenNode);
                                if (pDataPoint) {
                                    errno = 0;
                                    dp = strtol (pDataPoint, NULL, 10);
                                    if (errno != 0) {
                                        logErr ("knx panel switch%d dp(%s) is invalid!", pDimmer->keySum, pDataPoint);
                                        exit (EXIT_FAILURE); /* force exit */
                                    }
                                    pDimmer->chl[pDimmer->keySum].bright.point.id = dp;
                                }
                            }
                            pBrightChild = pBrightChild->next;
                        }
                    }
                    pChlChild = pChlChild->next;
                }
                pDimmer->keySum++;
            }
            pCellChild = pCellChild->next;
        }

        pDimmer->base.element.primaryType = (DeviceType_u) (KNX_DIMMER_ONE_KEY + pDimmer->keySum - 1);
        if (pPrimaryKey) {
            pDimmer->base.element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
            if (!pDimmer->base.element.pPrimaryKey) {
                continue;
            }
            snprintf ((char *) pDimmer->base.element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_KNX, pPrimaryKey);
            pDimmer->base.element.devId = strtol ((char *) pDimmer->base.element.pPrimaryKey + 12, NULL, 10);
            logDbg ("[knx dimmer]%s primary key %s", GetDeviceType (pDimmer->base.element.primaryType), pDimmer->base.element.pPrimaryKey);
        }
        if (pForeignKey && pDimmer->base.element.bMaster) {
            snprintf ((char *) pDimmer->base.element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, pDimmer->base.element.foreignType, pForeignKey);
            logDbg ("[knx dimmer]%s foreign key %s", GetDeviceType (pDimmer->base.element.primaryType), pDimmer->base.element.pForeignKey);
        }
        listAddNodeTail (pList, (void *) pDimmer);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

/* KNX curtain */
int ParseKnxCurtainCfg (const char *pDocPath, list_t *pList) {
    xmlDocPtr pDoc;
    //    xmlChar *pXpath = BAD_CAST ("/Device/KNX/Curtain/Cell[@model='bool']");
    xmlChar *pXpath = BAD_CAST ("/Device/KNX/Curtain/Cell");
    xmlXPathObjectPtr pXpathObj = NULL;
    xmlNodeSetPtr pNodeset = NULL;
    xmlNodePtr pNode = NULL;

    if (!pDocPath || !pList) {
        logErr ("ParseKnxCurtainBoolModel invalid param!");
        return -1;
    }

    pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }

    pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        KnxCurtain_t *pCurtain = (KnxCurtain_t *) calloc (1, sizeof (KnxCurtain_t));
        if (!pCurtain) {
            logErr ("KnxCurtain_t calloc fail!");
            continue;
        }
        pCurtain->base.element.primaryType = KNX_DEVICE_CURTAIN;
        pCurtain->base.element.szName = "Curtain";
        pCurtain->base.element.bMaster = false;
        pCurtain->base.bOnlineCheck = true;
        pCurtain->base.element.attri = DEVICE_ATTRI_CONTROLLER;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pCurtain->base.element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    if (!pCurtain->base.element.pProductId) {
                        logErr ("calloc knx curtain product id failed");
                        exit (EXIT_FAILURE);
                    }
                    strcpy ((char *) pCurtain->base.element.pProductId, pProduct);
                    pCurtain->base.element.bMaster = true;
                    pCurtain->base.element.attri = DEVICE_ATTRI_ACTUATOR;
                    logDbg ("[knx curtain] product id:%s", pCurtain->base.element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                char *pPrimaryKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pPrimaryKey) {
                    logErr ("get knx curtain primary key failed");
                    continue;
                }
                pCurtain->base.element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pCurtain->base.element.pPrimaryKey) {
                    logErr ("calloc knx curtain primary Key failed");
                    continue;
                }
                //                pCurtain->base.element.bMaster = true; // 有主键未必是主设备
                snprintf ((char *) pCurtain->base.element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_KNX, pPrimaryKey);
                pCurtain->base.element.devId = strtol ((char *) pCurtain->base.element.pPrimaryKey + 12, NULL, 10);
                logDbg ("[knx curtain] primary key:%s", pCurtain->base.element.pPrimaryKey);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                char *pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pForeignKey) {
                    logErr ("get knx curtain foreign key failed");
                    continue;
                }
                pCurtain->base.element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pCurtain->base.element.pForeignKey) {
                    logErr ("calloc knx curtain foreign Key failed");
                    continue;
                }
                snprintf ((char *) pCurtain->base.element.pForeignKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_KNX, pForeignKey);
                logDbg ("[knx curtain] foreign key:%s", pCurtain->base.element.pForeignKey);
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get curtain foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                logDbg ("[knx curtain] foreign type:%s", pTmp);
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pCurtain->base.element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pCurtain->base.element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pCurtain->base.element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pCurtain->base.element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    logErr ("knx curtain foreign type is invalid!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "OnlineCheck")) {
                char *pOnline = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                logDbg ("knx curtain online check:%s", pOnline);
                if (pOnline && ((*pOnline == 'N') || (*pOnline == 'n'))) {
                    pCurtain->base.bOnlineCheck = false;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "control")) {
                xmlNodePtr pCtrlChild = pNode->xmlChildrenNode;
                if (!pCtrlChild) {
                    logErr ("knx curtain control node is null!");
                    exit (EXIT_FAILURE); /* force exit */
                }
                uint16_t id = 0;
                while (pCtrlChild) {
                    if (!xmlStrcasecmp (pCtrlChild->name, (const xmlChar *) "open")) {
                        char *pOpen = (char *) XML_GET_CONTENT (pCtrlChild->xmlChildrenNode);
                        if (!pOpen) {
                            logErr ("knx curtain open addr is null!");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        pCurtain->pOpen = (KnxPoints_t *) calloc (1, sizeof (KnxPoints_t));
                        if (!pCurtain->pOpen) {
                            logErr ("[knx curtain] pOpen calloc failed");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        pCurtain->pOpen->pGroup = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                        if (!pCurtain->pOpen->pGroup) {
                            logErr ("[knx curtain] open group address calloc failed");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        strncpy (pCurtain->pOpen->pGroup->addr, pOpen, MIN (strlen (pOpen), EIB_ADDR));
                        pCurtain->pOpen->point.feature = CURTAIN_OPEN;
                        pCurtain->pOpen->point.type = POINT_TYPE_I32;
                        pCurtain->pOpen->point.oldValue.b1 = false;
                        logDbg ("[knx curtain]open group:%s, feature:%s", pCurtain->pOpen->pGroup->addr, GetPointFeature (pCurtain->pOpen->point.feature));
                    } else if (!xmlStrcasecmp (pCtrlChild->name, (const xmlChar *) "close")) {
                        char *pClose = (char *) XML_GET_CONTENT (pCtrlChild->xmlChildrenNode);
                        if (!pClose) {
                            logErr ("knx curtain close addr is null!");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        pCurtain->pClose = (KnxPoints_t *) calloc (1, sizeof (KnxPoints_t));
                        if (!pCurtain->pClose) {
                            logErr ("[knx curtain] close calloc failed");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        pCurtain->pClose->pGroup = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                        if (!pCurtain->pClose->pGroup) {
                            logErr ("[knx curtain] close group address calloc failed");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        strncpy (pCurtain->pClose->pGroup->addr, pClose, MIN (strlen (pClose), EIB_ADDR));
                        pCurtain->pClose->point.feature = CURTAIN_CLOSE;
                        pCurtain->pClose->point.type = POINT_TYPE_I32;
                        pCurtain->pClose->point.oldValue.b1 = false;
                        logDbg ("[knx curtain]close group:%s, feature:%s", pCurtain->pClose->pGroup->addr, GetPointFeature (pCurtain->pClose->point.feature));
                    } else if (!xmlStrcasecmp (pCtrlChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pCtrlChild->xmlChildrenNode);
                        if (!pDataPoint) {
                            logErr ("[knx curtain] open-close dp is null!");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        errno = 0;
                        id = strtol (pDataPoint, NULL, 10);
                        if (errno != 0) {
                            logErr ("[knx curtain] open-close dp(%s) is invalid!", pDataPoint);
                            exit (EXIT_FAILURE); /* force exit */
                        }
                    }
                    pCtrlChild = pCtrlChild->next;
                }
                if (pCurtain->pOpen) pCurtain->pOpen->point.id = id;
                if (pCurtain->pClose) pCurtain->pClose->point.id = id;
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "pause")) {
                xmlNodePtr pPauseChild = pNode->xmlChildrenNode;
                if (!pPauseChild) {
                    logErr ("[knx curtain] pause node is null!");
                    exit (EXIT_FAILURE); /* force exit */
                }
                uint16_t id = 0;
                while (pPauseChild) {
                    if (!xmlStrcasecmp (pPauseChild->name, (const xmlChar *) "pause")) {
                        char *pStop = (char *) XML_GET_CONTENT (pPauseChild->xmlChildrenNode);
                        if (!pStop) {
                            logErr ("[knx curtain] open addr is null!");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        pCurtain->pStop = (KnxPoints_t *) calloc (1, sizeof (KnxPoints_t));
                        if (!pCurtain->pStop) {
                            logErr ("[knx curtain] pause calloc failed");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        pCurtain->pStop->pGroup = (KnxGroup_t *) calloc (1, sizeof (KnxGroup_t));
                        if (!pCurtain->pStop->pGroup) {
                            logErr ("[knx curtain] stop group address calloc failed");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        strncpy (pCurtain->pStop->pGroup->addr, pStop, MIN (strlen (pStop), EIB_ADDR));
                        pCurtain->pStop->point.feature = CURTAIN_STOP;
                        pCurtain->pStop->point.type = POINT_TYPE_I32;
                        pCurtain->pStop->point.oldValue.b1 = false;
                        logDbg ("[knx curtain]stop group:%s, feature:%s", pCurtain->pStop->pGroup->addr, GetPointFeature (pCurtain->pStop->point.feature));
                    } else if (!xmlStrcasecmp (pPauseChild->name, (const xmlChar *) "DataPoint")) {
                        char *pDataPoint = (char *) XML_GET_CONTENT (pPauseChild->xmlChildrenNode);
                        if (!pDataPoint) {
                            logErr ("[knx curtain ]open-close dp is null!");
                            exit (EXIT_FAILURE); /* force exit */
                        }
                        errno = 0;
                        id = strtol (pDataPoint, NULL, 10);
                        if (errno != 0) {
                            logErr ("[knx curtain] open-close dp(%s) is invalid!", pDataPoint);
                            exit (EXIT_FAILURE); /* force exit */
                        }
                    }
                    pPauseChild = pPauseChild->next;
                }
                if (pCurtain->pStop) pCurtain->pStop->point.id = id;
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Fault")) {

            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "reversal")) {

            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "position")) {
            }
            pNode = pNode->next;
        }

        listAddNodeTail (pList, (void *) pCurtain);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

/* KNX DI */
int ParseDiCfg (const char *pDocPath, list_t *pList) {
    xmlDocPtr pDoc;
    xmlChar *pXpath = BAD_CAST ("/Device/DI/Cell");
    xmlXPathObjectPtr pXpathObj = NULL;
    xmlNodeSetPtr pNodeset = NULL;
    xmlNodePtr pNode = NULL;
    char *pPrimaryKey = NULL;
    char *pForeignKey = NULL;

    if (!pDocPath || !pList) {
        logErr ("ParseKnxPanelCfg invalid param!");
        return -1;
    }

    pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }

    pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        DiDev_t *pDI = (DiDev_t *) calloc (1, sizeof (DiDev_t));
        if (!pDI) {
            logErr ("pDI calloc fail!");
            continue;
        }
        pDI->element.szName = "DI";
        pDI->element.bMaster = false;
        pDI->status = -1;
        pDI->retryNum = 3;
        pDI->retryCnt = 0;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "UUID")) {
                char *pUUID = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pUUID) {
                    pDI->element.szUUID = (char *) calloc (1, strlen (pUUID) + PREFIX_SIZE);
                    if (!pDI->element.szUUID) {
                        logErr ("calloc szUUID failed");
                        continue;
                    }
                    strcpy ((char *) pDI->element.szUUID, pUUID);
                    logDbg ("DI szUUID:%s", pDI->element.szUUID);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "AUTHKEY")) {
                char *pKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pKey) {
                    pDI->element.szAuthKey = (char *) calloc (1, strlen (pKey) + PREFIX_SIZE);
                    if (!pDI->element.szAuthKey) {
                        logErr ("calloc di szAuthKey failed");
                        continue;
                    }
                    strcpy ((char *) pDI->element.szAuthKey, pKey);
                    logDbg ("di auth szAuthKey:%s", pDI->element.szAuthKey);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pDI->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    strcpy ((char *) pDI->element.pProductId, pProduct);
                    pDI->element.bMaster = true;
                    logDbg ("di product id:%s", pDI->element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                pPrimaryKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pPrimaryKey) {
                    logErr ("get di primary key failed");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pForeignKey) {
                    logErr ("get di foreign key failed");
                    continue;
                }
                pDI->element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pDI->element.pForeignKey) {
                    logErr ("calloc di foreign Key failed");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get di foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pDI->element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pDI->element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pDI->element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pDI->element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    logErr ("di foreign type is invalid!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Trigger")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get di foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "low", KEYWORD_LOW_LEN) == 0) {
                    pDI->eLevel = DI_LEVEL_LOW;
                } else if (memcmp (pTmp, "high", KEYWORD_HIGH_LEN) == 0) {
                    pDI->eLevel = DI_LEVEL_HIGH;
                } else {
                    logErr ("di trigger level is invalid!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Channel")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pTmp) {
                    errno = 0;
                    pDI->channel = (uint8_t) strtol (pTmp, NULL, 10);
                    if (errno != 0) {
                        logErr ("di channel invalid");
                    }
                    if (pDI->channel == 1) {
                        snprintf (pDI->cmd, sizeof (pDI->cmd) - 1, "cat %s", DI1);
                    } else if (pDI->channel == 2) {
                        snprintf (pDI->cmd, sizeof (pDI->cmd) - 1, "cat %s", DI2);
                    } else if (pDI->channel == 3) {
                        snprintf (pDI->cmd, sizeof (pDI->cmd) - 1, "cat %s", DI3);
                    } else if (pDI->channel == 4) {
                        snprintf (pDI->cmd, sizeof (pDI->cmd) - 1, "cat %s", DI4);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DataPoint")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pTmp) {
                    errno = 0;
                    pDI->dataPoint = strtol (pTmp, NULL, 10);
                    if (errno != 0) {
                        logErr ("di dataPoint invalid");
                    }
                }
            }
            pNode = pNode->next;
        }

        if (pPrimaryKey) {
            pDI->element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
            if (!pDI->element.pPrimaryKey) {
                continue;
            }
            snprintf ((char *) pDI->element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_DIO, pPrimaryKey);
            logDbg ("di primary key is %s", pDI->element.pPrimaryKey);
        }
        if (pForeignKey && pDI->element.bMaster) {
            snprintf ((char *) pDI->element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, pDI->element.foreignType, pForeignKey);
            logDbg ("di foreign key is %s", pDI->element.pForeignKey);
        }
        listAddNodeTail (pList, (void *) pDI);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

/* KNX DO */
int ParseDoCfg (const char *pDocPath, list_t *pList) {
    xmlDocPtr pDoc;
    xmlChar *pXpath = BAD_CAST ("/Device/DO/Cell");
    xmlXPathObjectPtr pXpathObj = NULL;
    xmlNodeSetPtr pNodeset = NULL;
    xmlNodePtr pNode = NULL;
    char *pPrimaryKey = NULL;
    char *pForeignKey = NULL;

    if (!pDocPath || !pList) {
        logErr ("ParseKnxPanelCfg invalid param!");
        return -1;
    }

    pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }

    pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        DoDev_t *pDo = (DoDev_t *) calloc (1, sizeof (DoDev_t));
        if (!pDo) {
            logErr ("pDO calloc fail!");
            continue;
        }
        pDo->element.szName = "DO";
        pDo->element.bMaster = false;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "UUID")) {
                char *pUUID = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pUUID) {
                    pDo->element.szUUID = (char *) calloc (1, strlen (pUUID) + PREFIX_SIZE);
                    if (!pDo->element.szUUID) {
                        logErr ("calloc szUUID failed");
                        continue;
                    }
                    strcpy ((char *) pDo->element.szUUID, pUUID);
                    logDbg ("do szUUID:%s", pDo->element.szUUID);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "AUTHKEY")) {
                char *pKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pKey) {
                    pDo->element.szAuthKey = (char *) calloc (1, strlen (pKey) + PREFIX_SIZE);
                    if (!pDo->element.szAuthKey) {
                        logErr ("calloc do szAuthKey failed");
                        continue;
                    }
                    strcpy ((char *) pDo->element.szAuthKey, pKey);
                    logDbg ("do auth szAuthKey:%s", pDo->element.szAuthKey);
                }
            }
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ProductID")) {
                char *pProduct = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pProduct) {
                    pDo->element.pProductId = (char *) calloc (1, strlen (pProduct) + 1);
                    strcpy ((char *) pDo->element.pProductId, pProduct);
                    pDo->element.bMaster = true;
                    logDbg ("do product id:%s", pDo->element.pProductId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                pPrimaryKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pPrimaryKey) {
                    logErr ("get do primary key failed");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                pForeignKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pForeignKey) {
                    logErr ("get do foreign key failed");
                    continue;
                }
                pDo->element.pForeignKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                if (!pDo->element.pForeignKey) {
                    logErr ("calloc do foreign Key failed");
                    continue;
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignType")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (!pTmp) {
                    logErr ("get do foreign type failed");
                    exit (EXIT_FAILURE); /* force exit */
                }
                if (memcmp (pTmp, "ZIGBEE", KEYWORD_ZIGBEE_LEN) == 0) {
                    pDo->element.foreignType = DEVICE_TYPE_ZIGBEE;
                } else if (memcmp (pTmp, "MODBUS", KEYWORD_MODBUS_LEN) == 0) {
                    pDo->element.foreignType = DEVICE_TYPE_MODBUS;
                } else if (memcmp (pTmp, "KNX", KEYWORD_KNX_LEN) == 0) {
                    pDo->element.foreignType = DEVICE_TYPE_KNX;
                } else if (memcmp (pTmp, "RF433", KEYWORD_RF433_LEN) == 0) {
                    pDo->element.foreignType = DEVICE_TYPE_RF433;
                } else {
                    logErr ("do foreign type is invalid!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Output")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pTmp) {
                    if (memcmp (pTmp, "open", KEYWORD_OPEN_LEN) == 0) {
                        pDo->eLevel = DO_LEVEL_OPEN;
                    } else if (memcmp (pTmp, "close", KEYWORD_CLOSE_LEN) == 0) {
                        pDo->eLevel = DO_LEVEL_CLOSE;
                    } else {
                        logErr ("do output level is invalid!");
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "Channel")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pTmp) {
                    errno = 0;
                    pDo->channel = (uint8_t) strtol (pTmp, NULL, 10);
                    if (errno != 0) {
                        logErr ("do channel invalid");
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "DataPoint")) {
                char *pTmp = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pTmp) {
                    errno = 0;
                    pDo->dataPoint = (uint8_t) strtol (pTmp, NULL, 10);
                    if (errno != 0) {
                        logErr ("do dataPoint invalid");
                    }
                }
            }
            pNode = pNode->next;
        }

        if (pPrimaryKey) {
            pDo->element.pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
            if (!pDo->element.pPrimaryKey) {
                continue;
            }
            snprintf ((char *) pDo->element.pPrimaryKey, DEVICE_ID_LENGTH, "%s%x%s", g_Mac, DEVICE_TYPE_DIO, pPrimaryKey);
            logDbg ("do primary key is %s", pDo->element.pPrimaryKey);
        }
        if (pForeignKey && pDo->element.bMaster) {
            snprintf ((char *) pDo->element.pForeignKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, pDo->element.foreignType, pForeignKey);
            logDbg ("do foreign key is %s", pDo->element.pForeignKey);
        }
        listAddNodeTail (pList, (void *) pDo);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

int ParseKnxSceneCfg (const char *pDocPath, list_t *pList) {
    xmlChar *pXpath = BAD_CAST ("/Device/KNX/scene/Cell | /Device/KNX/Scene/Cell | /Device/KNX/Scene/cell | /Device/KNX/scene/cell");
    xmlNodePtr pNode = NULL;

    if (!pDocPath || !pList) {
        logErr ("ParseKnxSceneCfg invalid param!");
        return -1;
    }

    xmlDocPtr pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }

    xmlXPathObjectPtr pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        logErr ("[knx scene] XmlGetNode fail!");
        return -1;
    }

    xmlNodeSetPtr pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        pNode = pNode->xmlChildrenNode;
        KnxScene_t *pScene = (KnxScene_t *) calloc (1, sizeof (KnxScene_t));
        if (!pScene) {
            logErr ("KnxPanel_t calloc fail!");
            continue;
        }

        while (pNode != NULL) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "id")) {
                char *pId = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pId) {
                    errno = 0;
                    pScene->remap = strtol (pId, NULL, 10);
                    if (errno != 0) {
                        logErr ("[knx scene] remap invalid, %s", pId);
                    } else {
                        logDbg ("[knx scene] remap:%d", pScene->remap);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "addr")) {
                char *pAddr = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pAddr) {
                    strncpy (pScene->addr, pAddr, MIN (strlen (pAddr), EIB_ADDR));
                    logDbg ("[knx scene] addr:%s", pScene->addr);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "value")) {
                char *pValue = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pValue) {
                    errno = 0;
                    pScene->value = strtol (pValue, NULL, 10);
                    if (errno != 0) {
                        logErr ("[knx scene] value invalid, %s", pValue);
                    } else {
                        logDbg ("[knx scene] value:%d", pScene->value);
                    }
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "name")) {
                char *pName = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pName) {
                    strncpy (pScene->addrName, pName, MIN (strlen (pName), DEVICE_NAME_LENGTH));
                    logDbg ("[knx scene] name:%s", pScene->addrName);
                }
            }
            pNode = pNode->next;
        }
        listAddNodeTail (pList, (void *) pScene);
    }
    xmlXPathFreeObject (pXpathObj);

    return 0;
}

int ParseGroupCfg (const char *pDocPath, const char *pPath, list_t *pList) {
    xmlDocPtr pDoc;
    //    xmlChar *pXpath = BAD_CAST ("/Device/Group/AirCondition/Cell");
    xmlChar *pXpath = BAD_CAST (pPath);
    xmlXPathObjectPtr pXpathObj = NULL;
    xmlNodeSetPtr pNodeset = NULL;
    xmlNodePtr pNode = NULL;

    if (!pDocPath || !pList) {
        logErr ("ParseGroupCfg invalid param!");
        return -1;
    }

    pDoc = xmlReadFile (pDocPath, NULL, XML_PARSE_RECOVER);
    if (pDoc == NULL) {
        logErr ("xmlReadFile %s fail!", pDocPath);
        return -1;
    }

    pXpathObj = XmlGetNode (pDoc, pXpath);
    if (pXpathObj == NULL) {
        xmlFreeDoc (pDoc);
        return -1;
    }

    pNodeset = pXpathObj->nodesetval;
    for (int i = 0; i < pNodeset->nodeNr; i++) {
        pNode = pNodeset->nodeTab[i];
        Group_t *pGroup = (Group_t *) calloc (1, sizeof (Group_t));
        if (!pGroup) {
            logErr ("pGroup calloc fail!");
            continue;
        }

        int memberSum = 0;
        int memberIdx = 0;
        xmlAttrPtr pAttr = pNode->properties;
        while (pAttr) {
            if (!xmlStrcasecmp (pAttr->name, BAD_CAST "type")) {
                xmlChar *pType = xmlGetProp (pNode, (xmlChar *) "type");
                if (pType) {
                    if (!xmlStrcasecmp (pType, BAD_CAST "485")) {
                        pGroup->deviceType = DEVICE_TYPE_MODBUS;
                        logDbg ("group type DEVICE_TYPE_MODBUS");
                    } else if (!xmlStrcasecmp (pType, BAD_CAST "KNX")) {
                        pGroup->deviceType = DEVICE_TYPE_KNX;
                        logDbg ("group type DEVICE_TYPE_KNX");
                    } else {
                        logErr ("group type %s illegal!", pType);
                    }
                    xmlFree (pType);
                }
            } else if (!xmlStrcasecmp (pAttr->name, BAD_CAST "member_sum")) {
                xmlChar *pMemberSum = xmlGetProp (pNode, (xmlChar *) "member_sum");
                if (pMemberSum) {
                    memberSum = atoi ((char *) pMemberSum);
                    logDbg ("[group] member_sum:%d", memberSum);
                    xmlFree (pMemberSum);
                }
            }
            pAttr = pAttr->next;
        }

        pGroup->ppForeignKey = (char **) calloc (memberSum, sizeof (char *));
        if (!pGroup->ppForeignKey) {
            logErr ("[group] ppForeignKey calloc failed");
            exit (EXIT_FAILURE);
        }
        pGroup->ppForeignKey[memberSum - 1] = NULL;
        pNode = pNode->xmlChildrenNode;

        while (pNode) {
            if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "group_id")) {
                char *pId = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pId) {
                    pGroup->groupId = atoi (pId);
                    logDbg ("[group] group_id:%d", pGroup->groupId);
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "PrimaryKey")) {
                char *pKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pKey) {
                    pGroup->pPrimaryKey = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pGroup->pPrimaryKey) continue;
                    if (pGroup->deviceType == DEVICE_TYPE_MODBUS) {
                        snprintf (pGroup->pPrimaryKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, DEVICE_TYPE_MODBUS, pKey);
                    } else if (pGroup->deviceType == DEVICE_TYPE_KNX) {
                        snprintf (pGroup->pPrimaryKey, DEVICE_ID_LENGTH, "%s%d%s", g_Mac, DEVICE_TYPE_KNX, pKey);
                    } else {
                        logErr ("[group] device type(%d) illegal!", pGroup->deviceType);
                    }
                    logDbg ("[group] primary key:%s", pGroup->pPrimaryKey);
                } else {
                    logErr ("[group] primary key is null!");
                }
            } else if (!xmlStrcasecmp (pNode->name, (const xmlChar *) "ForeignKey")) {
                char *pKey = (char *) XML_GET_CONTENT (pNode->xmlChildrenNode);
                if (pKey) {
                    pGroup->ppForeignKey[memberIdx] = (char *) calloc (1, DEVICE_ID_LENGTH + PREFIX_SIZE);
                    if (!pGroup->ppForeignKey[memberIdx]) continue;
                    if (pGroup->deviceType == DEVICE_TYPE_MODBUS) {
                        snprintf (pGroup->ppForeignKey[memberIdx], DEVICE_ID_LENGTH, "%s%d%s", g_Mac, DEVICE_TYPE_MODBUS, pKey);
                    } else if (pGroup->deviceType == DEVICE_TYPE_KNX) {
                        snprintf (pGroup->ppForeignKey[memberIdx], DEVICE_ID_LENGTH, "%s%d%s", g_Mac, DEVICE_TYPE_KNX, pKey);
                    } else {
                        logErr ("[group] device type illegal!");
                    }
                    logDbg ("[group] foreign key:%s", pGroup->ppForeignKey[memberIdx]);
                    memberIdx++;
                } else {
                    logErr ("[group] foreign key is null!");
                }
            }

            pNode = pNode->next;
        }
        listAddNodeTail (pList, (void *) pGroup);
    }

    return 0;
}
