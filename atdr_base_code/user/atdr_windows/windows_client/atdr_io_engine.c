#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\Include\atdr_io_engine.h"


void io_engine_init(enum owner_t owner, int pid)
{
	if(owner == IO_SERVER)
	{
		io_obj.ops = &io_server_ops;
	}
	else
	{
		io_obj.ops = &io_client_ops;
	}

	io_obj.ops->io_obj_create(pid);
}
