#ifndef EBDR_REPLICATION_H
#define EBDR_REPLICATION_H

#include "ebdr_replication_header.h"
#include "ebdr_conn.h"
#include "ebdr_disk.h"

#define BUF_SIZE_IN_BYTES 	4096
#define CHUNK_SIZE 			512
#define MAX_REPLICATIONS 	2048

enum rep_role_t
{
	REP_LOCAL,
	REP_REMOTE
};

struct ebdr_replication;

struct ebdr_replication_operations
{
	void (*replic_obj_create)(int pid);  
	void (*replic_obj_setup)(struct ebdr_replication *replic_obj, int pid);  
	void (*replic_obj_destroy)(struct ebdr_replication *replic_obj);  

};

extern struct ebdr_replication_operations replic_server_operations;
extern struct ebdr_replication_operations replic_client_operations;

struct ebdr_replication
{
	enum rep_role_t rep_role;
	unsigned long int last_resynced_bit;
	struct ebdr_connection *rep_conn;
	replic_header *rep_hdr;
	struct ebdr_disk *rep_disk;
	struct ebdr_replication_operations *ops;
};

struct ebdr_replication replic_req;
struct ebdr_replication replic_server_obj[MAX_REPLICATIONS];
struct ebdr_replication replic_client_obj[MAX_REPLICATIONS];

#endif
