#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\Include\atdr_replication_header.h"
#include "..\Include\atdr_replication.h"


void replic_server_obj_create(int server_pid)
{
	replic_server_obj[server_pid].ops = &replic_server_operations;
}

void replic_server_obj_setup(struct ebdr_replication *replic_obj, int server_pid)
{

	/* Initialize replication protocol */
	replication_header_init(REP_HDR_SERVER, server_pid);

	/* Fill connection, replication protocol and disk details in replication object */
	replic_server_obj[server_pid].rep_conn = &ebdr_conn_server[server_pid];
	replic_server_obj[server_pid].rep_hdr = &replic_hdr_server_obj[server_pid]; 
	replic_server_obj[server_pid].rep_disk = &ebdr_disk_src_obj[ds_count];
	//  rs_count++;
}

void replic_server_obj_destroy(struct ebdr_replication *replic_obj)
{
	free(replic_obj);
}

struct ebdr_replication_operations replic_server_operations = 
{	
	.replic_obj_create = replic_server_obj_create,
	.replic_obj_setup = replic_server_obj_setup,
	.replic_obj_destroy = replic_server_obj_destroy
};
