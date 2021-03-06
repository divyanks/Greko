//#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include "ebdr_disk.h"
#include "ebdr_daemon.h"
#include "ebdr_log.h"

extern int recovery_mode;
extern IOCTL ioctl_obj;

void disk_source_create(char *name,struct ebdr_disk  *ebdr_disk_obj)
{
  int server_pid;

  server_pid = ds_count;

	/* Allocate memory for source disk & initialize */
	/* If obj_state is already in use then dont create disk source object */
	if(server_pid < MAX_DISKS)
	//if(server_pid < MAX_DISKS && (ebdr_disk_src_obj[server_pid].obj_state != DISK_OBJ_IN_USE))
	{
		ebdr_disk_obj->obj_state = DISK_OBJ_IN_USE;
		memcpy(ebdr_disk_obj->name, name, DISK_NAME);
		ebdr_log(INFO, "[disk_src_create] disk name: %s\n", ebdr_disk_obj->name);
		if(recovery_mode !=1)
		{
			ebdr_disk_obj->bitmap_size = ioctl_obj.u_bitmap_size;
			insert_into_disk(server_pid, ebdr_disk_obj->name, "-", ebdr_disk_obj->bitmap_size, "ebdrdbs");
		}
		ebdr_disk_obj->ops = &disk_source_ops;
		ebdr_disk_obj->ops->disk_mmap(server_pid);
		ds_count++;
		ebdr_log(INFO, "[disk_src_create] server_pid = %d\n", server_pid);
	}
	else
	{
		ebdr_log(ERROR, "[disk_src_create] Max disks reached !\n");
	}
}

int disk_source_setup(struct ebdr_disk *disk_obj)
{
	int disk_fd = 0;
	ebdr_log(INFO, "[src disk] setup snap disk name: %s\n", disk_obj->snap_name);
	if((disk_fd = open(disk_obj->snap_name, O_RDWR|O_LARGEFILE)) < 0)
	{
		perror("INVALID_FILE SOURCE "); /* This print will change later */
		return -1;
	}
	disk_obj->disk_fd = disk_fd;
	ebdr_log(INFO, "[disk] src fd: %d\n", disk_obj->disk_fd);
	return disk_fd;
}

#if 0
int disk_source_mmap_windows(int server_pid)
{
  long result;
  HANDLE hDevice = NULL;    
  BOOL bResult = FALSE;
  DWORD bytesReturned;
  char *str; 
  RG_PINPUT  Input = (RG_PINPUT)malloc(sizeof(RG_INPUT));
  POUTPUT Output = (POUTPUT)malloc(sizeof(OUTPUT));
  Input->len = 16384;
  printf("before create file\n");
  hDevice = CreateFile(L"\\\\.\\C:",
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      OPEN_EXISTING,
      FILE_FLAG_NO_BUFFERING,
      NULL);
  printf("after create file\n");

  if (hDevice == INVALID_HANDLE_VALUE) {

    result = GetLastError();
    printf("ERROR opening device...%x\n",result);
    exit(0);
  }
  printf("before DeviceIoControl \n");

  bResult = DeviceIoControl(hDevice,  
      IOCTL_GET_USER_KERNEL_MAPPING,
      Input,
      sizeof(RG_INPUT),
      Output,
      sizeof(OUTPUT),
      &bytesReturned,
      NULL);
  printf("after DeviceIoControl \n");
  str = (char *)Output->MappedUserAddress;
  printf("My name is %s", str);

}
#endif
#ifdef WINDOWS
int disk_source_mmap_windows(int server_pid)
{

	int memory_fd[5];
	char memctl_dev_name[5][64];

	sprintf(memctl_dev_name[server_pid], "%s", ebdr_disk_src_obj[server_pid].name);

	printf("[disk_source_mmap] memctl_dev_name[%d]: %s \n", server_pid, memctl_dev_name[server_pid]);
	if((memory_fd[server_pid] = open(memctl_dev_name[server_pid], O_RDWR)) < 0)
	{
		ebdr_log(FATAL, "[server_bitmap] memory_fd[%d] = %d \n", server_pid, errno);
		stop_work("memctrl open failed ");
	}

	ebdr_log(INFO, "[disk_source_mmap] memory_fd[%d] = %d \n", server_pid, memory_fd[server_pid]);
	ebdr_log(INFO, "[disk_source_mmap] calling mmap for dev[%d]:%s \n", server_pid, memctl_dev_name[server_pid]);

	//Setting up the backup area for Even iteration
	if (!(ebdr_disk_src_obj[server_pid].active_bitmap =  ioctl(memory_fd[server_pid], IOCTL_START_ACCEPTING_CHANGES_WINDOWS, &ioctl_obj )))
	{
		ebdr_log(FATAL, "plkt_init: Could not mmap trace buffer\n");
		stop_work("mmap failed ");
		goto mmap_trace_buffer_fail;
	} else {
		//Setting up the backup area for odd iteration
		ebdr_disk_src_obj[server_pid].active_bitmap_backup = (unsigned long int *)((char *)(ebdr_disk_src_obj[server_pid].active_bitmap) + ebdr_disk_src_obj[server_pid].bitmap_size); 
		ebdr_log(INFO, " *** Snapshot of bitmap successfull. *** \n");
	}

	if(ebdr_disk_src_obj[server_pid].active_bitmap == NULL)
	{
		ebdr_log(FATAL, "Mapping Failed\n");
		goto mmap_fail;
	}
	return 0;

mmap_fail: return -1;
mmap_trace_buffer_fail: return -2;

}
#endif

