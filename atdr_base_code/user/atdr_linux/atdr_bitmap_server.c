#include "sys_common.h"
#include "ebdr_bitmap_server.h"
#include "ebdr_bitmap_user.h"
#include "ebdr_ioctl_cmds.h"
#include "ebdr_grain_req.h"
#include "ebdr_daemon.h"
#include "ebdr_relation.h"
#include "ebdr_log.h"

#define EBDR_TRACE_LOG_SIZE                4*4096
extern ioctl_fd;
extern pthread_mutex_t condition_mutex;
extern pthread_cond_t  condition_cond;
extern IOCTL ioctl_obj;
extern int recovery_mode[MAX_PID];

int mmaped_devices[10];

int get_total_bits_set(unsigned long int *dirtied_bitmap)
{
	int bits_set = 0;
	int bit_pos = 0;

	while(bit_pos < MAX_BITS_LEN)
	{
		if(ebdr_check_bit_set(dirtied_bitmap, bit_pos))
		{
			bits_set++;
		}
		bit_pos++;
	}
	return bits_set;
}

int ebdr_copy_mmaped_bitmap(ulong_t *dest, ulong_t *src, size_t nbytes)
{
	size_t i, nwords = nbytes/sizeof (ulong_t);
	for(i = 1; i <= nwords; i++)
		*dest++ = *src++;
	return (0);
}

void ebdr_clear_active_bitmap(ulong_t *dest, size_t nbytes)
{
	ulong_t val = 0x0;
	size_t i, nwords = nbytes/sizeof (ulong_t);
	for(i = 1; i <= nwords; i++)
		*dest++ = val;
	return;
}

static void bitmap_server_create(int server_pid)
{
	bitmap_server_obj[server_pid].bitmap_area = malloc(BITMAP_SIZE);
	if (!bitmap_server_obj[server_pid].bitmap_area)
	{
		ebdr_log(FATAL, "error in allocating memory");
		stop_work("bitmap server malloc failed\n");
	}
	memset(bitmap_server_obj[server_pid].bitmap_area, '\0', (BITMAP_SIZE));
	bitmap_server_obj[server_pid].bitmap_size = BITMAP_SIZE;

	bitmap_server_obj[server_pid].bitmap_state = EBDR_UBITMAP_INIT;
	bitmap_server_obj[server_pid].mmap_state = MMAP_UNUSED;
	bitmap_server_obj[server_pid].bmap_user = KERNEL;
	ebdr_log(INFO, "[server_bitmap_init] bitmap intialized and assing server ops\n");
	bitmap_server_obj[server_pid].ops = &bitmap_server_operations;
}

int server_bitmap_load_from_memory(struct ebdr_user_bitmap *ubitmap_obj, int server_pid)
{
	int user;
	int ret;
	unsigned long int gsize; /* VJ changes */

	ioctl_obj.index = server_pid+1;
	user = prepare_and_send_ioctl_cmd(EBDRCTL_GET_USER);
	if(user < 0)
	{
		stop_work("get user ioctl failed");
	}
	
	ubitmap_obj->bitmap_size = ebdr_disk_src_obj[server_pid].bitmap_size;
	printf("active bitmap area 0x%lx\n", *(ebdr_disk_src_obj[server_pid].active_bitmap));
	printf("backup bitmap area 0x%lx\n", *(ebdr_disk_src_obj[server_pid].active_bitmap_backup));

	ebdr_log(INFO, "[load_from_memory] ioctl_obj.bmap_user: %d ioctl_obj.index = %d\n", ioctl_obj.bmap_user, ioctl_obj.index);
	//ebdr_log(INFO, "[load_from_memory] ubitmap:0x%lx \n", *(ubitmap_obj->bitmap_area));
	//ebdr_log(INFO, "[load_from_memory] ebdr_disk active_bitmap[%d]:0x%lx \n", server_pid,
	//	   	*(ebdr_disk_src_obj[server_pid].active_bitmap));
	ebdr_log(INFO, "[load_from_memory] after ioctl get_user = %d \n",user);

	/* Don't FLIP bitmap area in recovery mode */
	if((recovery_mode[server_pid] == 0) && (ebdr_conn_server[server_pid].obj_state != CONN_OBJ_RESUME))
	{
		if (user == KERNEL)
		{
			ioctl_obj.bmap_user = USER;
			ubitmap_obj->bmap_user = USER;
			ebdr_copy_mmaped_bitmap(ubitmap_obj->bitmap_area, ebdr_disk_src_obj[server_pid].active_bitmap, ubitmap_obj->bitmap_size);
			ebdr_log(INFO, "[load_from_memory] KERNEL->USER bitmap_area:0x%lx *****\n", *(ubitmap_obj->bitmap_area));
		} else {
			ioctl_obj.bmap_user = KERNEL;
			ubitmap_obj->bmap_user = KERNEL;
			ebdr_copy_mmaped_bitmap(ubitmap_obj->bitmap_area, 
					ebdr_disk_src_obj[server_pid].active_bitmap_backup, ubitmap_obj->bitmap_size);
			ebdr_log(INFO, "[load_from_memory] USER->KERNEL bitmap_area:0x%lx *****\n", *(ubitmap_obj->bitmap_area));
		}
		ebdr_log(INFO, "sending EBDR_FLIP_BITMAP_AREA to kernel... \n");
		ret = prepare_and_send_ioctl_cmd(EBDR_FLIP_BITMAP_AREA);
		if(ret < 0)
		{
			stop_work("flip bitmap area failed ");
		}
		ebdr_log(INFO, "return value of flip_bitmap_area ioctl = %d\n", ret);
	}
	else
	{
		ebdr_conn_server[server_pid].obj_state = CONN_OBJ_IN_USE;
		if(*(ebdr_disk_src_obj[server_pid].active_bitmap))
		{
			ioctl_obj.bmap_user = USER;
			ubitmap_obj->bmap_user = USER;
			ebdr_copy_mmaped_bitmap(ubitmap_obj->bitmap_area, ebdr_disk_src_obj[server_pid].active_bitmap, 
					ubitmap_obj->bitmap_size);
		}
		else
		{
			ioctl_obj.bmap_user = KERNEL;
			ubitmap_obj->bmap_user = KERNEL;
			ebdr_copy_mmaped_bitmap(ubitmap_obj->bitmap_area, ebdr_disk_src_obj[server_pid].active_bitmap_backup,
				   	ubitmap_obj->bitmap_size);
		}
	}
	ebdr_log(INFO, "[bitmap_server_setup] After ioctl bitmap:0x%lx \n", *(ubitmap_obj->bitmap_area));
	return 0;
mmap_fail: return -1;
mmap_trace_buffer_fail: return -2;
}


