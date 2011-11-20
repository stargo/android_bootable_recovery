#ifndef SAFESTRAPCOMMANDS_H
#define SAFESTRAPCOMMANDS_H
#ifndef SAFE_SYSTEM_FILE
#define SAFE_SYSTEM_FILE "/systemorig/etc/safestrap/flags/alt_system_mode"
#endif

extern int safemode;

int check_systemorig_mount();
int get_safe_mode();
void show_safe_boot_menu();
void toggle_safe_mode();
#endif
