/* ebdr_main.c */


#include <linux/module.h>
#include <asm/uaccess.h>
#include <asm/types.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/mempool.h>
#include <linux/memcontrol.h>
#include <linux/mm_inline.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/reboot.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/highmem.h>
#include <linux/moduleparam.h>
#include <asm/io.h>
#include <asm/ioctl.h>
#include <linux/sched.h>
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <linux/ebdr_limits.h>
#include <linux/ebdr_config.h>

#ifdef COMPAT_HAVE_LINUX_BYTEORDER_SWABB_H
#include <linux/byteorder/swabb.h>
#else
#include <linux/swab.h>
#endif
#include "ebdr_int.h"
#include "ebdr_req.h"
#include "ebdr_ioctl_cmds.h"
#include "ebdr_bitmap.h"
#include "ebdr_memctl.h"
#include "ebdr_error.h"

MODULE_AUTHOR("Vijay Kumar <vijaykumar@ptechnosoft.com>");
MODULE_DESCRIPTION("ebdr - Enterprise Block Device Replicator" REL_VERSION);
MODULE_VERSION(REL_VERSION);
MODULE_LICENSE("GPL");
MODULE_PARM_DESC(minor_count, "Approximate number of ebdr devices ("
		__stringify(EBDR_MINOR_COUNT_MIN) "-" __stringify(EBDR_MINOR_COUNT_MAX) ")");
MODULE_ALIAS_BLOCKDEV_MAJOR(EBDR_MAJOR);

#include <linux/moduleparam.h>

#define MAX_EBDR_DEVICES 10


module_param(minor_count, uint, 0444);


IOCTL ioctl_obj;

extern struct ebdr_bitmap *bitmap;
extern char *vmalloc_area[MAX_MINOR+1];
extern long unsigned int vmalloc_area_sz[MAX_MINOR+1];
struct kmem_cache *ebdr_request_cache;
mempool_t *ebdr_request_mempool;
long unsigned int u_grain_size;
long unsigned int u_bitmap_size;
unsigned int minor_count = EBDR_MINOR_COUNT_DEF;
int minor = 1;
struct ebdr_device device[MAX_MINOR];
extern struct class *ebdr_memctl_dev[MAX_MINOR];
struct block_device *blkdev[MAX_MINOR];
extern char ctrldev_name[MAX_MINOR][124];

/* /dev/ebdr_base */
static struct gendisk *bd_disk;
static struct request_queue *bdqueue;
extern char *ebdr_device_buf;


static int ebdr_open(struct block_device *blkdev, fmode_t mode)
{
	printk(KERN_INFO "[EBDR] In open call \n");
	return 0;
}

static int ebdr_release(struct gendisk *gd, fmode_t mode)
{
	printk(KERN_INFO "[EBDR] In release call \n");
	return 0;
}

static void ebdr_minor_cleanup(int minor)
{
	int i = minor;
	ebdr_device_state(EBDR_BLKDEV_DESTROY);
	printk("Freeing vmalloc area[%d]\n", i);
	ebdr_free_vmalloc_area(i);
	printk(KERN_INFO "ebdr: vmalloc freed.\n");

	printk("Unregistering memctrl[%d]\n", i);
	device_destroy(ebdr_memctl_dev[i], MKDEV(MEMCTL_MAJOR, i));
	class_destroy(ebdr_memctl_dev[i]);
	unregister_chrdev(MEMCTL_MAJOR, ctrldev_name[i]);
	printk(KERN_INFO "ebdr_memctl: module cleanup done.\n");
}

