#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "ebdr_io_engine.h"
#include "ebdr_replication_header.h"
#include "ebdr_replication.h"
#include "ebdr_disk.h"
#include "ebdr_log.h"
static void io_server_obj_create(int server_pid)
{
	server_pid = server_pid; /* Temporary...will change later  */
	if(server_pid < MAX_IO)
	{
		io_server_obj[server_pid].obj_state = IO_OBJ_IN_USE;
		io_server_obj[server_pid].ops = &io_server_ops;
		ebdr_log(INFO, "[io_server_obj_create] server_pid = %d\n", server_pid);
	}
	else
	{
		ebdr_log(INFO, "Max IO Objects reached !\n");
		stop_work("[io_server_obj_create] man io objects server reached ");
	}
}

static int chunk_read_server(int fd, char *buff, unsigned long int size, unsigned long int start_lba)
{
	int ret = 0;
	off64_t loffset, x ;

	loffset = (off64_t)(start_lba * CHUNK_SIZE);

	x = lseek(fd, loffset, SEEK_SET);
	if(x < 0)
	{
		ebdr_log(INFO, "[chunk_read_server] lseek error %d\n", errno);
		//stop_work("[chunk_read_server] lseek error ");
		return x;
	}
	
	ret = read(fd, buff, size);
	if(ret < 0)
	{
		ebdr_log(INFO, "[chunk_read_server] read error %d\n", errno);
		//stop_work("[chunk_read_server] read error ");
		return ret;
	}
	
	ebdr_log(INFO, "[server_read] read %d bytes offset:%lld \n", ret, x);

	return 0;
}

static int chunk_write_server(int fd, char *buff, unsigned long int size, unsigned long int start_lba)
{
	ebdr_log(INFO, "[chunk_write_server] writing to socket fd:%d\n", fd);
	if(send(fd, buff, size, 0) < 0)
	{
		ebdr_log(INFO, "server Send error\n");
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

