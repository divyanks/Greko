#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ebdr_log.h"
#include "ebdr_relation.h"
#include "ebdr_replication_header.h"

void relation_server_obj_create(struct relation *relation_obj, int rid)
{
	if(rid < MAX_RELATIONS)
	{
		ebdr_log(INFO, "[relation_server] created relation_server_obj \n");
	}
	else
	{
		ebdr_log(INFO, "Max relation server objects reached! \n");
		stop_work("[relation_server_obj_create] max relation server objects reached ");
	}
}

int get_new_relation_server_id(void)
{
	int i;

	for(i = 0; i < MAX_RELATIONS; i++)
	{   
		if(all_relation_servers[i].obj_state != RELATION_OBJ_IN_USE)
			return i;
	}

	return -1; 
}

int make_relation_server(int role, int pid, char *disk)
{
	int ret;

	all_relation_servers[pid].role = role;
	all_relation_servers[pid].partner_id = pid;
	all_relation_servers[pid].relation_id = pid;
	all_relation_servers[pid].obj_state = RELATION_OBJ_IN_USE;
	strcpy(all_relation_servers[pid].device, disk);
	all_relation_servers[pid].bitmap_size = replic_hdr_server_obj[pid].bitmap_size;
	all_relation_servers[pid].grain_size  = replic_hdr_server_obj[pid].grain_size;

	return pid;

}

void server_list_relations(void)
{
	int i;
	ebdr_log(INFO, "\n+-----------------------------------------------------------------+\n");
	ebdr_log(INFO, "|                       SERVER RELATIONS                          |\n");
	ebdr_log(INFO, "+-----------------------------------------------------------------+\n");
	ebdr_log(INFO, "| Relation Role | Relation ID    |  Partner ID    | Device Name   |\n");
	ebdr_log(INFO, "+-----------------------------------------------------------------+\n");

	for(i = 0; i < ps_count; i++)
	{
		if(all_relation_servers[i].obj_state == RELATION_OBJ_IN_USE)
		{
			ebdr_log(INFO, "| %s \t| %d\t\t | %d\t\t  | %s\t  |\n",
					(all_relation_servers[i].role == 0)?"PRIMARY":"SECONDARY",
					all_relation_servers[i].relation_id, all_relation_servers[i].partner_id,
					all_relation_servers[i].device);
		}
	}
	ebdr_log(INFO, "+-----------------------------------------------------------------+\n");

}

int mkrelation_from_db_on_server()
{
	return retrieve_from_relation(all_relation_servers);  
}

void relation_server_obj_destroy(int rid)
{
	ebdr_log(INFO, "[rel_server_destroy] Releasing relation_id = %d\n", rid);
	//memset(&all_relation_servers[rid], '\0',sizeof(relation_t));
	all_relation_servers[rid].obj_state = RELATION_OBJ_RELEASED;
}

relation_operations_t relation_server_ops =
{
	.relation_obj_create 	= relation_server_obj_create,
	.make_relation 			= make_relation_server,
	.list_relations 		= server_list_relations,
	.relation_obj_destroy 	= relation_server_obj_destroy
};
