#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <winioctl.h>
#include <string.h>

typedef struct user_ioctl
{
	UINT32 u_grain_size;
	UINT32 u_bitmap_size;
	UINT32 bmap_user;
	UINT32 bmap_ver;
	UINT32 index;
	LARGE_INTEGER u_device_size; //Fire ioctl to the underlying device to find out the device size before firing in CREATE ioctl
	LARGE_INTEGER total_num_bits; //Divide the LARGE_INTEGER u_device_size/u_bitmap_size
	void **activebitmap;
}IOCTL;

typedef struct IOCTL_INPUT
{
	UINT32  len;
} RG_INPUT, *RG_PINPUT;

typedef struct IOCTL_OUTPUT
{
	PVOID MappedUserAddress;
} OUTPUT, *POUTPUT;

#define EBDR_DRIVER_NAME      L"EBDRVOLFLT.SYS"
#define EBDR_DEVICE_NAME      L"EBDRVOLFLT"
#define EBDR_W32_DEVICE_NAME  L"\\\\.\\ebdrvolflt"
#define EBDR_DOSDEVICE_NAME   L"\\DosDevices\\ebdrvolflt"
#define EBDRDEVICECONTROLTYPE 0x9000
#define EBDRCTL_DEV_CREATE              CTL_CODE(EBDRDEVICECONTROLTYPE, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define EBDRCTL_CHECK_BITMAP            CTL_CODE(EBDRDEVICECONTROLTYPE, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define EBDRCTRL_DESTROY                CTL_CODE(EBDRDEVICECONTROLTYPE, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)