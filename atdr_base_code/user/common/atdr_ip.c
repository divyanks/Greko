#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ebdr_ip.h"

void *ebdr_ip_obj_init(char *partner_ip, ip_mode type )
{
	if(type == NETLINK_SERVER)
	{
		ebdr_ip_obj.ops = &ip_server_ops;
	}
	else
	{
		ebdr_ip_obj.ops = &ip_client_ops;
	}

	ebdr_ip_obj.ops->do_ebdr_ip_obj_init(partner_ip, type);
}    

