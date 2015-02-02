#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "..\Include\atdr_conn.h"
#include "..\Include\atdr_log.h"

void atdr_connection_init(enum atdr_conn_mode_t conn_mode, int pid)
{
	if(conn_mode == ATDR_CONN_SERVER)
	{
		atdr_connect.conn_ops = &server_conn_ops;
	}
	else
	{
		atdr_connect.conn_ops = &client_conn_ops;
	}

	atdr_connect.conn_ops->do_atdr_conn_init(pid);
}
