#include "sys_common.h"
#include "ebdr_replication.h"
#include "ebdr_replication_header.h"
#include "ebdr_conn.h"
#include "ebdr_disk.h"
#include "ebdr_bitmap_user.h"
#include "ebdr_log.h"

void replic_client_obj_create(int client_pid)
{

 	ebdr_log(INFO, "[replic_client_create] client_pid = %d\n", client_pid);	
	replic_client_obj[client_pid].ops = &replic_client_operations;

}

void replic_client_obj_setup(struct ebdr_replication *replic_obj, int client_pid)
{
	/* Initialize replication protocol */
	replication_header_init(REP_HDR_CLIENT, client_pid); 

	/* Fill connection, replication protocol details in replication object */
	
	replic_client_obj[client_pid].rep_conn = &ebdr_conn_client[client_pid];
	replic_client_obj[client_pid].rep_hdr  = &replic_hdr_client_obj[client_pid];
}

/* resync table 09-10-14 */
int mkresync_from_db_on_client()
{
	return retrieve_from_resync(replic_client_obj, ebdr_disk_target_obj);
}



void replic_client_obj_destroy(struct ebdr_replication *replic_obj)
{
	free(replic_obj);
}

struct ebdr_replication_operations replic_client_operations = 
{	
	.replic_obj_create 	= replic_client_obj_create,
	.replic_obj_setup 	= replic_client_obj_setup,
	.replic_obj_destroy = replic_client_obj_destroy,
};
