#ifndef __ATDR_BITMAP_H
#define __ATDR_BITMAP_H

#include <assert.h>

#include "atdr_bitmap_server.h"
#include "atdr_bitmap_client.h"
#include "resyncd.h"

#define BITMAP_SIZE 4*4096
#define BITMAP_FILE "c:\\bitmlap"
#define FILENAME_LEN 128

#define MAX_BITMAP 2048

typedef unsigned long ulong_t;

enum mmap_t
{
	MMAP_UNUSED,
	MMAP_USED
};

enum role_t
{
	LOCAL,
	REMOTE
};


enum atdr_bitmap_obj_state_t
{
	BITMAP_OBJ_IN_USE,
	BITMAP_OBJ_PAUSE,
	BITMAP_OBJ_RESUME,
	BITMAP_OBJ_RELEASED
};


/*
 * ATDR_UBITMAP_INIT
 * ATDR_UBITMAP_UPDATING  -> Server Side Only: Swapping kernel and user 
 * ATDR_UBITMAP_READY     -> when bitmap is good to use for data copying 
 * ATDR_UBITMAP_COMPLETED -> when all bits are resynced 
 * ATDR_UBITMAP_FREE      -> free after resync is completed 
 *
*/

enum atdr_bitmap_state
{
	ATDR_UBITMAP_INIT,
	ATDR_UBITMAP_UPDATING,
	ATDR_UBITMAP_READY,    
	ATDR_UBITMAP_COMPLETED, 
	ATDR_UBITMAP_FREE      
};

struct atdr_conn 
{
	int conn_fd;
	int state;
};

struct atdr_user_bitmap;

/* atdr_progress:  These all are required for start and resume after indefinite 
 *		   period of time.
 * atdr_replication_ver: the repliation s/w version associated with this bitmap 
 * store_fd: checkpoint resync 
 * last_resynced_bit: Bit which is resynced recently
 * last_activity_time: Time at which last bit resynced
 **/
struct atdr_progress
{
	int atdr_replication_ver;
	int store_fd;
	int total_bits_set;
	int total_bits_resynced;
	int last_resynced_bit;
	int last_activity_time;
};



/* atdr_bitmap_operations  
 * do_atdr_bitmap_setup : 
 * do_atdr_bitmap_cleanup:
 *      Setup and cleanup of atdr_user_bitmap object on server/client
 *      on server: mmap the area and switch the buffer area between kernel and user
 *      on client: init connection and populate the infrastructure so that receive
		   bitmap can be initiated from the client
 *  load_atdr_bitmap_from_disk:
 *  store_atdr_bitmap_to_disk:
 *	Load & store bitmap from/to disk provided with checkpointing index
 *  recv_atdr_bitmap_from_conn
 *  send_atdr_bitmap_to_conn :
 *       Send and recevie bitmap from server/client with given version number
 *  get_next_bit_set :    
 *  get_total_bits_set :
 *  is_atdr_bitmap_valid: check whether the bitmap area is good for resync use
 */
struct atdr_bitmap_operations
{
	void  (*atdr_bitmap_create)(int pid);
	int  (*atdr_bitmap_setup)(struct atdr_user_bitmap *ubitmap, int pid);
	void (*atdr_bitmap_destroy)(struct atdr_user_bitmap *ubitmap, int pid);

	/* Load & Store from Local Disk */
	int (*load_bitmap_from_disk)(struct atdr_user_bitmap *ubitmap, int checkpoint_index);
	int (*store_bitmap_to_disk)(struct atdr_user_bitmap *ubitmap, int checkpoint_index);
	
	int (*load_bitmap_from_memory)(struct atdr_user_bitmap *ubitmap, int pid);

	/* Load & Store from Remote Server */
	int (*recv_bitmap_from_conn)(struct atdr_user_bitmap *ubitmap, int version, int sockfd);
	int (*send_bitmap_to_conn)(struct atdr_user_bitmap *ubitmap, int version, int sockfd);

	/* Operations for use locally */
	int (*get_next_bit_set)(struct atdr_user_bitmap *ubitmap);
	int (*get_total_bits_set)(unsigned long int *ubitmap);
	int (*is_atdr_bitmap_valid)(struct atdr_user_bitmap *ubitmap);
	void (*show_grain_resync_progress)(struct atdr_user_bitmap *ubitmap, int last_resynced_bit_pos);
};

extern struct atdr_bitmap_operations bitmap_server_operations;
extern struct atdr_bitmap_operations bitmap_client_operations;

/*
 * resync_bitmap: bitmap used for tracking the data changed and syncing
 *		  selective blocks to the target(local/remote)
 * bitmap_size:
 * bitmap_version: The version of snapshot disk which is in use for tracking
 * owner: Local/Remote
 * bitmap_state: 
 * ops: allows us to define different types of bitmaps eg. client/server
 * conn:
 * uptill:
*/

struct atdr_user_bitmap
{
	unsigned long int *bitmap_area;
	unsigned long int bitmap_size;
	unsigned long int grain_size;
	unsigned long int chunk_size;
	int bitmap_version;

	enum role_t owner;
	enum mmap_t mmap_state;
	int bitmap_state;
	int bmap_user;

	enum atdr_bitmap_obj_state_t obj_state;
	struct atdr_bitmap_operations *ops;
	struct atdr_conn *conn;
	struct atdr_progress uptill;
};

struct atdr_user_bitmap bitmap_obj;
struct atdr_user_bitmap bitmap_server_obj[MAX_BITMAP];
struct atdr_user_bitmap bitmap_client_obj[MAX_BITMAP];
void atdr_user_bitmap_init(enum role_t role, int pid);
int is_bit_set(unsigned long *bitmap_area, int pos);

#endif
