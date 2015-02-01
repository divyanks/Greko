#ifndef _EBDR_DEVICE_H
#define _EBDR_DEVICE_H

#include <linux/idr.h>
#include <linux/kobject.h>
#include <linux/mutex.h>

enum ebdr_dev_state
{
        EBDR_DEV_CREATE,
        EBDR_BLKDEV_CREATE,
        EBDR_BLKDEV_ATTACH,
        EBDR_BLKDEV_DESTROY,
        EBDR_DEV_DESTROY
};

struct ebdr_backing_dev {
        struct kobject kobject;
        struct block_device *backing_bdev;
        struct block_device *md_bdev;
        struct disk_conf *disk_conf; /* RCU, for updates: resource->conf_update */
        sector_t known_size; /* last known size of that backing device */
};

struct ebdr_resource {
        char *name;
        struct kref kref;
        struct idr devices;             /* volume number to device mapping */
        struct list_head resources;
        struct mutex adm_mutex;         /* mutex to serialize administrative requests */
        spinlock_t req_lock;
        unsigned susp:1;                /* IO suspended by user */
        unsigned susp_nod:1;            /* IO suspended because no data */
        unsigned susp_fen:1;            /* IO suspended because fence peer handler runs */
};
#define DISK_NAME 64
struct ebdr_device
{
   char disk_name[DISK_NAME];
   struct ebdr_resource *resource;
   unsigned int minor;
   int vnr;
   struct ebdr_bitmap *bitmap; // to track bitmap
   struct ebdr_backing_dev *ldev;
   struct request_queue *rq_queue; // queue of requests
   struct gendisk *vdisk;
   struct block_device *this_bdev;
   struct kobject kobj;
};

extern int ebdr_init_bmap(struct ebdr_device *device, unsigned long);
#endif
