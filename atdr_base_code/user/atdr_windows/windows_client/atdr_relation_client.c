#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "..\Include\atdr_relation.h"
#include "..\Include\atdr_log.h"
#include "..\Include\atdr_replication_header.h"

void relation_client_obj_create(struct relation *relation_obj, int rid)
{
	if(rid > MAX_RELATIONS)
	{
		atdr_log(ATDR_INFO, "Max relation client objects reached! \n");
		stop_work("[relation_client_obj_create] max relation client objects reached ");
	}
}

int get_new_relation_client_id(void)
{
	int i;

	for(i = 0; i < MAX_RELATIONS; i++)
	{
		if(all_relation_clients[i].obj_state  != RELATION_OBJ_IN_USE)
			return i;
	}

	return -1;
}


int make_relation_client(int role, int pid, char *target_disk)
{
	all_relation_clients[pid].role = role;
	all_relation_clients[pid].partner_id = pid;
	all_relation_clients[pid].relation_id = pid;
	all_relation_clients[pid].obj_state = RELATION_OBJ_IN_USE;
	strcpy(all_relation_clients[pid].device, target_disk);
	all_relation_clients[pid].bitmap_size = replic_hdr_client_obj[pid].bitmap_size ;
	all_relation_clients[pid].grain_size = replic_hdr_client_obj[pid].grain_size ;

	/*
	   atdr_log(ATDR_INFO, "[make_relation_client] relation role=%d partner_id=%d relation_id=%d disk=%s\n", 
	   all_relation_clients[rid].role, all_relation_clients[rid].partner_id,
	   all_relation_clients[rid].relation_id, all_relation_clients[rid].device);
	   insert_into_relation(role, id, rid, RELATION_OBJ_IN_USE,target_disk, all_relation_clients[rid].bitmap_size,all_relation_clients[rid].grain_size,"atdrdbc");

	 */
	return pid;
}

void client_list_relations(void)
{
	int i;
	atdr_log(ATDR_INFO, "\n+-----------------------------------------------------------------+\n");
	atdr_log(ATDR_INFO, "|                         CLIENT RELATIONS                        |\n");
	atdr_log(ATDR_INFO, "+-----------------------------------------------------------------+\n");
	atdr_log(ATDR_INFO, "| Relation Role | Relation ID    |  Partner ID    | Device Name   |\n");
	atdr_log(ATDR_INFO, "+-----------------------------------------------------------------+\n");

	for(i = 0; i < pc_count; i++)
	{
		if(all_relation_clients[i].obj_state == RELATION_OBJ_IN_USE)
		{
			atdr_log(ATDR_INFO, "| %s\t| %d\t\t | %d\t\t  | %s\t  |\n",
					(all_relation_clients[i].role == 0)?"PRIMARY":"SECONDARY",
					all_relation_clients[i].relation_id, all_relation_clients[i].partner_id,  
					all_relation_clients[i].device);
		}
	}
	atdr_log(ATDR_INFO, "+-----------------------------------------------------------------+\n");

}

int mkrelation_from_db_on_client()
{
   return retrieve_from_relation(all_relation_clients);   
}

void relation_client_obj_destroy(int rid)
{
	atdr_log(ATDR_INFO, "[rel_client_destroy] Releasing relation_id = [%d]\n", rid);
	//memset(&all_relation_clients[rid], '\0', sizeof(relation_t));
	all_relation_clients[rid].obj_state = RELATION_OBJ_RELEASED;
}

relation_operations_t relation_client_ops =
{
	.relation_obj_create 	= relation_client_obj_create,
	.make_relation 			= make_relation_client,
	.list_relations 		= client_list_relations,
	.relation_obj_destroy 	= relation_client_obj_destroy
};
