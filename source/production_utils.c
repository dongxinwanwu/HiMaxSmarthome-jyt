#include "production_utils.h"// Assuming this header declares generate_random_unicast_mac
#include "common.h"
#include <fcntl.h>// For open()
#include <stdint.h>
#include <stdlib.h>// For rand()
#include <time.h>  // For time()
#include <unistd.h>// For read(), close()
#include <string.h>

/**
* @brief 生成符合IEEE 802规范的随机单播MAC地址 (LAA)
 * @param mac_addr 存储结果的6字节数组指针
 * @return 0 成功, -1 失败
 */
int generate_random_unicast_mac (uint8_t *mac_addr) {
    if (mac_addr == NULL) return -1;

    // 1. 获取真随机数熵源 (Linux环境推荐 /dev/urandom)
    int fd = open ("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        // 如果没有urandom，回退到伪随机 (不推荐用于量产产品)
        // 必须确保 srand() 已经在外部被妥善初始化 (如 srand(time(NULL)))
        for (int i = 0; i < 6; i++) {
            mac_addr[i] = rand () & 0xFF;
        }
    } else {
        read (fd, mac_addr, 6);
        close (fd);
    }

    // 2. 强制设置位，确保是 "单播" 且 "本地管理" 地址
    // Byte 0 结构: [b7 b6 b5 b4 b3 b2 b1 b0]
    // b0 (LSB) = 0 -> Unicast (单播)
    // b1       = 1 -> Locally Administered (本地管理)

    mac_addr[0] &= 0xFE;// 清除 bit 0 (0b11111110) -> 确保单播
    mac_addr[0] |= 0x02;// 设置 bit 1 (0b00000010) -> 确保本地管理

    return 0;
}

// {"uuid":"hxkje2e5efe23f8aed46","key":"XazVlKkrAguOuOzKZHkGwmWtjj39gyXM"}
// {"uuid":"hxkjbc065f8a5fb123d0","key":"p77G073w43hfKARbp3qNMJjnciBkwS3q"}

char *GetUuid (const char *pJson) {
    if (!pJson) {
        logErr ("GetUuid: pJson is NULL!");
        return NULL;
    }
    char *pBuf = (char *) pJson;

    /* uuid:hxkjbc065f8a5fb123d0 key:p77G073w43hfKARbp3qNMJjnciBkwS3q */
    const char *pHead = "uuid:";
    const char *pTail = " ";
    int headLen = strlen (pHead);

    char *pUuid = strstr (pBuf, pHead);
    if (!pUuid || !(pUuid + headLen)) {
        logErr ("GetUuid: head(%s) not found!", pHead);
        return NULL;
    }

    char *pEnd = strstr (pUuid + headLen, pTail);
    if (!pEnd) {
        logErr ("GetUuid: uuid tail not found!");
        return NULL;
    }

    char *pRet = (char *) calloc (pEnd - pUuid - headLen + 1, sizeof (char));
    if (!pRet) {
        logErr ("GetUuid: calloc failed!");
        return NULL;
    }
    memcpy (pRet, pUuid + headLen, pEnd - pUuid - headLen);

    return pRet;
}

char *GetKey (const char *pJson) {
    if (!pJson) {
        logErr ("GetKey: pJson is NULL!");
        return NULL;
    }
    char *pBuf = (char *) pJson;

    const char *pHead = "key:";
    int headLen = strlen (pHead);

    char *pKey = strstr (pBuf, pHead);
    if (!pKey || !(pKey + headLen)) {
        logErr ("GetKey: %s not found!", pHead);
        return NULL;
    }

    char *pRet = (char *) calloc (strlen (pKey) - headLen + 1, sizeof (char));
    if (!pRet) {
        logErr ("GetKey: calloc failed!");
        return NULL;
    }
    memcpy (pRet, pKey + headLen, 32);

    return pRet;
}

void GetLicenses (uint8_t pMac[6], DeviceElement_t *GatewayElement) {
    char licenses[256] = {0};

    if (access ("/opt/smarthome/tmp/license", F_OK | R_OK) != 0) {
        logErr ("local license file not be exist!");
        char cmd[128] = {0};

        snprintf (cmd, sizeof (cmd) - 1, "/opt/smarthome/bin/license %02x:%02x:%02x:%02x:%02x:%02x", pMac[0], pMac[1], pMac[2], pMac[3], pMac[4], pMac[5]);
        FILE *fd = popen (cmd, "r");
        if (fd == NULL) {
            logErr ("popen license error!");
            exit (EXIT_FAILURE);
        }
        fgets (licenses, sizeof (licenses) - 1, fd);
        pclose (fd);
        if (licenses[0] == 0) {
            logErr ("get license failed!");
            exit (EXIT_FAILURE);
        } else {
            zlog_info (pLogCat, "get license: %s", licenses);
        }
        memset (cmd, 0, sizeof (cmd));
        snprintf (cmd, sizeof (cmd) - 1, "echo %s > /tmp/license", licenses);
        system (cmd);
        memset (cmd, 0, sizeof (cmd));
        snprintf (cmd, sizeof (cmd) - 1, "cp /tmp/license /opt/smarthome/tmp/");
        system (cmd);
        system ("rm /tmp/license");
        system ("sync");
    }

    memset (licenses, 0, sizeof (licenses));
    FILE *fd1 = popen ("cat /opt/smarthome/tmp/license", "r");
    fgets (licenses, sizeof (licenses) - 1, fd1);
    pclose (fd1);

    logDbg ("License: %s", licenses);
    GatewayElement->szUUID = GetUuid (licenses);
    GatewayElement->szAuthKey = GetKey (licenses);
    //    GatewayElement.pProductId = "1aechqkxk40bhxeo";
    // GatewayElement->pProductId = "qdhgdfnanotjumfv";

    logDbg("UUID:%s, KEY:%s", GatewayElement->szUUID, GatewayElement->szAuthKey);
    if (!GatewayElement->szUUID || !GatewayElement->szAuthKey) {
        system ("rm /opt/smarthome/tmp/license");
        logErr (COLOR_BOLD COLOR_RED "get license failed!" COLOR_CLEAR);
        exit (EXIT_FAILURE);
    }
}