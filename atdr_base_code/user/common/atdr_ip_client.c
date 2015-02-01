#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ebdr_ip.h"

void ebdr_ip_obj_client_init(char *ip, ip_mode type)
{
  int client_pid;

	strcpy(ebdr_ip_client_obj[client_pid].ip, ip);
	ebdr_ip_client_obj[client_pid].type = type;
	ebdr_ip_client_obj[client_pid].state = IP_OBJ_IN_USE;

	ebdr_ip_client_obj[client_pid].client = NULL;
	ebdr_ip_client_obj[client_pid].ops = &ip_client_ops;
}

void ebdr_ip_obj_client_setup(ebdr_ip_t *ip_obj, int client_pid)
{   
	all_ips_client[client_pid].type = ip_obj->type;
	all_ips_client[client_pid].state = ip_obj->state;
	strcpy(all_ips_client[client_pid].ip, ip_obj->ip);

 	printf("[ip_client_setup] ip addr = %s\n", all_ips_client[client_pid].ip);
}

void ebdr_ip_obj_client_unsetup(ebdr_ip_t *ip_obj, int client_pid)
{

}


struct ebdr_ip_operations ip_client_ops =
{
	.do_ebdr_ip_obj_init  	= ebdr_ip_obj_client_init,
	.do_ebdr_ip_obj_setup  	= ebdr_ip_obj_client_setup,
	.do_ebdr_ip_obj_unsetup	= ebdr_ip_obj_client_unsetup
};


