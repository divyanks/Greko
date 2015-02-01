#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "ebdr_io_engine.h"
#include "ebdr_replication_header.h"
#include "ebdr_replication.h"
#include "ebdr_log.h"

//static int recv_count = 20;
static void io_client_obj_create(int client_pid)
{
	if(client_pid < MAX_IO)
	{
		io_client_obj[client_pid].obj_state = IO_OBJ_IN_USE;
		io_client_obj[client_pid].ops = &io_client_ops;
		ebdr_log(INFO, "[io_client_obj_create] client_pid = %d\n", client_pid);
	}
	else
	{
		ebdr_log(ERROR, "MAX IO client objects reached! \n");
		stop_work("[io_client_obj_create] max io client objects reached ");
	}
}

static int chunk_read_client(int fd, char *buff, unsigned long int size, unsigned long int start_lba)
{
	int n;
	ebdr_log(INFO, "[client_read] reading from sock fd:%d\n", fd);
#if 0
   	recv_count--;
	if(recv_count == 0)
	{
		return -1;
	}
#endif

	n = recv(fd, buff, size, MSG_WAITALL);
	if(n < 0)
	{
		ebdr_log(FATAL, "[client] recv error....returning -1 \n");
		return -1;
	}

 	return 0;
}

static int chunk_write_client(int fd, char *buff, unsigned long int size, unsigned long int start_lba)
{
	int ret = 0, pret = 0;
	off64_t loffset,x;
	
	loffset = (off64_t)(start_lba * CHUNK_SIZE);

	x = lseek(fd, loffset, SEEK_SET);
	if(x < 0)
	{
		ebdr_log(FATAL, "[chunk_write_client]lseek error [%d]\n", errno);
		//stop_work("[chuk_write_client] lseek error ");
		return x;
	}
	
	ret = write(fd, buff, size);
	if(ret < 0)
	{
		ebdr_log(FATAL, "[chunk_write_client] write error %d\n", errno);
		//stop_work("[chunk_write_client] write error ");
		return ret;
	}
	
	pret = posix_fadvise(fd, loffset, size, POSIX_FADV_DONTNEED);
	
	ebdr_log(INFO, "[client_write] written %d bytes offset:%lld \n", ret, x);
	return 0;
}

static void io_client_obj_destroy(struct ebdr_io_engine *io_eng_obj)
{
	ebdr_log(INFO, "[client_obj_destroy] setting obj_state to IO_OBJ_RELEASED \n");
	io_eng_obj->obj_state = IO_OBJ_RELEASED;
}


struct ebdr_io_engine_ops io_client_ops =
{
	.io_obj_create 		= io_client_obj_create,
	.chunk_read      	= chunk_read_client,
	.chunk_write     	= chunk_write_client,
	.io_obj_destroy 	= io_client_obj_destroy
};

