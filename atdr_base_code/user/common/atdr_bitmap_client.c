#include "ebdr_sys_common.h"
#include "ebdr_bitmap_user.h"
#include "ebdr_log.h"

static void bitmap_client_create(int client_pid)
{
	bitmap_client_obj[client_pid].bitmap_area = malloc(BITMAP_SIZE);
	if (!bitmap_client_obj[client_pid].bitmap_area)
	{
		ebdr_log(FATAL, "error in allocating memory");
		stop_work("bitmap malloc failed");
	}
	memset(bitmap_client_obj[client_pid].bitmap_area, '\0', (BITMAP_SIZE));
	bitmap_client_obj[client_pid].bitmap_size = BITMAP_SIZE;
	bitmap_client_obj[client_pid].bitmap_state = EBDR_UBITMAP_INIT;
	bitmap_client_obj[client_pid].ops = &bitmap_client_operations;

}

static int bitmap_client_setup(struct ebdr_user_bitmap *ubitmap_obj, int client_pid)
{
	bitmap_client_obj[client_pid].obj_state = BITMAP_OBJ_IN_USE; 
	ebdr_log(INFO, "[bitmap_client_setup] client_pid = %d\n", client_pid);
 	return 0;
}


static void bitmap_client_destroy(struct ebdr_user_bitmap *ubitmap_obj, int pid)
{
	ebdr_log(INFO, "[bitmap_client_destroy] freeing bitmap_area... \n");
	free(ubitmap_obj->bitmap_area);
}

struct ebdr_bitmap_operations bitmap_client_operations =
{
	.ebdr_bitmap_create 	= bitmap_client_create,
	.ebdr_bitmap_setup 		= bitmap_client_setup,
	.ebdr_bitmap_destroy 	= bitmap_client_destroy,
};