static int ebdr_ctl_ioctl(struct block_device *blkdev, fmode_t mode,
                unsigned int cmd, void *mem_addr)
{
	int err = 0;
  int i=0; // santosh
  char str[20]; // santosh
  int device_exists=0; //santosh

	switch(cmd)
	{
		case EBDRCTL_DEV_CREATE:
			printk("Received EBDRCTL_DEV_CREATE from user \n");
			if(copy_from_user(&ioctl_obj, (IOCTL *)mem_addr, sizeof(ioctl_obj)))
			{
				goto err_copy_from_user;
			}
			vmalloc_area_sz[minor] = ioctl_obj.u_bitmap_size;

			printk("Minor number: %d\n", minor);
			err = ebdr_alloc_memory_mmap(minor);
			if (err)
			{
				printk(KERN_ERR "Failed to allocate memory map\n");
				return MEMCTL_DEV_REG_ERROR;
			}
			err = ebdr_register_kernel_memctl_device(minor);
			if (err)
			{
				printk(KERN_ERR "Failed to register 'memctl device'\n");
				return MEMCTL_DEV_REG_ERROR;
			}
			err = ebdr_device_state(EBDR_DEV_CREATE);
			if( err < 0 )
			{
				ebdr_minor_cleanup(minor); 
				printk(KERN_ERR "Failed to register 'memctl device'\n");
				return MEMCTL_DEV_REG_ERROR;
			}

			device[minor].bitmap->bmap_user = KERNEL;
			device[minor].bitmap->bitmap_state = B_ALLOCATED;
			memset(device[minor].disk_name, '\0', sizeof(device[minor].disk_name));
			memcpy(device[minor].disk_name, ioctl_obj.src_disk, sizeof(device[minor].disk_name));
			minor++;
			break;
		case EBDRCTL_BLKDEV_CREATE:
			printk("Received EBDRCTL_BLKDEV_CREATE from user \n");
			break;
		case EBDRCTL_BLKDEV_ATTACH:
			printk("Received EBDRCTL_BLKDEV_ATTACH from user \n");
			break;
		case EBDRCTL_BLKDEV_DESTROY:
			printk("Received EBDRCTL_DEV_DESTROY from user \n");
			ebdr_device_state(EBDR_BLKDEV_DESTROY);
			break;
		case EBDRCTL_DEV_DESTROY:
			printk("Received EBDRCTL_DEV_DESTROY from user \n");
			break;
		case EBDRCTL_GET_GRAIN_SIZE:
			printk("Received 'EBDRCTL_GET_GRAIN_SIZE' from user \n");
			if(copy_to_user(mem_addr, &grain_size, sizeof (unsigned long)))
			{
				printk("copy to user failed\n");
				return -1;
			}
			printk("ebdr_ctl_ioctl: grain size %lu sent %lu\n", grain_size, *(unsigned long *)mem_addr);
			break;
		case EBDRCTL_SET_GRAIN_SIZE:
			printk("Received 'EBDRCTL_SET_GRAIN_SIZE' from user");
			if(copy_from_user(&ioctl_obj, (IOCTL *)mem_addr, sizeof(ioctl_obj)))
			{
				goto err_copy_from_user;
			}
			spin_lock(&device[ioctl_obj.index].bitmap->bm_lock);

			printk("Warning: Overwriting current grain & bitmap size by user requested value\n");
			printk("IOCTL:u_grain_size = %lu, u_bitmap_size = %lu \n",
					ioctl_obj.u_grain_size, ioctl_obj.u_bitmap_size);
			/* Overwriting current grain & bitmap size */
			u_grain_size = ioctl_obj.u_grain_size;  
			vmalloc_area_sz[minor] = ioctl_obj.u_bitmap_size;
			bitmap->grain_size = ioctl_obj.u_grain_size;
			bitmap->bitmap_size = (vmalloc_area_sz[minor] * 8);

			spin_unlock(&device[ioctl_obj.index].bitmap->bm_lock);

			break;
		case EBDRCTL_GET_USER:
			{ 
				int user = 0;
				if(copy_from_user(&ioctl_obj, (IOCTL *) mem_addr, sizeof(ioctl_obj)))
				{
					goto err_copy_from_user;
				}

				spin_lock(&device[ioctl_obj.index].bitmap->bm_lock);

				//ebdr_print_bmap(&device[ioctl_obj.index]);
				user =  device[ioctl_obj.index].bitmap->bmap_user;
				spin_unlock( &device[ioctl_obj.index].bitmap->bm_lock);

				//Could be inaccurate as its outside spin lock can't print in spinlock
				printk("bitmap_area: 0x%lx \n", *(unsigned long int *)device[ioctl_obj.index].bitmap->bitmap_area);
				//printk("total_bits_set: %d \n", get_total_bits_set((unsigned long int *)device[ioctl_obj.index].bitmap->bitmap_area));
				return user;


				break;
			}

		case EBDR_FLIP_BITMAP_AREA:
			printk("Received 'EBDR_FLIP_BITMAP_AREA' from user\n");
			if(copy_from_user(&ioctl_obj, (IOCTL *)mem_addr, sizeof(ioctl_obj)))
			{
				goto err_copy_from_user;
			}

			printk("[FLIP] device[%d].bitmap->bitmap_area = 0x%lx \n", ioctl_obj.index,
					(unsigned long int)(device[ioctl_obj.index].bitmap->bitmap_area));

			spin_lock(&device[ioctl_obj.index].bitmap->bm_lock);


			if( device[ioctl_obj.index].bitmap->bmap_user == USER) {
				//device[ioctl_obj.index].bitmap->bitmap_area -=  vmalloc_area_sz[ioctl_obj.index];
				device[ioctl_obj.index].bitmap->bitmap_area =  vmalloc_area[ioctl_obj.index];
				device[ioctl_obj.index].bitmap->bmap_user = KERNEL;
			} else {
				device[ioctl_obj.index].bitmap->bitmap_area += vmalloc_area_sz[ioctl_obj.index];
				device[ioctl_obj.index].bitmap->bmap_user = USER;
			}
			spin_unlock(&device[ioctl_obj.index].bitmap->bm_lock);

			// For debugging may be inaccurate outside spinlock
			if(device[ioctl_obj.index].bitmap->bmap_user == USER)
				printk("[FLIP_USER] starting address of bitmap_area = 0x%p\n", device[ioctl_obj.index].bitmap->bitmap_area);
			else 
				printk("[FLIP_KERNEL] starting address of bitmap_area = 0x%p\n", device[ioctl_obj.index].bitmap->bitmap_area);

			break;

    case EBDRCTL_BLKDEV_SEARCH:
      device_exists = 0;
			if(copy_from_user(&ioctl_obj, (IOCTL *)mem_addr, sizeof(ioctl_obj)))
			{
				goto err_copy_from_user;
			}

      for(i=0; i< MAX_EBDR_DEVICES; i++)
      {
        if(device[i].disk_name)
        {
        if(memcmp(device[i].disk_name, ioctl_obj.src_disk, 20) == 0)
          device_exists =1;
        }
      }
      copy_to_user(mem_addr, &device_exists, sizeof(int));
			break;


		default:
			printk("Unknown IOCTL Received ..!!\n");
	}

	return 0;
err_copy_from_user:
	printk("copy from user failed\n");
	return -1;
}

