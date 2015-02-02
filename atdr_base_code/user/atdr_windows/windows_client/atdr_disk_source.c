//#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "..\Include\atdr_disk.h"
#include "..\Include\atdr_daemon.h"
#include "..\Include\atdr_log.h"
#include "..\Include\sys_common.h"

extern int recovery_mode[MAX_PID]; 
extern IOCTL ioctl_obj;

int insert_into_disk(int pid, char *name, char *snap_name, unsigned long int bitmap_size, int state, char *db_name);

void disk_source_create(char *name,struct atdr_disk  *atdr_disk_obj)
{
  int server_pid;

  server_pid = ds_count;

	/* Allocate memory for source disk & initialize */
	/* If obj_state is already in use then dont create disk source object */
	if(server_pid < MAX_DISKS && (atdr_disk_src_obj[server_pid].obj_state != DISK_OBJ_IN_USE))
	{
		atdr_disk_obj->obj_state = DISK_OBJ_IN_USE;
		memcpy(atdr_disk_obj->name, name, DISK_NAME);
		atdr_log(ATDR_INFO, "[disk_src_create] disk name: %s\n", atdr_disk_obj->name);
		if(recovery_mode[server_pid] !=1)
		{
			atdr_disk_obj->bitmap_size = ioctl_obj.u_bitmap_size;
			insert_into_disk(server_pid, atdr_disk_obj->name, "-", atdr_disk_obj->bitmap_size,atdr_disk_obj->obj_state, "atdrdbs"); 
		}
		atdr_disk_obj->ops = &disk_source_ops;
		// atdr_disk_obj->ops->disk_mmap(server_pid);
		ds_count++;
		atdr_log(ATDR_INFO, "[disk_src_create] server_pid = %d\n", server_pid);
	}
	else
	{
		atdr_log(ATDR_ERROR, "[disk_src_create] Max disks reached !\n");
	}
}

int disk_source_setup(struct atdr_disk *disk_obj)
{
	HANDLE disk_fd;

    #define WCHAR_BUFFER_SIZE 128
	WCHAR disk_name[WCHAR_BUFFER_SIZE];
	int length = 0;
	length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, disk_obj->snap_name, -1, (LPWSTR)disk_name,
		WCHAR_BUFFER_SIZE / sizeof(WCHAR));
	if (length <= 0)
	{
		atdr_log(ATDR_ERROR, "Failed to convert to multibyte wide characters %d\n", GetLastError()); /* This print will change later */
		return -1;
	}

	atdr_log(ATDR_INFO, "[src disk] setup snap disk name: %s\n", disk_obj->snap_name);
	if ((disk_fd = CreateFile(disk_name, GENERIC_WRITE|GENERIC_READ, // open for writing
		FILE_SHARE_READ, // share for reading

		NULL, // default security

		OPEN_EXISTING, // create new file only

		FILE_ATTRIBUTE_NORMAL || FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,

		// normal file archive and impersonate client

		NULL)) == INVALID_HANDLE_VALUE)
	{
		atdr_log(ATDR_ERROR, "INVALID_FILE SOURCE %d\n", GetLastError()); /* This print will change later */
		return -1;
	}
	disk_obj->disk_fd = disk_fd;
	atdr_log(ATDR_INFO, "[disk] src fd: %d\n", disk_obj->disk_fd);

	return 0;
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

	sprintf(memctl_dev_name[server_pid], "%s", atdr_disk_src_obj[server_pid].name);

	printf("[disk_source_mmap] memctl_dev_name[%d]: %s \n", server_pid, memctl_dev_name[server_pid]);
	if((memory_fd[server_pid] = open(memctl_dev_name[server_pid], O_RDWR)) < 0)
	{
		atdr_log(FATAL, "[server_bitmap] memory_fd[%d] = %d \n", server_pid, errno);
		stop_work("memctrl open failed ");
	}

	atdr_log(ATDR_INFO, "[disk_source_mmap] memory_fd[%d] = %d \n", server_pid, memory_fd[server_pid]);
	atdr_log(ATDR_INFO, "[disk_source_mmap] calling mmap for dev[%d]:%s \n", server_pid, memctl_dev_name[server_pid]);

	//Setting up the backup area for Even iteration
	if (!(atdr_disk_src_obj[server_pid].active_bitmap =  ioctl(memory_fd[server_pid], IOCTL_START_ACCEPTING_CHANGES_WINDOWS, &ioctl_obj )))
	{
		atdr_log(FATAL, "plkt_init: Could not mmap trace buffer\n");
		stop_work("mmap failed ");
		goto mmap_trace_buffer_fail;
	} else {
		//Setting up the backup area for odd iteration
		atdr_disk_src_obj[server_pid].active_bitmap_backup = (unsigned long int *)((char *)(atdr_disk_src_obj[server_pid].active_bitmap) + atdr_disk_src_obj[server_pid].bitmap_size); 
		atdr_log(ATDR_INFO, " *** Snapshot of bitmap successfull. *** \n");
	}

	if(atdr_disk_src_obj[server_pid].active_bitmap == NULL)
	{
		atdr_log(FATAL, "Mapping Failed\n");
		goto mmap_fail;
	}
	return 0;

mmap_fail: return -1;
mmap_trace_buffer_fail: return -2;

}
#endif


int mkdisk_from_db_on_server(void)
{
//	return retrieve_from_disk(atdr_disk_src_obj); santosh
	return 0;
}

void disk_source_destroy(struct atdr_disk *disk_obj)
{
	// ds_count--;
	disk_obj->obj_state = DISK_OBJ_RELEASED;
}

void disk_source_unsetup(struct atdr_disk *disk_obj)
{
	atdr_log(ATDR_INFO, "[disk_source_unsetup] closing source disk_fd: %d\n", disk_obj->disk_fd);
  CloseHandle(disk_obj->disk_fd); 
}

struct atdr_disk_operations disk_source_ops =
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
