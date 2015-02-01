#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "ebdr_log.h"
#include "ebdr_partner.h"

int ps_count;
int pc_count;

void *ebdr_partner_init(enum partner_mode_t partner_mode,struct partner *partner_obj)
{

	if(partner_mode == PARTNER_SERVER)
	{   
		partner_obj->ops = &partner_server_ops;
	}   
	else
	{   
		partner_obj->ops = &partner_client_ops;
	}   

	partner_obj->ops->partner_obj_create(partner_obj);
}
