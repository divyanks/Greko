#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\Include\atdr_replication.h"
#include "..\Include\atdr_replication_header.h"


void replic_client_obj_create(int client_pid)
{
 	atdr_log(ATDR_INFO, "[replic_client_create] client_pid = %d\n", client_pid);	
	replic_client_obj[client_pid].ops = &replic_client_operations;
}

void replic_client_obj_setup(struct atdr_replication *replic_obj, int client_pid)
{
	/* Initialize replication protocol */
	replication_header_init(REP_HDR_CLIENT, client_pid);

	/* Fill connection, replication protocol details in replication object */

	replic_client_obj[client_pid].rep_conn = &atdr_conn_client[client_pid];
	replic_client_obj[client_pid].rep_hdr = &replic_hdr_client_obj[client_pid];
}

/* resync table 09-10-14 */
int mkresync_from_db_on_client()
{
	return retrieve_from_resync(replic_client_obj, atdr_disk_target_obj); 
}


void replic_client_obj_destroy(struct atdr_replication *replic_obj)
{
	free(replic_obj);
}


struct atdr_replication_operations replic_client_operations = 
{	
	.replic_obj_create 	= replic_client_obj_create,
	.replic_obj_setup = replic_client_obj_setup,
	.replic_obj_destroy = replic_client_obj_destroy
};
