#ifndef _EBDR_INT_H
#define _EBDR_INT_H

#include <linux/types.h>
#include <linux/version.h>
#include <linux/genhd.h>
#include <linux/idr.h>
#include <linux/ebdr_config.h>
#include <linux/ebdr_limits.h>
#include <linux/miscdevice.h>
#include <linux/blkdev.h>

#include "compat.h"
#include "ebdr_device.h"

#ifndef EBDR_MAJOR
#define EBDR_MAJOR 187
#endif

#ifndef MEMCTL_MAJOR
#define MEMCTL_MAJOR 188
#endif

#define MAX_MINOR 64

#define EBDR_MAX_BIO_SIZE_SAFE (1U << 12)       /* Works always = 4k */
#define EBDR_MAX_BIO_SIZE (1U << 20)
//#define BLKDEV_PATH "/dev/mapper/myvg-lvol0" //hardcoded
#define BLKDEV_PATH "/dev/sdb" //hardcoded

typedef struct user_ioctl
{
    char src_disk[64];
    long unsigned int u_grain_size;
	long unsigned int u_bitmap_size;
	int bmap_user;
	int bmap_ver;
	int index;

}IOCTL;

enum {      
	KERNEL,
	USER,
};


int ebdr_init(void);

extern struct kmem_cache *ebdr_request_cache;
extern mempool_t *ebdr_request_mempool;

extern unsigned int minor_count;
extern int minor;

extern struct ebdr_device device[MAX_MINOR];
extern struct block_device *blkdev[MAX_MINOR];
extern enum ebdr_dev_state state;
extern int ebdr_device_state(enum ebdr_dev_state state);

extern const struct block_device_operations ebdr_ops;

/* ebdr_memctl*/
//extern struct miscdevice ebdr_memctl_dev[MAX_MINOR];
extern int ebdr_register_kernel_memctl_device(int minor);
extern int ebdr_alloc_memory_mmap(int minor);
extern void ebdr_free_vmalloc_area(int minor);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
extern int ebdr_make_request(struct request_queue *q, struct bio *bio);
static inline struct block_device *blkdev_get_by_path(const char *path, fmode_t mode, void *holder)
{

	return open_bdev_exclusive(path, mode, holder);

}
#else
extern void ebdr_make_request(struct request_queue *q, struct bio *bio);
#endif

#endif
