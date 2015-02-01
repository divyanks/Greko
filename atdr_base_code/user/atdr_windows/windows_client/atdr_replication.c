#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\Include\atdr_replication.h"

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
