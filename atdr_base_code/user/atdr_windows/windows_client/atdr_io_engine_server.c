#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

#include "..\Include\atdr_log.h"
#include "..\Include\atdr_io_engine.h"
#include "..\Include\atdr_replication.h"
#include "..\Include\atdr_replication_header.h"
#include "..\Include\atdr_disk.h"

static void io_server_obj_create(int server_pid)
{
	server_pid = server_pid; /* Temporary...will change later  */
	if(server_pid < MAX_IO)
	{
		io_server_obj[server_pid].obj_state = IO_OBJ_IN_USE;
		io_server_obj[server_pid].ops = &io_server_ops;
		ebdr_log(EBDR_INFO, "[io_server_obj_create] server_pid = %d\n", server_pid);
	}
	else
	{
		ebdr_log(EBDR_INFO, "Max IO Objects reached !\n");
		stop_work("[io_server_obj_create] man io objects server reached ");
	}
}

static int chunk_read_server(handle_fd_t hndle, char *buff, unsigned long int size, unsigned long int start_lba)
{
	int ret = 0;
	__int64 loffset;

	loffset = (__int64)(start_lba * CHUNK_SIZE);

	LARGE_INTEGER li;
	li.QuadPart = loffset;
	li.LowPart = SetFilePointer(hndle.hndle, li.LowPart, &li.HighPart, FILE_BEGIN);

	if (li.LowPart == INVALID_SET_FILE_POINTER)
	{
		ebdr_log(EBDR_FATAL, "[chunk_write_client]lseek error [%d]\n", GetLastError());
		ebdr_log(EBDR_FATAL, "[chunk_write_client]lseek %llu\n", li.QuadPart);
		return -1;
	}
	DWORD bytesRead;

    ret = ReadFile(hndle.hndle, buff, size, &bytesRead, NULL); 
	if(ret <= 0 || bytesRead == 0)
	{
		ebdr_log(EBDR_INFO, "[chunk_read_server] read error %d\n", errno);
		//stop_work("[chunk_read_server] read error ");
		return ret;
	}
	
	ebdr_log(EBDR_INFO, "[server_read] read %d bytes offset:%llu \n", bytesRead,li.LowPart);

	return 0;
}

static int chunk_write_server(handle_fd_t fd, char *buff, unsigned long int size, unsigned long int start_lba)
{
	ebdr_log(EBDR_INFO, "[chunk_write_server] writing to socket fd:%d\n", fd);
	if(send(fd.fd, buff, size, 0) <= 0) 
	{
		ebdr_log(EBDR_INFO, "server Send error\n");
		return -1;
	}

 	return 0;
}

static void io_server_obj_destroy(struct ebdr_io_engine *io_eng_obj)
{
	io_eng_obj->obj_state = IO_OBJ_RELEASED;
}

struct ebdr_io_engine_ops io_server_ops =
{
	.io_obj_create 		= io_server_obj_create,
	.chunk_read      	= chunk_read_server,
	.chunk_write     	= chunk_write_server,
	.io_obj_destroy 	= io_server_obj_destroy
};

