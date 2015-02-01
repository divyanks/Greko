#pragma once
#include "ntddk.h"
#include <mountdev.h>

typedef unsigned long long sector_t;
typedef unsigned long long ulong_t;


typedef struct IOCTL_INPUT
{
	UINT32  len;
}INPUT, *PINPUT;

typedef struct IOCTL_OUTPUT_CREATE
{

	PVOID MappedUserAddress;
	
}OUTPUT_CREATE, *POUTPUT_CREATE;


typedef struct IOCTL_OUTPUT_USER
{

	UINT8 bmap_user;

}OUTPUT_USER, *POUTPUT_USER;


typedef struct user_ioctl
{
	CHAR src_disk[64];
	UINT32 u_grain_size;
	UINT32 u_bitmap_size;
	UINT32 bmap_user;
	UINT32 bmap_ver;
	UINT32 index;	
} IOCTL;


typedef enum user
{
	KERNEL,
	USER	
}BMAP_USER;

typedef struct _EBDR_BITMAP
{
	char *bitmap_area;
	char *bitmap_area_start;
	char *bitmap_area_backup;

	UINT32 bitmap_size; //This is number of  bytes used for bitmap_size
	UINT32 grain_size; //This is in bytes
	
	UINT32 total_num_bits; //This is device_size/grain_size
	KSPIN_LOCK Lock;
	KSPIN_LOCK Lock_bm;
	BMAP_USER bmap_user;
	
	PVOID  MappedUserAddress;
	PVOID  MappedUserAddressBackup;
	PMDL   Mdl;
}EBDR_BITMAP ,*PEBDR_BITMAP;

UINT32 ebdr_find_pos(EBDR_BITMAP *bitmap, sector_t addr);
void ebdr_set_bits(PEBDR_BITMAP bitmap, int first, int last);

