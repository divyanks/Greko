#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "..\Include\atdr_io_engine.h"
#include "..\Include\atdr_replication.h"
#include "..\Include\atdr_replication_header.h"
#include "..\Include\atdr_log.h"

//static int recv_count = 20;
static void io_client_obj_create(int client_pid)
{
	if(client_pid < MAX_IO)
	{
		io_client_obj[client_pid].obj_state = IO_OBJ_IN_USE;
		io_client_obj[client_pid].ops = &io_client_ops;
		atdr_log(ATDR_INFO, "[io_client_obj_create] client_pid = %d\n", client_pid);
	}
	else
	{
		atdr_log(ATDR_ERROR, "MAX IO client objects reached! \n");
		stop_work("[io_client_obj_create] max io client objects reached ");
	}
}

static int chunk_read_client(handle_fd_t fd, char *buff, unsigned long int size, unsigned long int start_lba)
{
	int n;
	atdr_log(ATDR_INFO, "[client_read] reading from sock fd:%d\n", fd);
#if 0
   	recv_count--;
	if(recv_count == 0)
	{
		return -1;
	}
#endif
	
    n = recv(fd.fd, buff, size, 0);  
	if ((n < 0) || (errno == ETIMEDOUT))
	{
		atdr_log(ATDR_FATAL, "[client] recv error %d....returning -1 \n", errno);
		return -1;
	}
	atdr_log(ATDR_INFO, "[client] recvd %d bytes\n", n);

 	return 0;
}

static int chunk_write_client(handle_fd_t hndle, char *buff, unsigned long int size, unsigned long int start_lba)
{
	int ret = 0, pret = 0;
	__int64 loffset; 
	
	loffset = (__int64)(start_lba * CHUNK_SIZE); 

	LARGE_INTEGER li;
	li.QuadPart = loffset;
	li.LowPart = SetFilePointer(hndle.hndle, li.LowPart, &li.HighPart, FILE_BEGIN);

	if (li.LowPart == INVALID_SET_FILE_POINTER)
	{
		atdr_log(ATDR_FATAL, "[chunk_write_client]lseek error [%d]\n", GetLastError());
		atdr_log(ATDR_FATAL, "[chunk_write_client]lseek %llu\n", li.QuadPart);
		return -1;
	}
	

	
	DWORD dwBytesWritten;
	BOOL bErrorFlag;

	bErrorFlag = WriteFile(
		hndle.hndle,           // open file handle
		buff,      // start of data to write
		size,  // number of bytes to write
		&dwBytesWritten, // number of bytes that were written
		NULL);            // no overlapped structure

	if (FALSE == bErrorFlag)
	{
		atdr_log(ATDR_FATAL, "Unable to write to file. [%d]\n", GetLastError());
		exit(0);
	}
	

	return 0;
}

static void io_client_obj_destroy(struct atdr_io_engine *io_eng_obj)
{
	atdr_log(ATDR_INFO, "[client_obj_destroy] setting obj_state to IO_OBJ_RELEASED \n");
	io_eng_obj->obj_state = IO_OBJ_RELEASED;
}


struct atdr_io_engine_ops io_client_ops =
{
	.io_obj_create 		= io_client_obj_create,
	.chunk_read      	= chunk_read_client,
	.chunk_write     	= chunk_write_client,
	.io_obj_destroy 	= io_client_obj_destroy
};

