#ifndef __ATDR_DISK_H
#define __ATDR_DISK_H

#include <Windows.h>

#define MAX_DISKS 2048
#define DISK_NAME 64

enum disk_obj_state_t
{
	DISK_OBJ_FREE,
	DISK_OBJ_IN_USE,
	DISK_OBJ_PAUSE,
	DISK_OBJ_RESUME,
	DISK_OBJ_RELEASED
};


enum disk_state_t
{
	DISK_FOUND,
	DISK_SCSCI_FAIL,
	DISK_CONN_FAIL,
	DISK_NOT_FOUND

};


enum disk_role_t
{
	PRIMARY,
	SECONDARY
};


struct atdr_disk;

struct atdr_disk_operations
{
	void(*disk_create)(char *diskname, struct atdr_disk *atdr_disk_obj);
	int(*disk_setup)(struct atdr_disk * disk_obj);
	int(*disk_mmap)(int pid);
	void(*disk_unsetup)(struct atdr_disk *disk_obj);
	void(*disk_destroy)(struct atdr_disk *disk_obj);

};

extern struct atdr_disk_operations disk_source_ops;
extern struct atdr_disk_operations disk_target_ops;

struct atdr_disk
{
	char name[DISK_NAME];
	char snap_name[DISK_NAME];

	HANDLE disk_fd;
	int disk_size;
	unsigned long int bitmap_size;

	int disk_start_lba;
	int ip;
	int bandwidth;

	unsigned long int *active_bitmap; //This is the area which mmap to offset 0
	unsigned long int *active_bitmap_backup; //This is the area which mmap to offset+grainsize 
	enum disk_role_t disk_role;
	enum disk_state_t disk_state;
	enum disk_obj_state_t obj_state;

	struct atdr_disk_operations *ops;

	struct atdr_erp *disk_erp;
	struct atdr_user_bitmap *bitmap;
};

struct atdr_disk atdr_disk_src_obj[MAX_DISKS];
struct atdr_disk atdr_disk_target_obj[MAX_DISKS];
extern int ds_count;
extern int dt_count;
int atdr_disk_init(enum disk_role_t disk_role, char *diskname, struct atdr_disk *atdr_disk_obj);
int mkdisk_from_db_on_server(void);
#endif

