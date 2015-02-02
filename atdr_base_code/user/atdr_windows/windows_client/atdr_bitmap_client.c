#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "..\Include\atdr_bitmap_user.h"
#include "..\Include\atdr_log.h"

static void bitmap_client_create(int client_pid)
{
	bitmap_client_obj[client_pid].bitmap_area = malloc(BITMAP_SIZE);
	if (!bitmap_client_obj[client_pid].bitmap_area)
	{
		atdr_log(ATDR_FATAL, "error in allocating memory");
		stop_work("bitmap malloc failed");
	}
	memset(bitmap_client_obj[client_pid].bitmap_area, '\0', (BITMAP_SIZE));
	bitmap_client_obj[client_pid].bitmap_size = BITMAP_SIZE;
	bitmap_client_obj[client_pid].bitmap_state = ATDR_UBITMAP_INIT;
	bitmap_client_obj[client_pid].ops = &bitmap_client_operations;

}

static int bitmap_client_setup(struct atdr_user_bitmap *ubitmap_obj, int client_pid)
{
	bitmap_client_obj[client_pid].obj_state = BITMAP_OBJ_IN_USE; 
	atdr_log(ATDR_INFO, "[bitmap_client_setup] client_pid = %d\n", client_pid);
 	return 0;
}


int load_bitmap_from_disk(struct atdr_user_bitmap *ubitmap_obj, int pid)
{
	FILE *fp;
	char file_name[FILENAME_LEN];
	sprintf(file_name, "%s%d", BITMAP_FILE, pid);
	printf("Load bitmap from file %s\n", file_name);
	fp = fopen(file_name, "r");
	if (fp == NULL)
	{
		atdr_log(ATDR_ERROR, "Error while opening the bitmap file to Load.\n");
		stop_work("Error while opening the file to load  bitmap");
	}
	//fscanf(fp, "0x%lx", ubitmap_obj->bitmap_area);
	fread((unsigned long int *)(ubitmap_obj->bitmap_area), ubitmap_obj->bitmap_size, 1, fp);
	printf("Loaded bitmap area = 0x%lx \n", *(ubitmap_obj->bitmap_area));
	fclose(fp);
	return 1;
}

int store_bitmap_to_disk(struct atdr_user_bitmap *ubitmap_obj, int pid)
{
	FILE *fp;
	char file_name[FILENAME_LEN];

	sprintf(file_name, "%s%d", BITMAP_FILE, pid);
	//printf("Write bitmap into file %s\n", file_name);
	fp = fopen(file_name, "w");
	if (fp == NULL)
	{
		atdr_log(ATDR_ERROR, "Error while opening the bitmap file to write.\n");
		stop_work("Error while opening the file to write into bitmap file");
	}

	fwrite((unsigned long int *)(ubitmap_obj->bitmap_area), ubitmap_obj->bitmap_size, 1, fp);
	//printf("Storing bitmap area = 0x%lx \n",*(ubitmap_obj->bitmap_area));
	fclose(fp);

	return 0;
}

static void bitmap_client_destroy(struct atdr_user_bitmap *ubitmap_obj, int pid)
{
	atdr_log(ATDR_INFO, "[bitmap_client_destroy] freeing bitmap_area... \n");
	free(ubitmap_obj->bitmap_area);
}

struct atdr_bitmap_operations bitmap_client_operations =
{
	.atdr_bitmap_create 	= bitmap_client_create,
	.atdr_bitmap_setup 		= bitmap_client_setup,
	.atdr_bitmap_destroy 	= bitmap_client_destroy,
	.store_bitmap_to_disk = store_bitmap_to_disk,
	.load_bitmap_from_disk = load_bitmap_from_disk,
};
