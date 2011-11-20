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
#include <sys/vfs.h>

#include <signal.h>
#include <sys/wait.h>

#include "common.h"
#include "cutils/properties.h"
#include "recovery_ui.h"

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"
#include "roots.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include <libgen.h>
#include "mtdutils/mtdutils.h"

#include "safebootcommands.h"

int safemode = 0;

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
       struct statfs info;
       if (0 == statfs(SAFE_SYSTEM_FILE, &info))
           result = 1;
    }
    return result;
}

void show_safe_boot_menu() {
    char tmp[PATH_MAX];
    char* headers[] = {  "Safe System Menu",
                                "",
                                "",
                                NULL
    };
    static char* list[] = { "Toggle Safe System",
                            "Quick Toggle Safe System (Dangerous)",
                            NULL
    };

    for (;;)
    {
        sprintf(tmp, "Currently: %s", safemode ? "ENABLED" : "DISABLED");
        headers[1] = strdup(tmp);
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
            {
                static char* confirm_install  = "Confirm Toggle?";
                static char confirm[PATH_MAX];
                sprintf(confirm, "Yes - %s Safe System", !safemode ? "Enable" : "Disable");
                if (confirm_selection(confirm_install, confirm)) toggle_safe_mode();
                break;
            }
            case 1:
            {
                static char* confirm_install  = "Confirm Quick Toggle? (WARNING: DANGEROUS!)";
                static char confirm[PATH_MAX];
                sprintf(confirm, "Yes - %s Safe System (DANGEROUS! NO BACKUP/RESTORE)", !safemode ? "Enable" : "Disable");
                if (confirm_selection(confirm_install, confirm)) quick_toggle_safe_mode();
                break;
            }
        }
    }
}

void quick_toggle_safe_mode() {
    char cmd[256];
    safemode = get_safe_mode();
    if (!safemode) {
        /* 4. touch SAFE_SYSTEM_FILE */
        sprintf(cmd, "touch %s", SAFE_SYSTEM_FILE);
        ui_print("\n%s\n", cmd);
        __system(cmd);
    } else {
        /* 4. rm SAFE_SYSTEM_FILE */
        sprintf(cmd, "rm %s", SAFE_SYSTEM_FILE);
        ui_print("\n%s\n", cmd);
        __system(cmd);
    }
    safemode = get_safe_mode();
    ui_print("Safe System is now: %s!\n", safemode ? "ENABLED" : "DISABLED");
}

void toggle_safe_mode() {
    struct statfs info;
    char cmd[256];

    safemode = get_safe_mode();
    char orig_backup_path[PATH_MAX];
    sprintf(orig_backup_path, "/emmc/%s/orig", EXPAND(RECOVERY_FOLDER));
    char safe_backup_path[PATH_MAX];
    sprintf(safe_backup_path, "/emmc/%s/safe", EXPAND(RECOVERY_FOLDER));

    if (ensure_path_mounted("/emmc") != 0) {
        ui_print("Can't mount /emmc\n");
        return;
    }
   
    int ret;
    if (0 != (ret = statfs("/emmc", &info))) {
        ui_print("Unable to stat /emmc\n");
        return;
    }
    uint64_t bavail = info.f_bavail;
    uint64_t bsize = info.f_bsize;
    uint64_t sdcard_free = bavail * bsize;
    uint64_t sdcard_free_mb = sdcard_free / (uint64_t)(1024 * 1024);
    ui_print("Emmc space free: %lluMB\n", sdcard_free_mb);
    if (sdcard_free_mb < 1024)
        ui_print("There may not be enough free space to complete backup... continuing...\n");

    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
    if (!safemode) {
   
        sprintf(cmd, "mkdir -p %s", orig_backup_path);
        __system(cmd);

        /* 1. make a backup of the existing /data + /cache in /emmc/safestrap/orig/ */
        ui_print("\n-- Backing up Original Data...\n");

        sprintf(cmd, "rm %s/*", orig_backup_path);
        __system(cmd);
        // if (0 != (ret = nandroid_backup_partition(orig_backup_path, "/system"))) return;
        // ui_set_progress(0.15);
        if (0 != (ret = nandroid_backup_partition(orig_backup_path, "/data"))) return;
        // ui_set_progress(0.40);
        // if (0 != (ret = nandroid_backup_partition(orig_backup_path, "/cache"))) return;
        // ui_set_progress(0.45);

        ui_print("\n-- Restoring Safe System Data...\n");

        // if (0 != (ret = nandroid_restore_partition(safe_backup_path, "/system"))) return;
        // ui_set_progress(0.60);
        if (0 != (ret = nandroid_restore_partition(safe_backup_path, "/data"))) return;
        // ui_set_progress(0.80);
        // if (0 != (ret = nandroid_restore_partition(safe_backup_path, "/cache"))) return;
        // ui_set_progress(0.90);

        /* 3. wipe Dalvik Cache */
        __system("rm -r /data/dalvik-cache");
        __system("rm -r /cache/dalvik-cache");
        __system("rm -r /sd-ext/dalvik-cache");
        // ui_set_progress(0.95);

        /* 4. touch SAFE_SYSTEM_FILE */
        sprintf(cmd, "touch %s", SAFE_SYSTEM_FILE);
        ui_print("\n%s\n", cmd);
        __system(cmd);

        // ui_set_progress(1);
        ui_print("Swap to Safe System Complete.\n");

    } else {

        sprintf(cmd, "mkdir -p %s", safe_backup_path);
        __system(cmd);

        /* 1. make a backup of the existing /data + /cache in /emmc/safestrap/safe/ */
        ui_print("\n-- Backing up Safe System...\n");

        sprintf(cmd, "rm %s/*", safe_backup_path);
        __system(cmd);
        // if (0 != (ret = nandroid_backup_partition(safe_backup_path, "/system"))) return;
        // ui_set_progress(0.15);
        if (0 != (ret = nandroid_backup_partition(safe_backup_path, "/data"))) return;
        // ui_set_progress(0.40);
        // if (0 != (ret = nandroid_backup_partition(safe_backup_path, "/cache"))) return;
        // ui_set_progress(0.45);

        ui_print("\n-- Restoring Original System...\n");

        // if (0 != (ret = nandroid_restore_partition(orig_backup_path, "/system"))) return;
        // ui_set_progress(0.60);
        if (0 != (ret = nandroid_restore_partition(orig_backup_path, "/data"))) return;
        // ui_set_progress(0.80);
        // if (0 != (ret = nandroid_restore_partition(orig_backup_path, "/cache"))) return;
        // ui_set_progress(0.90);

        /* 3. wipe Dalvik Cache */
        __system("rm -r /data/dalvik-cache");
        __system("rm -r /cache/dalvik-cache");
        __system("rm -r /sd-ext/dalvik-cache");
        // ui_set_progress(0.95);

        /* 4. rm SAFE_SYSTEM_FILE */
        sprintf(cmd, "rm %s", SAFE_SYSTEM_FILE);
        ui_print("\n%s\n", cmd);
        __system(cmd);

        // ui_set_progress(1);
        ui_print("Swap to Original System Complete.\n");
    }
    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();

    safemode = get_safe_mode();
    ui_print("Safe System is now: %s!\n", safemode ? "ENABLED" : "DISABLED");
}