static int bitmap_server_setup(struct ebdr_user_bitmap *ubitmap_obj, int server_pid)
{
	int ret; 
	unsigned long gsize;

	ebdr_log(INFO, "[bitmap_server_setup] calling load_ebdr_bitmap_from_disk server_pid:[%d] \n", server_pid);
	ret = ubitmap_obj->ops->load_bitmap_from_memory(ubitmap_obj, server_pid);
	if(ret < 0)
	{
		ebdr_log(FATAL, "Loading server bitmap from disk failed\n");
		stop_work("load bitmap from memory failed ");
	}

	/* get total bits set in the bitmap_area */
	ubitmap_obj->uptill.total_bits_set = ubitmap_obj->ops->get_total_bits_set(ubitmap_obj->bitmap_area);
	ebdr_log(INFO, "[bitmap_server_setup] totalbitsset = %d\n", ubitmap_obj->uptill.total_bits_set);
	ebdr_log(INFO, "[bitmap_server_setup] assigning bitmap state as READy \n");
	ubitmap_obj->bitmap_state = EBDR_UBITMAP_READY;
	return (0);

mmap_trace_buffer_fail:
	return (-2);
mmap_fail:
	return (-1);
}


int ebdr_test_bit2(int num, char *arg)
{
	ulong_t temp = num%8;
	char mask = (1 << (8-1-temp));
	ulong_t off = num/8;
	char *var;
	if (!arg)
		return (0);
	var = ((char *)arg + off);
	return (*var & mask);
}


int ebdr_check_bit_set(unsigned long *bitmap_area, int pos)
{
	int ret = 1, pnum;
	char *paddr = (char *)bitmap_area;

	pnum = pos >> (PAGE_SHIFT+3);
	if ((pnum * NUM_BITS_PER_PAGE) > MAX_BITS_LEN) {
		ebdr_log(FATAL, "ebdr_check_bit_set: out of bound access\n");
		return (0);
	}
	paddr = paddr + (pnum * NUM_BITS_PER_PAGE/8);
	ret = ebdr_test_bit2(pos & NUM_BITS_PER_PAGE_MASK, (char *)paddr);
	return (ret);
}


/*
 *  * test bit
 *   */
static inline int test_bit(unsigned long nr, const volatile void *addr)
{
	return 1UL & (((const volatile unsigned int *) addr)[nr >> 5] >> (nr & 31));
}


inline int is_bit_set(unsigned long *bitmap_area, int pos)
{
	if(!(pos < MAX_BITS_LEN)) {
		assert(0);
	}

	if (bitmap_area) {
		return (test_bit(pos, bitmap_area));
	} else {
		assert(0);
	}
}

static void bitmap_server_destroy(struct ebdr_user_bitmap *ubitmap, int pid)
{
	ebdr_log(INFO, "[bitmap_server_destroy] freeing bitmap...\n");
	free(ubitmap->bitmap_area);
        if( ubitmap->bmap_user == USER)
	{
             //clear active bitmap
             ebdr_clear_active_bitmap(ebdr_disk_src_obj[pid].active_bitmap, ebdr_disk_src_obj[pid].bitmap_size);
			 ebdr_log (INFO, "Cleaning active bitmap \n");
	} else {
             //clear active bitmap backup
             ebdr_clear_active_bitmap(ebdr_disk_src_obj[pid].active_bitmap_backup, ebdr_disk_src_obj[pid].bitmap_size);
			 ebdr_log (INFO, "Cleaning active bitmap_backup \n");
	}
}

struct ebdr_bitmap_operations bitmap_server_operations =
{
	.ebdr_bitmap_create 	 = bitmap_server_create,
	.ebdr_bitmap_setup 		 = bitmap_server_setup,
	.load_bitmap_from_memory = server_bitmap_load_from_memory,
	.get_total_bits_set 	 = get_total_bits_set,
	.ebdr_bitmap_destroy 	 = bitmap_server_destroy
};
