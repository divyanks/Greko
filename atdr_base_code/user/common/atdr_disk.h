#ifndef __EBDR_DISK_H
#define __EBDR_DISK_H

#define MAX_DISKS 2048

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


struct ebdr_erp
{
	

};

struct ebdr_disk;

struct ebdr_disk_operations 
{
	void (*disk_create)(char *diskname, struct ebdr_disk *ebdr_disk_obj);
	int (*disk_setup)(struct ebdr_disk * disk_obj);
	int (*disk_mmap)(int pid);
	void (*disk_unsetup)(struct ebdr_disk *disk_obj);
	void (*disk_destroy)(struct ebdr_disk *disk_obj);

};

extern struct ebdr_disk_operations disk_source_ops;
extern struct ebdr_disk_operations disk_target_ops;

/* ebdr disk 
 * disk_fd : fd of the disk
 * disk_size: size of the disk
 * ip: Ip address of the disk
 * bandwidth: bandwidth of the connection
 * disk_role: Role of the disk(primary/secondary)
 * disk_state: state of the disk
 * disk_erp: Error Recovery Procedures of ebdr disk
 * bitmap: user bitmap associated with the disk (Only for source disk)
 */
#define DISK_NAME 64
struct ebdr_disk
{
	char name[DISK_NAME];
	char snap_name[DISK_NAME];
	
	int disk_fd;
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

	struct ebdr_disk_operations *ops;
	
	struct ebdr_erp *disk_erp;
	struct ebdr_user_bitmap *bitmap;
};

struct ebdr_disk ebdr_disk_src_obj[MAX_DISKS];
struct ebdr_disk ebdr_disk_target_obj[MAX_DISKS];
extern int ds_count;
extern int dt_count;
#endif
