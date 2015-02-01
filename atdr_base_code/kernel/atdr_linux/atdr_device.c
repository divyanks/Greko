
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/device.h>
#include <linux/version.h>
#include "ebdr_int.h"
#include "ebdr_device.h"
#include "ebdr_bitmap.h"
#include "ebdr_int.h"

extern IOCTL ioctl_obj;

struct ebdr_backing_dev bd_dev[MAX_MINOR];

static int ebdr_device_open(struct block_device *blkdev, fmode_t mode)
{
	printk(KERN_INFO "[EBDR] In open call \n");
	return 0;
}

static int ebdr_device_release(struct gendisk *gd, fmode_t mode)
{
	printk(KERN_INFO "[EBDR] In release call \n");
	return 0;
}

static int ebdr_device_ioctl(struct block_device *bdev, fmode_t mode,
		unsigned int cmd, void *mem_addr)
{
//	struct ebdr_device *device = NULL;
//	device = device[minor];
	switch(cmd)
	{
#if 0
		default:
			if (device && device->ldev && device->this_bdev && device->this_bdev->bd_disk
					&& device->ldev->backing_bdev->bd_disk->fops && device->ldev->backing_bdev->bd_disk->fops->ioctl)
			{
				printk("** device name = %s \n", device->vdisk->disk_name);
				printk("** backing device name = %s \n", device->ldev->backing_bdev->bd_disk->disk_name);
				device->ldev->backing_bdev->bd_disk->fops->ioctl(device->ldev->backing_bdev, mode,
						cmd, (long unsigned int)mem_addr);
			}
			printk("EBDR1: Unknown IOCTL Received ..!!\n");
#endif
	}
	return 0;
}


const struct block_device_operations ebdr_device_ops = {
	.owner   = THIS_MODULE,
	.open    = ebdr_device_open,
	.release = ebdr_device_release,
	.ioctl   = ebdr_device_ioctl,
};



#ifndef disk_to_dev
/* disk_to_dev was introduced with 2.6.27. Before that the kobj was directly in gendisk */


static inline struct kobject *ebdr_kobj_of_disk(struct gendisk *disk)
{
	return &disk->kobj;
}
#else
static inline struct kobject *ebdr_kobj_of_disk(struct gendisk *disk)
{
	return &disk_to_dev(disk)->kobj;
}
#endif

void ebdr_free_bdev(struct ebdr_backing_dev *ldev)
{
	if (ldev == NULL)
		return;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
	close_bdev_exclusive(ldev->backing_bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);
#else
	blkdev_put(ldev->backing_bdev, FMODE_READ | FMODE_WRITE | FMODE_EXCL);
#endif
	kobject_del(&ldev->kobject);
	kobject_put(&ldev->kobject);
}

static void ebdr_device_destroy(struct kobject *kobj)
{
	struct ebdr_device *device = container_of(kobj, struct ebdr_device, kobj);

	printk("[DEV_DESTROY] Cleaning up ebdr device...\n");
	if (device->this_bdev)
		bdput(device->this_bdev);

	ebdr_free_bdev(device->ldev);
	device->ldev = NULL;

	put_disk(device->vdisk);
	blk_cleanup_queue(device->rq_queue);
}

static struct kobj_type ebdr_device_kobj_type = {
	.release = ebdr_device_destroy,
};


void ebdr_blkdev_destroy(struct ebdr_device *device, int minor_num)
{
	printk("[BLK_DESTROY] Cleaning up block device \n");
	if (device) {
		printk("[BLK_DESTROY] device->ldev \n");
		if (device->ldev) {
			device->ldev = NULL;
		}
		if (device->vdisk) {
			printk("[BLK_DESTROY] device->vdisk \n");
			put_disk(device->vdisk);
			del_gendisk(device->vdisk);
			device->vdisk = NULL;
		}
        if(blkdev[minor_num] != NULL)
		{
          //Call close/put only if open is successful

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
			close_bdev_exclusive(blkdev[minor_num], FMODE_READ | FMODE_WRITE | FMODE_EXCL);
#else
			printk("[BLK_DESTROY] blkdev[%d] \n", minor_num);
			blkdev_put(blkdev[minor_num], FMODE_READ | FMODE_WRITE | FMODE_EXCL);
#endif
		}

		printk("[BLK_DESTROy] deleting device->this_bdev \n");	
		if (device->this_bdev)
			bdput(device->this_bdev);
		blk_cleanup_queue(device->rq_queue);
		printk("[BLK_DESTROy] Freeing ebdr device \n");	
	}
}

inline sector_t ebdr_get_capacity(struct block_device *blkdev)
{
	return blkdev ? i_size_read(blkdev->bd_inode) >> 9 : 0;
}

