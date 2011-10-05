#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <reboot/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>

#include <signal.h>
#include <sys/wait.h>

#include "common.h"
#include "cutils/properties.h"
#include "recovery_ui.h"

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include <libgen.h>
#include "mtdutils/mtdutils.h"

#include "safebootcommands.h"

int check_systemorig_mount() {
    int result = 0;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return 0;
    }
    const MountedVolume* mv = find_mounted_volume_by_mount_point("/systemorig");
    if (mv == NULL) {
        __system("mount /dev/block/systemorig /systemorig");
        result = scan_mounted_volumes();
        if (result < 0) {
            LOGE("failed to scan mounted volumes\n");
            return 0;
        }
        const MountedVolume* mv = find_mounted_volume_by_mount_point("/systemorig");
        if (mv == NULL) {
            LOGE ("Can't mount primary system!\n");
            return 0;
        }
    }
    return 1;
}

int get_safe_mode() {
    int result =0;
    if (check_systemorig_mount()) {
       struct stat info;
       if (0 == stat(SAFE_SYSTEM_FILE, &info))
           result = 1;
    }
    return result;
}

void show_safe_boot_menu() {
    int safe_mode = get_safe_mode();
    char boot_status[256];
    strcpy(boot_status, "Currently: ");
    strcat(boot_status, safe_mode ? "ENABLED" : "DISABLED");
    char* headers[] = {  "Safe System Menu",
                                "",
                                "",
                                NULL
    };
    headers[1] = strdup(boot_status);
    static char* list[] = { "Toggle Safe System",
                            NULL
    };

    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
            {
                static char* confirm_install  = "Confirm Toggle?";
                static char confirm[PATH_MAX];
                sprintf(confirm, "Yes - %s Safe System", !safe_mode ? "Enable" : "Disable");
                if (confirm_selection(confirm_install, confirm)) toggle_safe_mode();
                break;
            }
        }
    }
}

void toggle_safe_mode() {
    int safe_mode = get_safe_mode();
    if (!safe_mode) {
        char cmd[256];
        strcpy(cmd, "touch ");
        strcat(cmd, SAFE_SYSTEM_FILE);
        __system(cmd);
    } else {
        char cmd[256];
        strcpy(cmd, "rm ");
        strcat(cmd, SAFE_SYSTEM_FILE);
        __system(cmd);
    }
    safe_mode = get_safe_mode();
    ui_print("Safe System is now: %s!\n", safe_mode ? "ENABLED" : "DISABLED");
}