int disk_source_mmap_linux(int server_pid)
{
	int memory_fd[5];
	char memctl_dev_name[5][64];

	sprintf(memctl_dev_name[server_pid], "/dev/ebdr_memctrl%d", server_pid+1);

	printf("[disk_source_mmap] memctl_dev_name[%d]: %s \n", server_pid, memctl_dev_name[server_pid]);
	if((memory_fd[server_pid] = open(memctl_dev_name[server_pid], O_RDWR)) < 0)
	{
		ebdr_log(FATAL, "[server_bitmap] memory_fd[%d] = %d \n", server_pid, errno);
		stop_work("memctrl open failed ");
	}

	ebdr_log(INFO, "[disk_source_mmap] memory_fd[%d] = %d \n", server_pid, memory_fd[server_pid]);
	ebdr_log(INFO, "[disk_source_mmap] calling mmap for dev[%d]:%s \n", server_pid, memctl_dev_name[server_pid]);

	//Setting up the backup area for Even iteration
	if ((ebdr_disk_src_obj[server_pid].active_bitmap =  mmap(0, 2*ebdr_disk_src_obj[server_pid].bitmap_size, PROT_READ|PROT_WRITE,
					MAP_FILE | MAP_SHARED, memory_fd[server_pid], 0)) == MAP_FAILED)
	{
		ebdr_log(FATAL, "plkt_init: Could not mmap trace buffer\n");
		stop_work("mmap failed ");
		goto mmap_trace_buffer_fail;
	} else {
		//Setting up the backup area for odd iteration
		ebdr_disk_src_obj[server_pid].active_bitmap_backup = (unsigned long int *)((char *)(ebdr_disk_src_obj[server_pid].active_bitmap) + ebdr_disk_src_obj[server_pid].bitmap_size); 
		ebdr_log(INFO, " *** Snapshot of bitmap successfull. *** \n");
	}

	if(ebdr_disk_src_obj[server_pid].active_bitmap == NULL)
	{
		ebdr_log(FATAL, "Mapping Failed\n");
		goto mmap_fail;
	}
	return 0;

mmap_fail: return -1;
mmap_trace_buffer_fail: return -2;

}

int mkdisk_from_db_on_server(void)
{
	return retrieve_from_disk(ebdr_disk_src_obj);
}

void disk_source_destroy(struct ebdr_disk *disk_obj)
{
	// ds_count--;
	disk_obj->obj_state = DISK_OBJ_RELEASED;
}

void disk_source_unsetup(struct ebdr_disk *disk_obj)
{
	ebdr_log(INFO, "[disk_source_unsetup] closing source disk_fd: %d\n", disk_obj->disk_fd);
	close(disk_obj->disk_fd);
}

struct ebdr_disk_operations disk_source_ops =
{
	.disk_create 	= disk_source_create,
	.disk_setup 	= disk_source_setup,
#ifdef LINUX
	.disk_mmap	 	= disk_source_mmap_linux,
#elif WINDOWS
	.disk_mmap	 	= disk_source_mmap_windows,
#endif
	.disk_unsetup 	= disk_source_unsetup,
	.disk_destroy 	= disk_source_destroy
};