int ebdr_device_attach_blkdev(struct ebdr_device *device)
{
	printk("***ioctl_obj.src_disk = '%s' \n", ioctl_obj.src_disk);
	/* Get block device of the given path */
	blkdev[minor] = blkdev_get_by_path(ioctl_obj.src_disk,
			FMODE_READ | FMODE_WRITE | FMODE_EXCL, device);

	if(IS_ERR(blkdev[minor]))
	{
            blkdev[minor] = NULL;
            return -1;

	}

	printk("blkdev[%d] = %p\n", minor, blkdev[minor]);
	/* Assign this block device to the backing block device of ebdr_backing_dev */
	bd_dev[minor].backing_bdev = blkdev[minor];
	printk("bd_dev[%d].backing_bdev = %p\n", minor, bd_dev[minor].backing_bdev);
	/* Assign ebdr_backing_dev to backing device of ebdr_device*/
	device->ldev = &bd_dev[minor];

	bd_dev[minor].known_size = ebdr_get_capacity(blkdev[minor]);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
	printk("blkdev size:%lu \n", bd_dev[minor].known_size);
#else
	printk("blkdev size:%llu \n", bd_dev[minor].known_size);
#endif
	/* Set size of gendisk of ebdr_device */
	set_capacity(device->vdisk, bd_dev[minor].known_size);// block layer
	device->this_bdev->bd_inode->i_size = bd_dev[minor].known_size;// inode layer
	return 0;
}

int ebdr_device_create(int minor)
{
	int vnr = 10;
	
	device[minor].minor = minor;
	device[minor].vnr = vnr;
        
    //Calculate the bitmap size based on device size and grain, right now hardcode it to 32 bit
    //Instead of calling alloc call the ebdr mm function that will vmalloc and give
    //mmap capable area to ebdr_init_bmap
	ebdr_init_bmap(&device[minor], 32); 

	printk("initalized bmap success.. returning device\n");
	return 0;
}


int ebdr_blkdev_create(int minor, struct ebdr_device *device)
{
	struct gendisk *disk;
	struct request_queue *q;
	struct kobject *parent;
	int refs = 2;

	printk("before blk_alloc_queue\n");
	q = blk_alloc_queue(GFP_KERNEL); //creating a queue for communication b/w layer fs and block
	if (!q)
		goto out_no_q;

	printk("queue alloced successfully\n");
	device->rq_queue = q;
	q->queuedata   = device;

	printk("before alloc_disk\n");
	disk = alloc_disk(1);
	if (!disk)
		goto out_no_disk;

	device->vdisk = disk;

	disk->queue = q;
	disk->major = EBDR_MAJOR;
	disk->first_minor = minor;
	disk->fops = &ebdr_device_ops;
	sprintf(disk->disk_name, "ebdr%d", minor);
	disk->private_data = device;
	printk("before bdget\n");
	device->this_bdev = bdget(MKDEV(EBDR_MAJOR, minor));
	printk("device->this_bdev = %p\n", device->this_bdev);
	/* we have no partitions. we contain only ourselves. */
	device->this_bdev->bd_contains = device->this_bdev;
	printk("before blk_queue_make_request\n");
	blk_queue_make_request(q, ebdr_make_request);
#ifdef REQ_FLUSH
	blk_queue_flush(q, REQ_FLUSH | REQ_FUA);
#endif

	blk_queue_max_hw_sectors(q, EBDR_MAX_BIO_SIZE_SAFE >> 8);
	blk_queue_bounce_limit(q, BLK_BOUNCE_ANY);

	printk("before add_disk\n");
	add_disk(disk);
#if 0
  parent = ebdr_kobj_of_disk(disk);

	/* one ref for both idrs and the the add_disk */
	if (kobject_init_and_add(&device->kobj, &ebdr_device_kobj_type, parent, "ebdr"))
		goto out_del_disk;
	while (refs--)
		kobject_get(&device->kobj);
#endif

	return 0;
out_del_disk:
	del_gendisk(device->vdisk);
	put_disk(disk);
out_no_disk:
	blk_cleanup_queue(q);
out_no_q:
	return -1;

}

int ebdr_device_state(enum ebdr_dev_state state)
{
	int i = 0;
    int ret = 0;
	switch(state)
	{
		case EBDR_DEV_CREATE: 
			printk("creating ebdr device\n");
			if ( (ret = ebdr_device_create(minor)) < 0) {
                            goto error;
                        }
			if( (ret = ebdr_device_state(EBDR_BLKDEV_CREATE)) < 0) {
                            goto error;
                        }  
			break;

		case EBDR_BLKDEV_CREATE:
			printk("creating the block device\n");
			if( (ret = ebdr_blkdev_create(minor, &device[minor])) < 0) {
                            goto error;
                        }
			if( (ret = ebdr_device_state(EBDR_BLKDEV_ATTACH)) < 0) {
				goto error;
			}
			break;

		case EBDR_BLKDEV_ATTACH: 
			printk("attaching the device \n");
			if((ret = ebdr_device_attach_blkdev(&device[minor])) < 0) {
				ebdr_blkdev_destroy(&device[minor], minor);
				goto error;
			}
			break;

		case EBDR_BLKDEV_DESTROY:
			for(i = 1; i < minor; i++)
			{
				printk("deleting blkdev for minor: %d\n", i);
				ebdr_blkdev_destroy(&device[i], i);
			}
			break;

		case EBDR_DEV_DESTROY:
			//ebdr_device_destroy(device);
			break;
		default:
			break;
	}
error:
	return ret;
}
