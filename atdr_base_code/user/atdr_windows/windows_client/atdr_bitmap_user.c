/* User Space ATDR Bitmap */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "..\Include\atdr_bitmap_user.h"

void atdr_user_bitmap_init(enum role_t role, int pid)
{
	if(role == LOCAL)
	{   
		bitmap_obj.ops = &bitmap_server_operations; 
	} else{
		bitmap_obj.ops = &bitmap_client_operations;
	}
	bitmap_obj.ops->atdr_bitmap_create(pid);
}
