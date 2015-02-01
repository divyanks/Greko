#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ebdr_replication_header.h"
#include "ebdr_log.h"


void replication_header_init(enum rep_hdr_role_t role, int pid)
{
	
	if(role == REP_HDR_SERVER)
	{
		replic_hdr_obj.ops = &replic_hdr_server_ops;
	}
	else
	{
		replic_hdr_obj.ops = &replic_hdr_client_ops;
	}
	
	replic_hdr_obj.ops->replic_hdr_create(role, pid);
}

