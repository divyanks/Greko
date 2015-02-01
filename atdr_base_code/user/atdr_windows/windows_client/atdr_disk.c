#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include "..\Include\atdr_disk.h"

int ds_count, dt_count;

int ebdr_disk_init(enum disk_role_t disk_role, char *diskname,struct ebdr_disk *ebdr_disk_obj)
{
	if(disk_role == PRIMARY)
	{
		ebdr_disk_obj->ops = &disk_source_ops;
	}
	else
	{
		ebdr_disk_obj->ops = &disk_target_ops;
	}

	ebdr_disk_obj->ops->disk_create(diskname, ebdr_disk_obj);

	return 0;
}

