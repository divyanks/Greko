#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include "..\Include\atdr_disk.h"

int ds_count, dt_count;

int atdr_disk_init(enum disk_role_t disk_role, char *diskname,struct atdr_disk *atdr_disk_obj)
{
	if(disk_role == PRIMARY)
	{
		atdr_disk_obj->ops = &disk_source_ops;
	}
	else
	{
		atdr_disk_obj->ops = &disk_target_ops;
	}

	atdr_disk_obj->ops->disk_create(diskname, atdr_disk_obj);

	return 0;
}

