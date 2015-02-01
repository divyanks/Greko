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

#include "ebdr_memctl.h"
#include "ebdr_error.h"
#include "ebdr_bitmap.h"
#include "ebdr_int.h"

struct miscdevice ebdr_memctl_dev[MAX_MINOR]; 
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
    printk("Remapping address:: 0x%p \n", bitmap->bitmap_area);
    ret = remap_vmalloc_range(vma, bitmap->bitmap_area, 0);
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

#if 0
{
    .minor = MISC_DYNAMIC_MINOR,
    .fops = &ebdr_memctl_device_fops,
};
#endif

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
    int err;

	printk(" inside register memctl device \n");
	sprintf(ctrldev_name[minor],"ebdr_memctrl%d",minor);
	ebdr_memctl_dev[minor].name = ctrldev_name[minor];

	ebdr_memctl_dev[minor].minor = MISC_DYNAMIC_MINOR;
	ebdr_memctl_dev[minor].fops = &ebdr_memctl_device_fops;
    printk(KERN_INFO "Registering ebdr memory control device: %s with minor:[%d]\n",
                            ebdr_memctl_dev[minor].name, ebdr_memctl_dev[minor].minor );
    err = misc_register(&ebdr_memctl_dev[minor]);
    if (err < 0)
    {
        printk(KERN_ERR "Failed to register misc device errno[%d]\n", err);
        return MISC_DEV_REG_ERROR;
    }
    return 0;
}
