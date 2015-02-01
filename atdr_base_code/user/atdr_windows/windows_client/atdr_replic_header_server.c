#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\Include\atdr_replication_header.h"
#include "..\Include\atdr_log.h"

// extern FILE *ebdrlog_fp;

void replic_hdr_server_create(enum rep_hdr_role_t rep_hdr_role, int server_pid)
{
	/* Allocate and initialize replication header */
	if(server_pid < MAX_HDR)
	{
		replic_hdr_server_obj[server_pid].obj_state = REP_HDR_OBJ_IN_USE;
		replic_hdr_server_obj[server_pid].role = rep_hdr_role;
		replic_hdr_server_obj[server_pid].ops = &replic_hdr_server_ops;
	}
	else
	{
		ebdr_log(EBDR_INFO, "MAX Replic header server objects reached !\n");
		stop_work("[replic_hdr_server_create] max replic header server objects reached");
	}

}

void replic_hdr_server_setup(unsigned long int bitmap_size, unsigned long int grain_size, 
		unsigned long int chunk_size, int pid)
{
	ebdr_log(EBDR_INFO, "\n in replic_hdr_server_setup \n");
	replic_hdr_server_obj[pid].bitmap_size = bitmap_size; 
	replic_hdr_server_obj[pid].grain_size = grain_size; 	   
	replic_hdr_server_obj[pid].chunk_size = chunk_size; 	
	replic_hdr_server_obj[pid].partner_id = pid; 		
	
	ebdr_log(EBDR_INFO, "[server_setup] server_obj[%d]  bitmap_size: %lu grain_size:%lu chunk_size:%lu\n",
			pid, replic_hdr_server_obj[pid].bitmap_size,
			replic_hdr_server_obj[pid].grain_size,
			replic_hdr_server_obj[pid].chunk_size);
}


unsigned long int get_minimum(unsigned long int server_var, unsigned long int client_var)
{
    if(server_var < client_var)
    {
        return server_var;
    }
    else
    {
        return client_var;
    }

}

struct replication_header *replic_hdr_server_nego(struct replication_header *rep_hdr_obj, int sockfd, int server_pid)
{
	/* recv sync, send sync-ack, recv ack */
	/* currently this code is commented..negotiation will not happen
	 * server hardcoded values will be assigned to client
	 * enable this code later
	 */
#if 0
	rplc_proto_hdr_server->bitmap_size =  get_minimum(rplc_proto_hdr_server->bitmap_size, rep_header->bitmap_size);
	rplc_proto_hdr_server->grain_size  =  get_minimum(rplc_proto_hdr_server->grain_size, rep_header->grain_size);
	rplc_proto_hdr_server->chunk_size   =  get_minimum(rplc_proto_hdr_server->chunk_size, rep_header->chunk_size);
#endif

	rep_hdr_obj->bitmap_size = replic_hdr_server_obj[server_pid].bitmap_size;
	rep_hdr_obj->grain_size = replic_hdr_server_obj[server_pid].grain_size;
	rep_hdr_obj->chunk_size = replic_hdr_server_obj[server_pid].chunk_size;

	return rep_hdr_obj;
}


void replic_hdr_server_destroy(struct replication_header *rep_hdr_obj)
{
	rep_hdr_obj->obj_state = REP_HDR_OBJ_RELEASED;
}

struct replication_header_operations replic_hdr_server_ops =
{
    .replic_hdr_create 	= replic_hdr_server_create,
    .replic_hdr_setup 	= replic_hdr_server_setup,
    .replic_hdr_nego 	= replic_hdr_server_nego,
    .replic_hdr_destroy = replic_hdr_server_destroy
};

