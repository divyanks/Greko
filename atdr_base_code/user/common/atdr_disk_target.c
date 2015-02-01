//#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ebdr_disk.h"
#include "ebdr_daemon.h"
#include "ebdr_log.h"

void disk_target_create(char *name, struct ebdr_disk *ebdr_disk_obj)
{
  int client_pid;

  client_pid = dt_count;

	/* Allocate memory for source disk & initialize */
	if(client_pid < MAX_DISKS)
	//if(client_pid < MAX_DISKS && (ebdr_disk_target_obj[client_pid].obj_state != DISK_OBJ_IN_USE))
	{
		ebdr_disk_obj->obj_state = DISK_OBJ_IN_USE;
		memcpy(ebdr_disk_obj->name, name, DISK_NAME);
		ebdr_log(INFO, "[target disk] disk name:%s\n", ebdr_disk_obj->name);
		ebdr_disk_obj->ops = &disk_target_ops;
	}
	else
	{
		ebdr_log(ERROR, "[disk_target_create] MAX DISKS reached !! \n");
		//client_pid = 0;
	}
}

int disk_target_setup(struct ebdr_disk *disk_obj)
{
	int disk_fd = 0;
	if((disk_fd = open(disk_obj->name, O_RDWR | O_CREAT| O_TRUNC| O_SYNC | O_LARGEFILE)) < 0)
	{
		perror("INVALID_FILE "); /* This print will change later */
		ebdr_log(INFO, "[disk] target fd:%s\n", disk_obj->name);
		return -1;
	}
	disk_obj->disk_fd = disk_fd;
	ebdr_log(INFO, "[disk] target fd:%d\n", disk_obj->disk_fd);
	dt_count++;
	ebdr_log(INFO, "[disk_target_setup] dt_count = %d\n", dt_count);
	return disk_fd;
}

void disk_target_destroy(struct ebdr_disk *disk_obj)
{
	ebdr_log(INFO, "[disk_target_destroy] setting obj_state to DISK_OBJ_RELEASED \n");
	disk_obj->obj_state = DISK_OBJ_RELEASED;
}

void disk_target_unsetup(struct ebdr_disk *disk_obj)
{
	ebdr_log(INFO, "[disk_target_unsetup] closing target disk_fd: %d\n", disk_obj->disk_fd);
	close(disk_obj->disk_fd);
}

struct ebdr_disk_operations disk_target_ops =
{
	.disk_create 	= disk_target_create,
	.disk_setup 	= disk_target_setup,
	.disk_unsetup 	= disk_target_unsetup,
	.disk_destroy 	= disk_target_destroy
};
