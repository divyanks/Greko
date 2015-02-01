#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ebdr_ip.h"


void ebdr_ip_obj_server_init(char *ip, ip_mode type)
{
  int server_pid;

	strcpy(ebdr_ip_server_obj[server_pid].ip, ip);
	ebdr_ip_server_obj[server_pid].type = type;
	ebdr_ip_server_obj[server_pid].state = IP_OBJ_IN_USE;

	ebdr_ip_server_obj[server_pid].client = NULL;
	ebdr_ip_server_obj[server_pid].ops = &ip_server_ops;
}

void ebdr_ip_obj_server_setup(ebdr_ip_t *ip_obj, int pid)
{
	all_ips_server[pid].type = ip_obj->type;
	all_ips_server[pid].state = ip_obj->state;
	strcpy(all_ips_server[pid].ip, ip_obj->ip);
	printf("[ip_server_setup] ip addr = %s\n", all_ips_server[pid].ip);
}

void ebdr_ip_obj_server_unsetup(ebdr_ip_t *ip_obj, int pid)
{

}



struct ebdr_ip_operations ip_server_ops =
{
	.do_ebdr_ip_obj_init   	= ebdr_ip_obj_server_init,
	.do_ebdr_ip_obj_setup  	= ebdr_ip_obj_server_setup,
	.do_ebdr_ip_obj_unsetup	= ebdr_ip_obj_server_unsetup,
};


