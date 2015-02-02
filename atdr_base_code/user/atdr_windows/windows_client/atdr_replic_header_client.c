#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\Include\atdr_replication_header.h"
#include "..\Include\atdr_log.h"

// extern FILE *atdrlog_fp;

void replic_hdr_client_create(enum rep_hdr_role_t rep_hdr_role, int client_pid)
{
	if(client_pid < MAX_HDR)
	{
		/* Allocate and initialize replication header */
		replic_hdr_client_obj[client_pid].obj_state = REP_HDR_OBJ_IN_USE;
		replic_hdr_client_obj[client_pid].role = rep_hdr_role;
		replic_hdr_client_obj[client_pid].ops = &replic_hdr_client_ops;
	}
	else
	{
		atdr_log(ATDR_INFO, "Max replic header client objects reached !\n");
		stop_work("[replic_hdr_client_create] max replic header client objects reached ");
	}
}

struct replication_header *replic_hdr_client_nego(struct replication_header *rep_hdr_obj, int sockfd, int client_pid)
{

	/* send sync, recv sync-ack, send ack */
	replic_hdr_client_obj[client_pid].bitmap_size = rep_hdr_obj->bitmap_size;
	replic_hdr_client_obj[client_pid].grain_size = rep_hdr_obj->grain_size;
	replic_hdr_client_obj[client_pid].chunk_size = rep_hdr_obj->chunk_size;

	atdr_log(ATDR_INFO, "[client_proto_nego] Agreed on these sizes bitmap:%lu grain:%lu chunksize=%lu client_pid: %d\n",
			replic_hdr_client_obj[client_pid].bitmap_size, replic_hdr_client_obj[client_pid].grain_size,
			replic_hdr_client_obj[client_pid].chunk_size, client_pid );

	return rep_hdr_obj;

}	


void replic_hdr_client_destroy(struct replication_header *rep_hdr_obj)
{
	atdr_log(ATDR_INFO, "[replic_hdr_client_destroy] setting obj_state to REP_HDR_OBJ_RELEASED\n");
	rep_hdr_obj->obj_state = REP_HDR_OBJ_RELEASED;
}

struct replication_header_operations replic_hdr_client_ops =
{
	.replic_hdr_create 	= replic_hdr_client_create,
	.replic_hdr_nego 	= replic_hdr_client_nego,
	.replic_hdr_destroy = replic_hdr_client_destroy
};

