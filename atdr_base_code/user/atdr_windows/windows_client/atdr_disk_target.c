//#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "..\Include\atdr_disk.h"
#include "..\Include\atdr_daemon.h"
#include "..\Include\atdr_log.h"
#include "..\Include\atdr_relation.h"
#include "..\Include\sys_common.h"
#include "..\Include\atdr_db.h"

extern int recovery_mode[MAX_PID];

void disk_target_create(char *name, struct atdr_disk *atdr_disk_obj)
{
  int client_pid;

  client_pid = dt_count;

	/* Allocate memory for source disk & initialize */
	if(client_pid < MAX_DISKS)
	//if(client_pid < MAX_DISKS && (atdr_disk_target_obj[client_pid].obj_state != DISK_OBJ_IN_USE))
	{
		atdr_disk_obj->obj_state = DISK_OBJ_IN_USE;
		memcpy(atdr_disk_obj->name, name, DISK_NAME);
		atdr_log(ATDR_INFO, "[target disk] disk name:%s\n", atdr_disk_obj->name);
		if (recovery_mode[client_pid] != 1)
		{
			atdr_disk_obj->bitmap_size = all_relation_clients[client_pid].bitmap_size;
			insert_into_disk(client_pid, atdr_disk_obj->name, "-", atdr_disk_obj->bitmap_size, atdr_disk_obj->obj_state, "atdrdbc"); 
		}
		atdr_disk_obj->ops = &disk_target_ops;
	}
	else
	{
		atdr_log(ATDR_ERROR, "[disk_target_create] MAX DISKS reached !! \n");
		//client_pid = 0;
	}
}

int prepare_to_write(HANDLE disk_fd)
{
	DWORD status;
	/* Step 1
	* Get volume name from drive
	*
	* Step 2: Umount the volume
	* Step 3: Lock the volume
	*
	*/
	BOOL bResult;

	bResult = DeviceIoControl(disk_fd,                // device to be queried
		FSCTL_DISMOUNT_VOLUME,  // operation to perform
		NULL, 0,                // no input buffer
		NULL, 0,      // output buffer
		&status,                  // # of bytes returned
		(LPOVERLAPPED)NULL);    // synchronous I/O

	bResult = DeviceIoControl(disk_fd,
		FSCTL_LOCK_VOLUME,
		NULL, 0, NULL,
		0, &status, NULL);

	return 0;
}


int disk_target_setup(struct atdr_disk *disk_obj)
{
	HANDLE disk_fd;

    #define WCHAR_BUFFER_SIZE 28
	WCHAR tmp[28];
	WCHAR disk_name[WCHAR_BUFFER_SIZE];
	int length = 0;
	wcscpy(disk_name, L"\\\\.\\");
	length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, disk_obj->name, -1, (LPWSTR)tmp,
		WCHAR_BUFFER_SIZE / sizeof(WCHAR));
	wcscat(&disk_name[4],tmp);
	if (length <= 0)
	{
		atdr_log(ATDR_ERROR, "Failed to convert to multibyte wide characters %d\n", GetLastError()); /* This print will change later */
		return -1;
	}
	if ((disk_fd = CreateFile(disk_name, GENERIC_WRITE | GENERIC_READ, // open for writing
		FILE_SHARE_WRITE, // share for writing
		NULL, // default security
		OPEN_EXISTING, // create new file only
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_RANDOM_ACCESS,
		// normal file archive and impersonate client
		NULL)) == INVALID_HANDLE_VALUE)
	{
		atdr_log(ATDR_ERROR, "INVALID_FILE SOURCE %d %s\n", GetLastError()); /* This print will change later */
	   return -1;
	}

	disk_obj->disk_fd = disk_fd;
	prepare_to_write(disk_fd);

	atdr_log(ATDR_INFO, "[disk] target fd:%d\n", disk_obj->disk_fd);
	dt_count++;
	atdr_log(ATDR_INFO, "[disk_target_setup] dt_count = %d\n", dt_count);
	return 0;
}

void disk_target_destroy(struct atdr_disk *disk_obj)
{
	atdr_log(ATDR_INFO, "[disk_target_destroy] setting obj_state to DISK_OBJ_RELEASED \n");
	disk_obj->obj_state = DISK_OBJ_RELEASED;
}

int mkdisk_from_db_on_client(void)
{
	return retrieve_from_disk(atdr_disk_target_obj);
}


void disk_target_unsetup(struct atdr_disk *disk_obj)
{
	atdr_log(ATDR_INFO, "[disk_target_unsetup] closing target disk_fd: %d\n", disk_obj->disk_fd);
    CloseHandle(disk_obj->disk_fd); 
}

struct atdr_disk_operations disk_target_ops =
{
	.disk_create 	= disk_target_create,
	.disk_setup 	= disk_target_setup,
	.disk_unsetup 	= disk_target_unsetup,
	.disk_destroy 	= disk_target_destroy
};