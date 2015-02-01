#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "..\Include\atdr_conn.h"
#include "..\Include\atdr_log.h"

void ebdr_connection_init(enum ebdr_conn_mode_t conn_mode, int pid)
{
	if(conn_mode == EBDR_CONN_SERVER)
	{
		ebdr_connect.conn_ops = &server_conn_ops;
	}
	else
	{
		ebdr_connect.conn_ops = &client_conn_ops;
	}

	ebdr_connect.conn_ops->do_ebdr_conn_init(pid);
}
