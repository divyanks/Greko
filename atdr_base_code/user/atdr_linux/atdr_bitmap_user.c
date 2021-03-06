/* User Space eBDR Bitmap */
#include "sys_common.h"
#include "ebdr_bitmap_user.h"

void ebdr_user_bitmap_init(enum role_t role, int pid)
{
	if(role == LOCAL)
	{   
		bitmap_obj.ops = &bitmap_server_operations;
	} else{
		bitmap_obj.ops = &bitmap_client_operations;
	}
	bitmap_obj.ops->ebdr_bitmap_create(pid);
}
