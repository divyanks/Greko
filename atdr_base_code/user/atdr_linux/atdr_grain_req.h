#ifndef _EBDR_GRAIN_IO_H
#define _EBDR_GRAIN_IO_H

#include "ebdr_io_engine.h"

#define EBDR_BYTES_TO_SECTOR(val)       (val/512) 
#define MAX_BITS_LEN                4*4096*8
#define DEVICE_SIZE             (unsigned long) 8*1024*1024
#define GRAIN_SIZE              (DEVICE_SIZE/MAX_BITS_LEN) 
//CHANGE
#define CHUNK_SIZE              512
#define CHUNK_SIZE_IN_SECTORS   CHUNK_SIZE/SECTOR_SIZE
#define GRAIN_IN_SECTORS                   512 // 256K/512 = 512
#define SECTOR_SIZE                        512
#define START                  0
#define STOP                   1

//extern int write_sector(const char *device_name, unsigned char *buff, unsigned long lba, unsigned long size);
//extern int read_sector(const char *device_name, unsigned char *buff, unsigned long lba, unsigned long size);

extern int resync;
extern char *rbuff;

struct ebdr_grain_req;

struct ebdr_grain_req_operations
{
    void (*completed_chunk)(struct ebdr_grain_req *req, int total_number_of_times);
    void (*completed_grain)(struct ebdr_grain_req *req);
};

extern struct ebdr_grain_req_operations grain_operations;

struct ebdr_grain_req
{
	int io_size;   
	int cur_bitmap_pos;
	unsigned long long lba;
        int next_chunk;
        //int total_dirtied_bits;
	ebdr_io_engine_t *io_engine_req;

	int state;
	void *io_req_private;
   
	//int (* grain_copy)( struct ebdr_grain_req *req);
	void (* grain_completed)( struct ebdr_grain_req *);
	struct ebdr_grain_req_operations *ops;

};
typedef struct ebdr_grain_req ebdr_grain_req_t;



#endif
