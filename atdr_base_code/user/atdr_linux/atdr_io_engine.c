#include "sys_common.h"
#include "ebdr_io_engine.h"

/*
 * This function will do setup of I/O engine
 *
 */

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
