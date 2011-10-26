#ifndef NANDROID_H
#define NANDROID_H

int nandroid_main(int argc, char** argv);
int nandroid_backup_partition(const char* backup_path, const char* root);
int nandroid_backup(const char* backup_path);
int nandroid_backup_multi(const char* backup_path, int backup_boot, int backup_system, int backup_data, int backup_cache, int backup_sdext, int backup_wimax, int backup_systemorig);
int nandroid_restore_partition(const char* backup_path, const char* root);
int nandroid_restore(const char* backup_path, int restore_boot, int restore_system, int restore_data, int restore_cache, int restore_sdext, int restore_wimax, int restore_systemorig);

#endif