const struct block_device_operations ebdr_ops = {
	.owner 	 = THIS_MODULE,
	.open 	 = ebdr_open,
	.release = ebdr_release,
	.ioctl   = ebdr_ctl_ioctl,
};

static void ebdr_destroy_mempools(void)
{
	if (ebdr_request_mempool)
		mempool_destroy(ebdr_request_mempool);
	if (ebdr_request_cache)
		kmem_cache_destroy(ebdr_request_cache);
	ebdr_request_mempool = NULL;
	ebdr_request_cache   = NULL;
}

static int ebdr_create_mempools(void)
{
	const int number = (EBDR_MAX_BIO_SIZE/PAGE_SIZE) * minor_count;
	/* prepare our caches and mempools */
	ebdr_request_mempool = NULL;
	ebdr_request_cache   = NULL;

	/* caches */
	ebdr_request_cache = kmem_cache_create(
			"ebdr_req", sizeof(struct ebdr_request), 0, 0, NULL);
	if (ebdr_request_cache == NULL)
		goto Enomem;
	/* mempools */
	ebdr_request_mempool = mempool_create(number,
			mempool_alloc_slab, mempool_free_slab, ebdr_request_cache);
	if (ebdr_request_mempool == NULL)
		goto Enomem;
	return 0;
Enomem:
	ebdr_destroy_mempools(); /* in case we allocated some */
	return -1;
}

static void ebdr_cleanup(void)
{
	int i = 0;
	int max = minor;
	ebdr_device_state(EBDR_BLKDEV_DESTROY);
	for(i = 1; i < max; i++)
	{
		printk("Freeing vmalloc area[%d]\n", i);
		ebdr_free_vmalloc_area(i);
	}
    /* /dev/ebdr_base */
    del_gendisk(bd_disk);
    blk_cleanup_queue(bdqueue);
    bdqueue = NULL;
    put_disk(bd_disk);
    unregister_blkdev(EBDR_MAJOR, "ebdr_dev");
    printk(KERN_INFO "ebdr: module cleanup done.\n");
	for(i = 1; i < max; i++)
	{
		printk("Unregistering memctrl[%d]\n", i);
		device_destroy(ebdr_memctl_dev[i], MKDEV(MEMCTL_MAJOR, i));
		class_destroy(ebdr_memctl_dev[i]);
		unregister_chrdev(MEMCTL_MAJOR, ctrldev_name[i]);
		minor--;
	}

	printk(KERN_INFO "ebdr_memctl: module cleanup done.\n");
}


int __init ebdr_init(void)
{
	int err;

	if (minor_count < EBDR_MINOR_COUNT_MIN || minor_count > EBDR_MINOR_COUNT_MAX) {
		printk(KERN_ERR
				"ebdr: invalid minor_count (%d)\n", minor_count);
#ifdef MODULE
		return -1;
#else
		minor_count = EBDR_MINOR_COUNT_DEF;
#endif
	}

	err = register_blkdev(EBDR_MAJOR, "ebdr_dev"); 
	if (err) {
		printk(KERN_ERR
				"ebdr: unable to register block device major %d\n",
				EBDR_MAJOR);
		return err;
	}

	//ebdr_create_mempools(); // creating cache and mempools: find out why does it need

	printk(KERN_INFO "ebdr: initialized. "
			"Version: " REL_VERSION " (api:%d/proto:%d-%d)\n",
			API_VERSION, PRO_VERSION_MIN, PRO_VERSION_MAX);
	printk(KERN_INFO "ebdr: %s\n", ebdr_buildtag());
	printk(KERN_INFO "ebdr: registered as block device major %d\n", EBDR_MAJOR);

	/* /dev/ebdr_base */ // INITIALIZING A DISK BELOW
	if( !(bd_disk=alloc_disk(1)) ) { 		

		printk("alloc_disk failed ...\n");
		return -1;
	}
	bd_disk->major = EBDR_MAJOR;
	bd_disk->first_minor = 0;
	bdqueue = blk_alloc_queue(GFP_KERNEL);
	bd_disk->queue = bdqueue;
	sprintf(bd_disk->disk_name, "ebdr_base");
	bd_disk->fops = &ebdr_ops;
	add_disk( bd_disk ); // add the disk to the system
	return 0; /* Success! */

}
module_init(ebdr_init)
module_exit(ebdr_cleanup)
