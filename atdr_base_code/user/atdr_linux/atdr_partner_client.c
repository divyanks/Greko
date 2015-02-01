#include "sys_common.h"
#include "ebdr_log.h"
#include "ebdr_partner.h"


void partner_client_obj_create(partner_t *partner_obj)
{
	if(pc_count < MAX_PARTNERS)
	{
		partner_obj->status = ACTIVE; /* hardcoded */
	}
	else
	{
		ebdr_log(INFO, "Max partner client objects reached! \n");
		stop_work("[partner_client_obj_create] max partner client objects reached ");
	}
}

int get_new_partner_client_id(void)
{
	int i;
	for(i = 0; i < MAX_PARTNERS; i++)
	{
		/* Give new partner id such that we can reuse the partner id which got deleted */
		if((all_partner_clients[i].obj_state != PARTNER_OBJ_IN_USE) &&
		  (all_partner_clients[i].obj_state != PARTNER_OBJ_IN_USE) && 	
		  (all_partner_clients[i].obj_state != PARTNER_OBJ_RELEASED) )
		return i;
	}
	return -1;
}
int get_partner_client_by_id(int id)
{
	int i;

	for(i = 0; i < MAX_PARTNERS; i++)
	{
		if(all_partner_clients[i].id == id)
			return i;
	}

	return -1;
}


void client_list_partners(void)
{
	int i;
	ebdr_log(INFO, "\n+----------------------------------------------------------------+\n");
	ebdr_log(INFO, "|              CLIENT PARTNERS		                         |\n");
	ebdr_log(INFO, "+----------------------------------------------------------------+\n");
	ebdr_log(INFO, "| Partner ID	|  IP Address	 | Bandwidth     | Socket Number |\n");
	ebdr_log(INFO, "+----------------------------------------------------------------+\n");

	for(i = 0; i < pc_count; i++)
	{
		if(all_partner_clients[i].obj_state == PARTNER_OBJ_IN_USE)
		{
			ebdr_log(INFO, "|\t%d\t| %s\t | %lu\t\t |  %d\t\t |\n", all_partner_clients[i].id,
					all_partner_clients[i].ip,  all_partner_clients[i].bandwidth,
					all_partner_clients[i].socket_fd);
		}
	}
	ebdr_log(INFO, "+----------------------------------------------------------------+\n");

}

int is_client_partner_id_valid(int pid)
{
	int i;
	for(i = 0; i < MAX_PARTNERS; i++)
	{
		if(all_partner_clients[i].id == pid)
		{
			ebdr_log(INFO, "[is_client_pid_valid] client partner id [%d] is valid\n", pid);
			return 1;
		}
	}
	return 0;
}

int make_partner_client(char *ip, unsigned long int bandwidth, int sfd, int pid)
{

	all_partner_clients[pid].id = pid;
	all_partner_clients[pid].obj_state = PARTNER_OBJ_IN_USE;
	strcpy(all_partner_clients[pid].ip, ip);
	all_partner_clients[pid].bandwidth = bandwidth;
	all_partner_clients[pid].socket_fd = sfd;
	pc_count++;
	insert_into_partner(pid, PARTNER_OBJ_IN_USE, ip, bandwidth, "ebdrdbc");
	ebdr_log(INFO, "[make_partner_client] partnership id=%d ip=%s bandwidth=%lu\n", all_partner_clients[pid].id, 
			all_partner_clients[pid].ip, all_partner_clients[pid].bandwidth);
	return pid;
}

int mkpartner_from_db_on_client()
{
	return retrieve_from_partner(all_partner_clients);
}

void partner_client_obj_destroy(int client_pid)
{
	pc_count--;
	ebdr_log(INFO, "[partner_client_destroy] releasing client_pid :%d\n", client_pid);
	ebdr_log(INFO, "[partner_client_destroy] closing client sockfd: [%d]\n", all_partner_clients[client_pid].socket_fd);
	close(all_partner_clients[client_pid].socket_fd);
	memset(&all_partner_clients[client_pid], '\0', sizeof(partner_t));
	all_partner_clients[client_pid].obj_state = PARTNER_OBJ_RELEASED;
}

partner_operations_t partner_client_ops =
{
	.partner_obj_create 	= partner_client_obj_create,
	.make_partner 			= make_partner_client,
	.list_partners 			= client_list_partners,
	.partner_obj_destroy 	= partner_client_obj_destroy
};
