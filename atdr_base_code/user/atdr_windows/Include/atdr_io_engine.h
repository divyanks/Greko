#ifndef _EBDR_IO_ENGINE_H
#define _EBDR_IO_ENGINE_H

#include "atdr_replication_header.h"
#include "atdr_replication.h"

#define MAX_IO 2048

enum io_state_t
{
	IO_OBJ_FREE,
	IO_OBJ_IN_USE,
	IO_OBJ_RELEASED
};

enum owner_t
{
	IO_SERVER,
	IO_CLIENT
};

struct ebdr_io_engine;
/*  ebdr_engine_ops:
 *  io_engine_setup: setup function of io_engine
 *  do_chunk_read:
 *  do_chunk_write:
 *			Do chunk read/write on disk/connection
 *  chunk_completed:
 *			completion function of io_engine
 */
typedef struct handle_fd
{
	HANDLE hndle;
	SOCKET fd;
}handle_fd_t;

struct ebdr_io_engine_ops
{
	void (*io_obj_create)(int pid);
	int (*chunk_read)(handle_fd_t fd, char *buff, unsigned long int size, unsigned long int start_lba);
	int (*chunk_write)(handle_fd_t fd, char *buff, unsigned long int size, unsigned long int start_lba);
	void (*io_obj_destroy)(struct ebdr_io_engine *io_eng_obj);
};


extern struct ebdr_io_engine_ops io_server_ops;
extern struct ebdr_io_engine_ops file_ops;
extern struct ebdr_io_engine_ops io_client_ops;
/*
 *  ebdr_io_engine: This structure is used by the I/O Engine which will finally
 *					do SCSI read/write
 *	src_disk: Source disk fd
 *  target_disk: Target disk fd
 *	buff: buffer to hold data read/written to disk/connection
 *  lba: starting lba to be passed to the scsi read/write 
 *		lba = grain_size * grain_pos + chunk_size * chunk_pos
 *  owner: LOCAL / RELAY
 *  ops: ebdr_engine operations 
 */

struct ebdr_io_engine
{
	int src_disk_fd;
	int target_disk_fd;
	char buff[BUF_SIZE_IN_BYTES];
	int chunk_pos;
	unsigned long long chunk_lba;
	enum owner_t owner;
	enum io_state_t obj_state;

	struct ebdr_io_engine_ops *ops;
};

typedef struct ebdr_io_engine ebdr_io_engine_t;

struct ebdr_io_engine io_obj;
struct ebdr_io_engine io_server_obj[MAX_IO];
struct ebdr_io_engine io_client_obj[MAX_IO];
void io_engine_init(enum owner_t owner, int pid);

#endif
