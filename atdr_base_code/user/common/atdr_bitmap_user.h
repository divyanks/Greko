#ifndef __EBDR_BITMAP_H
#define __EBDR_BITMAP_H

#include <assert.h>
#include "ebdr_bitmap_server.h"
#include "ebdr_bitmap_client.h"
#include "resyncd.h"

#define BITMAP_SIZE 4*4096

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


enum ebdr_bitmap_obj_state_t
{
	BITMAP_OBJ_IN_USE,
	BITMAP_OBJ_PAUSE,
	BITMAP_OBJ_RESUME,
	BITMAP_OBJ_RELEASED
};


/*
 * EBDR_UBITMAP_INIT
 * EBDR_UBITMAP_UPDATING  -> Server Side Only: Swapping kernel and user 
 * EBDR_UBITMAP_READY     -> when bitmap is good to use for data copying 
 * EBDR_UBITMAP_COMPLETED -> when all bits are resynced 
 * EBDR_UBITMAP_FREE      -> free after resync is completed 
 *
*/

enum ebdr_bitmap_state
{
	EBDR_UBITMAP_INIT,
	EBDR_UBITMAP_UPDATING,
	EBDR_UBITMAP_READY,    
	EBDR_UBITMAP_COMPLETED, 
	EBDR_UBITMAP_FREE      
};

struct ebdr_conn 
{
	int conn_fd;
	int state;
};

struct ebdr_user_bitmap;

/* ebdr_progress:  These all are required for start and resume after indefinite 
 *		   period of time.
 * ebdr_replication_ver: the repliation s/w version associated with this bitmap 
 * store_fd: checkpoint resync 
 * last_resynced_bit: Bit which is resynced recently
 * last_activity_time: Time at which last bit resynced
 **/
struct ebdr_progress
{
	int ebdr_replication_ver;
	int store_fd;
	int total_bits_set;
	int total_bits_resynced;
	int last_resynced_bit;
	int last_activity_time;
};



/* ebdr_bitmap_operations  
 * do_ebdr_bitmap_setup : 
 * do_ebdr_bitmap_cleanup:
 *      Setup and cleanup of ebdr_user_bitmap object on server/client
 *      on server: mmap the area and switch the buffer area between kernel and user
 *      on client: init connection and populate the infrastructure so that receive
		   bitmap can be initiated from the client
 *  load_ebdr_bitmap_from_disk:
 *  store_ebdr_bitmap_to_disk:
 *	Load & store bitmap from/to disk provided with checkpointing index
 *  recv_ebdr_bitmap_from_conn
 *  send_ebdr_bitmap_to_conn :
 *       Send and recevie bitmap from server/client with given version number
 *  get_next_bit_set :    
 *  get_total_bits_set :
 *  is_ebdr_bitmap_valid: check whether the bitmap area is good for resync use
 */
struct ebdr_bitmap_operations
{
	void  (*ebdr_bitmap_create)(int pid);
	int  (*ebdr_bitmap_setup)(struct ebdr_user_bitmap *ubitmap, int pid);
	void (*ebdr_bitmap_destroy)(struct ebdr_user_bitmap *ubitmap, int pid);

	/* Load & Store from Local Disk */
	int (*load_bitmap_from_disk)(struct ebdr_user_bitmap *ubitmap, int checkpoint_index);
	int (*store_bitmap_to_disk)(struct ebdr_user_bitmap *ubitmap, int checkpoint_index);
	
	int (*load_bitmap_from_memory)(struct ebdr_user_bitmap *ubitmap, int pid);

	/* Load & Store from Remote Server */
	int (*recv_bitmap_from_conn)(struct ebdr_user_bitmap *ubitmap, int version, int sockfd);
	int (*send_bitmap_to_conn)(struct ebdr_user_bitmap *ubitmap, int version, int sockfd);

	/* Operations for use locally */
	int (*get_next_bit_set)(struct ebdr_user_bitmap *ubitmap);
	int (*get_total_bits_set)(unsigned long int *ubitmap);
	int (*is_ebdr_bitmap_valid)(struct ebdr_user_bitmap *ubitmap);
	void (*show_grain_resync_progress)(struct ebdr_user_bitmap *ubitmap, int last_resynced_bit_pos);
};

extern struct ebdr_bitmap_operations bitmap_server_operations;
extern struct ebdr_bitmap_operations bitmap_client_operations;

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

struct ebdr_user_bitmap
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

	enum ebdr_bitmap_obj_state_t obj_state;
	struct ebdr_bitmap_operations *ops;
	struct ebdr_conn *conn;
	struct ebdr_progress uptill;
};

struct ebdr_user_bitmap bitmap_obj;
struct ebdr_user_bitmap bitmap_server_obj[MAX_BITMAP];
struct ebdr_user_bitmap bitmap_client_obj[MAX_BITMAP];

#endif
