#ifndef ATDR_REPLICATION_H
#define ATDR_REPLICATION_H

// #include "atdr_replication_header.h" santhosh
#include "atdr_conn.h"
#include "atdr_disk.h"
#include "atdr_log.h"
#include "atdr_replication_header.h"
#include "atdr_db.h"

#define BUF_SIZE_IN_BYTES 	4096
#define CHUNK_SIZE 			512
#define MAX_REPLICATIONS 	2048

enum rep_state_t
{
	RESYNC_IN_PROGRESS,
	RESYNC_PAUSE,
	RESYNC_RESUME,
	RESYNC_COMPLETED
};

enum rep_role_t
{
	REP_LOCAL,
	REP_REMOTE
};

struct atdr_replication;

struct atdr_replication_operations
{
	void (*replic_obj_create)(int pid);  
	void (*replic_obj_setup)(struct atdr_replication *replic_obj, int pid);  
	void (*replic_obj_destroy)(struct atdr_replication *replic_obj);  

};

extern struct atdr_replication_operations replic_server_operations;
extern struct atdr_replication_operations replic_client_operations;

struct atdr_replication
{
	enum rep_role_t rep_role;
	enum rep_state_t rep_state;
	unsigned long int last_resynced_bit;
	struct atdr_connection *rep_conn;
    replic_header *rep_hdr; 
	struct atdr_disk *rep_disk;
	struct atdr_replication_operations *ops;
};

struct atdr_replication replic_req;
struct atdr_replication replic_server_obj[MAX_REPLICATIONS];
struct atdr_replication replic_client_obj[MAX_REPLICATIONS];
int replication_init(enum rep_role_t rep_role, int pid);
int mkresync_from_db_on_client();
#endif
