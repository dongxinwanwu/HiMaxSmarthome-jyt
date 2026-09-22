//
// Created by 王建 on 2025/9/10.
//

#include "upgrade.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 判断当前运行的 slot
const char *get_active_slot () {
    static char slot[2] = "A";
    char buf[256] = {0};
    ssize_t len = readlink ("/opt/smarthome/bin", buf, sizeof (buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        if (strstr (buf, "bin_A")) {
            strcpy (slot, "A");
        } else if (strstr (buf, "bin_B")) {
            strcpy (slot, "B");
        }
    }
    return slot;
}

// 回退到上一个版本
void rollback_to_previous () {
    char buf[256] = {0};
    ssize_t len = readlink ("/opt/smarthome/bin", buf, sizeof (buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        if (strstr (buf, "bin_A")) {
            system ("ln -sfn /opt/smarthome/bin_B /opt/smarthome/bin");
            system ("ln -sfn /opt/smarthome/plugin_B /opt/smarthome/plugin");
        } else {
            system ("ln -sfn /opt/smarthome/bin_A /opt/smarthome/bin");
            system ("ln -sfn /opt/smarthome/plugin_A /opt/smarthome/plugin");
        }
    }
    zlog_warn (pLogCat, "Rollback executed, switched to previous slot");
}

// 自检函数：可根据实际需求扩展 (心跳检测/配置加载/关键线程等)
bool self_check_ok() {
    // 1. 检查主程序是否存在且可执行
    if (access("/opt/smarthome/bin/smarthome", X_OK) != 0) {
        zlog_error(pLogCat, "self_check: smarthome binary missing or not executable");
        return false;
    }

    // 2. 执行自检命令 (假设 smarthome -t 能做基本自检)
    int ret = system("/opt/smarthome/bin/smarthome -t");
    if (ret != 0) {
        zlog_error(pLogCat, "self_check: smarthome self test failed, ret=%d", ret);
        return false;
    }

    zlog_info(pLogCat, "self_check: passed");
    return true;
}


static void _gw_upgrade_cb (const char *img) {
    zlog_info (pLogCat, COLOR_RED "Upgrade ready! img: %s" COLOR_CLEAR, img);

    char targetBin[128], targetPlugin[128], cmd[512];
    const char *active = get_active_slot ();// 需要你实现: 返回 "A" 或 "B"

    // 1. 确定目标目录
    if (strcmp (active, "A") == 0) {
        strcpy (targetBin, "/opt/smarthome/bin_B");
        strcpy (targetPlugin, "/opt/smarthome/plugin_B");
    } else {
        strcpy (targetBin, "/opt/smarthome/bin_A");
        strcpy (targetPlugin, "/opt/smarthome/plugin_A");
    }

    // 2. 清理目标目录
    snprintf (cmd, sizeof (cmd), "rm -rf %s %s && mkdir -p %s %s", targetBin, targetPlugin, targetBin, targetPlugin);
    system (cmd);

    // 3. 解压升级包到临时目录，再拷贝到目标
    snprintf (cmd, sizeof (cmd), "tar -xf %s -C /opt/smarthome/upgrade", img);
    if (system (cmd) != 0) {
        logErr ( "upgrade failed: cannot extract image");
        return;
    }

    snprintf (cmd, sizeof (cmd), "cp -rf /opt/smarthome/upgrade/bin/* %s/", targetBin);
    system (cmd);
    snprintf (cmd, sizeof (cmd), "cp -rf /opt/smarthome/upgrade/plugin/* %s/", targetPlugin);
    system (cmd);

    // 4. 校验新二进制
    snprintf (cmd, sizeof (cmd), "%s/smarthome -v", targetBin);
    if (system (cmd) != 0) {
        logErr ( "upgrade failed: new binary invalid");
        return;
    }

    // 5. 切换软链 (原子操作)
    system ("ln -sfn /opt/smarthome/bin_A /opt/smarthome/bin");// 确保存在
    system ("ln -sfn /opt/smarthome/plugin_A /opt/smarthome/plugin");

    snprintf (cmd, sizeof (cmd), "ln -sfn %s /opt/smarthome/bin", targetBin);
    system (cmd);
    snprintf (cmd, sizeof (cmd), "ln -sfn %s /opt/smarthome/plugin", targetPlugin);
    system (cmd);

    // 6. 写入版本号
    // write_file ("/opt/smarthome/version", get_img_version (img));// 需要你实现

    // 7. 标记回退点
    system ("touch /opt/smarthome/rollback.flag");
    zlog_info (pLogCat, COLOR_RED COLOR_BOLD "upgrade completed, restart..." COLOR_CLEAR);

    // 8. 启动新版本
    execl ("/opt/smarthome/bin/smarthome", "smarthome", "-e", NULL);

    logErr ( "[upgrade] new version execl failed");
    exit (0);
}

// 建议加到 main() 初始化阶段
void check_rollback () {
    if (access ("/opt/smarthome/rollback.flag", F_OK) == 0) {
        if (!self_check_ok ()) {    // 需要你实现: 心跳检测/进程自检
            rollback_to_previous ();// 切回旧目录软链
            system ("rm -f /opt/smarthome/rollback.flag");
            system ("reboot");
        } else {
            unlink ("/opt/smarthome/rollback.flag");// 新版本稳定，清理标记
        }
    }
}
