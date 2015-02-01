#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ebdr_bitmap_user.h"
#include "ebdr_grain_req.h"
#include "ebdr_log.h"
#include "ebdr_conn.h"
#include "ebdr_replication_header.h"
#include "ebdr_replication.h"

int replication_init(enum rep_role_t rep_role, int pid)
{

	if(rep_role == REP_LOCAL)
	{
		replic_req.ops = &replic_server_operations;
	}
	else
	{
		replic_req.ops = &replic_client_operations;
	
	}
	replic_req.ops->replic_obj_create(pid);
	
	return 0;
}
