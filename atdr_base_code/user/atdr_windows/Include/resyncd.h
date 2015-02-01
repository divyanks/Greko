#ifndef _RESYNC_H
#define _RESYNC_H
#include <stdio.h>

#define MAX_DISKS 	2048

#define	EBDR_BYTES_TO_SECTOR(val)		(val/512)	
#define TRUE  1
#define FALSE 0
#define EBDR_TRACE_LOG_SIZE			4*4096
#define	PAGE_SHIFT				12
#define	PAGE_SIZE				(1UL << PAGE_SHIFT)
#define NUM_BITS_PER_PAGE			(1UL << (PAGE_SHIFT + 3))
#define NUM_BITS_PER_PAGE_MASK			(NUM_BITS_PER_PAGE - 1)
#define BITS_PER_LONG				32

#if BITS_PER_LONG               ==      32
#define NUM_BPL                         5
#elif BITS_PER_LONG             ==      64
#define NUM_BPL                         6
#endif

#define NUM_WORDS_PER_PAGE              	(PAGE_SIZE  >> (NUM_BPL - 3))

#define MAX_BITS_LEN				4*4096*8
#define	DEVICE_SIZE				(unsigned long) 8*1024*1024
#define	GRAIN_SIZE				(DEVICE_SIZE/MAX_BITS_LEN) // 8MB /32 = 256K
#define	GRAIN_IN_SECTORS			512 // 256K/512 = 512
#define SECTOR_SIZE				512
#define BUFF_SIZE				512 
#define START					0
#define STOP					1

#define TOTAL_BITS_IN_BITMAP 		   8 * sizeof(dirtied_bmap)

//extern int write_sector(const char *device_name, unsigned char *buff, unsigned long lba, unsigned long size);
//extern int read_sector(const char *device_name, unsigned char *r_buff, unsigned long lba, unsigned long size);

int resync;

int memory_fd[MAX_DISKS];
long unsigned int *active_bitmap[MAX_DISKS];

void *resyncd(void *args);

char *wbuff, *rbuff;

#endif
