#include "main.h"
int main()
{
	long result;
	HANDLE hDevice = NULL;
	BOOL bResult = FALSE;
	DWORD bytesReturned, i;
	IOCTL ioctl;
	wchar_t ptr[8] = L"\\\\.\\c:";
	ptr[4] = L'c';
	char *str;
	char *str2;
	ioctl.u_bitmap_size = 16384;
	ioctl.u_grain_size = 16384;
	ioctl.activebitmap = &str;
	
	POUTPUT Output = (POUTPUT)malloc(sizeof(OUTPUT));
			
	printf("before create file\n");
		hDevice = CreateFile(ptr,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_NO_BUFFERING,
			NULL);
		printf("after create file\n");

		if (hDevice == INVALID_HANDLE_VALUE) {

			result = GetLastError();

			printf("ERROR opening device...%x\n", result);
			exit(0);
		}
		printf("before DeviceIoControl \n");

		bResult = DeviceIoControl(hDevice,
			EBDRCTL_DEV_CREATE,
			&ioctl,
			sizeof(IOCTL),
			Output,
			sizeof(OUTPUT),
			&bytesReturned,
			NULL);
		printf("after DeviceIoControl \n");
		// Perform Write on the Opened Drive "Myworld" "yourworld"
	

	Sleep(6000*100);

	for (i = 0; i < 2; i++)
	{
		printf("%s \n%s ", str, str2);

	}
	

}
