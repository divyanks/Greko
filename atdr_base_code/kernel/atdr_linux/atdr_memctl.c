#include <linux/module.h>
#include <linux/ebdr.h>
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
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/string.h>

#include "ebdr_memctl.h"
#include "ebdr_error.h"
#include "ebdr_bitmap.h"
#include "ebdr_int.h"

struct class *ebdr_memctl_dev[MAX_MINOR]; 
char ctrldev_name[MAX_MINOR][124];
char *vmalloc_area[MAX_MINOR];
long unsigned int vmalloc_area_sz[MAX_MINOR];
extern struct ebdr_bitmap *bitmap;

void ebdr_free_vmalloc_area(int minor)
{
    if (vmalloc_area[minor])
    {
        vfree(vmalloc_area[minor]);
    }
    vmalloc_area[minor] = NULL;
}


static int ebdr_ctr_open(struct inode *inode, struct file *file)
{
	file->private_data = inode;
    printk(KERN_INFO "ebdr_memctrl: Device is opened %p\n", file->private_data);
    return 0;
}

static int ebdr_ctr_close(struct inode *inode, struct file *file){
    printk(KERN_INFO "ebdr_memctrl: Device is closed\n");
    return 0;
}

static int ebdr_dev_ctr_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret = 0;
	int minor_no = 0;
    printk("inode is: 0x%p \n", filp->private_data);
	minor_no = iminor(filp->private_data);
	printk("Minor No: %d\n", minor_no);
    printk("curent bitmap areas:: 0x%p \n", device[minor_no].bitmap->bitmap_area);
    printk("Remapping address:: 0x%p \n", vmalloc_area[minor_no]);
    ret = remap_vmalloc_range(vma, vmalloc_area[minor_no], 0);
    if (ret) {
        printk("remap page range failed\n");
        return -ENXIO;
    }
    return ret;
}

static struct file_operations ebdr_memctl_device_fops = {
	.open           = ebdr_ctr_open,
	.release        = ebdr_ctr_close,
	.owner          = THIS_MODULE,
	.mmap           = ebdr_dev_ctr_mmap,
};


int ebdr_alloc_memory_mmap(int minor)
{
    int ret = 0;
    vmalloc_area[minor] = vmalloc_user(2 * vmalloc_area_sz[minor]);
    if (!vmalloc_area[minor])
    {
        printk("Vmalloc: Failed to allocate memory\n");
        ret = -ENOMEM;
        return ret;
    }
    printk("vmalloc[%d] :: %lu bytes with start addr = 0x%p \n", minor, (2 * vmalloc_area_sz[minor]),
                                            vmalloc_area[minor]);
    printk("vmalloc[%d] :: next 4th page start addr = 0x%p \n", minor,
                                (vmalloc_area_sz[minor] + vmalloc_area[minor]));
    return ret;
}


int ebdr_register_kernel_memctl_device(int minor)
{

	sprintf(ctrldev_name[minor],"ebdr_memctrl%d",minor);
	//sprintf(ebdr_memctl_dev[minor]->name, "ebdr_memctrl%d", minor);

	printk("Registering memctrl driver ebdr_memctl_dev[%d].name = %s \n", minor, ctrldev_name[minor]);
	//printk("Registering memctrl driver ebdr_memctl_dev[%d].name = %s \n", minor, ebdr_memctl_dev[minor]->name);
	if (register_chrdev(MEMCTL_MAJOR, ctrldev_name[minor], &ebdr_memctl_device_fops))
		printk("unable to get major %d for memory control devs\n", MEMCTL_MAJOR);

	ebdr_memctl_dev[minor] = class_create(THIS_MODULE, ctrldev_name[minor]);
	if (IS_ERR(ebdr_memctl_dev[minor]))
		return PTR_ERR(ebdr_memctl_dev[minor]);

	printk("Created class ebdr_memctl_dev[%d] = %p\n", minor, ebdr_memctl_dev[minor]);
	device_create(ebdr_memctl_dev[minor], NULL, MKDEV(MEMCTL_MAJOR, minor), NULL , ctrldev_name[minor]);
	printk("Created Device ebdr_memctl_dev[%d] = %p\n", minor, ebdr_memctl_dev[minor]);

    return 0;
}
