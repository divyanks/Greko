#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ebdr_log.h"
#include "ebdr_partner.h"


void partner_server_obj_create(partner_t *partner_obj)
{
	if(ps_count < MAX_PARTNERS)
	{
   		partner_obj->status = ACTIVE; /* hardcoded */
  	 	partner_obj->bandwidth = 100;  /* hardcoded */
	}
	else
	{
		ebdr_log(INFO, "Max partner server objects reached! \n");
		stop_work("[partner_server_obj_create] man partner server objects reached");
	}
}


int get_new_partner_server_id(void)
{
	int i;

	for(i = 0; i < MAX_PARTNERS; i++)
	{   
		if(all_partner_servers[i].obj_state != PARTNER_OBJ_IN_USE)
			return i;
	}

	return -1; 
}

void server_list_partners(void)
{
	int i;
	ebdr_log(INFO, "\n+----------------------------------------------------------------+\n");
	ebdr_log(INFO, "|              SERVER PARTNERS                                   |\n");
	ebdr_log(INFO, "+----------------------------------------------------------------+\n");
	ebdr_log(INFO, "| Partner ID    |  IP Address    | Bandwidth     | Socket Number |\n");
	ebdr_log(INFO, "+----------------------------------------------------------------+\n");

	for(i = 0; i < ps_count; i++)
	{
		if(all_partner_servers[i].obj_state == PARTNER_OBJ_IN_USE || all_partner_servers[i].obj_state == PARTNER_OBJ_RELEASED)
		{
			ebdr_log(INFO, "|\t%d\t| %s\t | %lu\t\t |  %d\t\t |\n", all_partner_servers[i].id, 
					all_partner_servers[i].ip,  all_partner_servers[i].bandwidth, all_partner_servers[i].socket_fd);
		}
	}
	ebdr_log(INFO, "+----------------------------------------------------------------+\n");

}

int get_partner_server_by_id(int id)
{
	int i;

	for(i = 0; i < MAX_PARTNERS; i++)
	{   
		if(all_partner_servers[i].id == id)
			return i;
	}

	return -1; 
}

int is_server_partner_id_valid(int pid)
{
	int i;
	for(i = 0; i < MAX_PARTNERS; i++)
	{
		if(all_partner_servers[i].id == pid)
		{
			ebdr_log(INFO, "[is_server_pid_valid] server partner id [%d] is valid\n", pid);
			return 1;
		}
	}
	return 0;
}


int make_partner_server(char *ip, unsigned long int bandwidth, int sfd, int pid)
{

		all_partner_servers[pid].id = pid;
		all_partner_servers[pid].obj_state = PARTNER_OBJ_IN_USE;
		strcpy(all_partner_servers[pid].ip, ip);
		all_partner_servers[pid].bandwidth = bandwidth;
		all_partner_servers[pid].socket_fd = sfd;
		insert_into_partner(pid, PARTNER_OBJ_IN_USE, ip, bandwidth, "ebdrdbs");
		ps_count++;
		ebdr_log(INFO, "[make_partner_server] partnership id=%d ip=%s bandwidth=%lu\n", all_partner_servers[pid].id,
				all_partner_servers[pid].ip, all_partner_servers[pid].bandwidth);
		return pid;
}

int mkpartner_from_db_on_server()
{ 
	return retrieve_from_partner(all_partner_servers);
}

void partner_server_obj_destroy(int pid)
{
	ebdr_log(INFO, "[partner_server_destroy] releasing pid :%d\n", pid);
	ebdr_log(INFO, "[partner_server_destroy] closing server connfd: [%d]\n", all_partner_servers[pid].socket_fd);
	close(all_partner_servers[pid].socket_fd);
	memset(&all_partner_servers[pid], '\0', sizeof(partner_t));
	all_partner_servers[pid].obj_state = PARTNER_OBJ_RELEASED;
}

partner_operations_t partner_server_ops =
{
	.partner_obj_create  	= partner_server_obj_create,
	.make_partner 			= make_partner_server,
	.list_partners 			= server_list_partners,
	.partner_obj_destroy 	= partner_server_obj_destroy
};
