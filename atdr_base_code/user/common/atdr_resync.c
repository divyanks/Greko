#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/fs.h>
#include "ebdr_io_engine.h"
#include "ebdr_bitmap_user.h"
#include "ebdr_replication_header.h"
#include "ebdr_replication.h"
#include "ebdr_partner.h"
#include "ebdr_disk.h"
#include "ebdr_conn.h"
#include "ebdr_log.h"
#include "ebdr_daemon.h"

extern struct ebdr_thread_t thread_params[MAX_DISKS+CNTRL_THREADS];
extern THREAD_ID_T resync_tid[MAX_DISKS];
extern int recovery_mode;

int replic_verify_start(int pid)
{
	int ret = 0;

	replic_hdr_client_obj[pid].opcode = VERIFY_DATA;

	replic_client_obj[pid].rep_conn->conn_ops->do_ebdr_conn_send(&replic_hdr_client_obj[pid],
			sizeof(replic_header), all_partner_clients[pid].socket_fd, PROTO_TYPE);


	replic_client_obj[pid].rep_conn->conn_ops->do_ebdr_conn_recv(&replic_hdr_client_obj[pid],
			sizeof(replic_header), all_partner_clients[pid].socket_fd, PROTO_TYPE, pid);

	return ret;
}

void replication_terminate(int pid)
{
	/* Send termination to server */
	replic_hdr_client_obj[pid].opcode = RPLC_TERMINATION;
	/* After resync completed successfully, assign last resynced bit as zero*/
	if(all_partner_clients[pid].obj_state != PARTNER_OBJ_PAUSE)
	{
		printf("[resync_start] partner object state = %d\n", all_partner_clients[pid].obj_state);
		replic_client_obj[pid].last_resynced_bit = 0; 
	}
	/* clear buffer */
	ioctl(ebdr_disk_target_obj[pid].disk_fd, BLKFLSBUF, 0);
	/* close file descriptor */
	ebdr_log(INFO, "[resync_start(%d)] Closing target_disk_fd: [%d]\n", pid, ebdr_disk_target_obj[pid].disk_fd);
	replic_client_obj[pid].rep_conn->conn_ops->do_ebdr_conn_send(&replic_hdr_client_obj[pid],
	               sizeof(replic_header), all_partner_clients[pid].socket_fd, PROTO_TYPE);
	ebdr_log(INFO, "[resync_start (%d)] Resync Completed !! \n", pid);
	
	if(all_partner_clients[pid].obj_state == PARTNER_OBJ_RESUME)
	{
		all_partner_clients[pid].obj_state = PARTNER_OBJ_RELEASED;
	}

	bitmap_client_obj[pid].ops->ebdr_bitmap_destroy(&bitmap_client_obj[pid], pid);
}

void restart_resync(int pid , unsigned long int bit_pos, enum rep_mode_t mode)
{
	unsigned long int chunk_id = 0;
	char buff[BUF_SIZE_IN_BYTES];
	unsigned long int completed_bits[MAX_DISKS] = {0};
	unsigned long int start_lba, size_available;
	int total_iterations = 0;

	unsigned long int size = BUF_SIZE_IN_BYTES;


	total_iterations = replic_hdr_client_obj[pid].grain_size / replic_hdr_client_obj[pid].chunk_size;

	replic_hdr_client_obj[pid].opcode = DATA;

	ebdr_log(INFO, "[resync_start] bit_pos = %lu\n", bit_pos);
	ebdr_log(INFO, "[resync_start] grain_size: %lu, chunk_size:%lu\n",
			replic_hdr_client_obj[pid].grain_size, replic_hdr_client_obj[pid].chunk_size);

	/* If recovery mode is ON, then set bit_pos to last_resynced_bit and turn OFF recovery_mode
	 * else insert into resync table
	 */
	if(recovery_mode == 1)
	{
		recovery_mode = 0;
	}
	else
	{
		if(bit_pos == 0)
		{
			insert_into_resync(pid, ebdr_disk_target_obj[pid].name, bit_pos, "ebdrdbc"); /* resync table 09-10-14 */
		}
	}
	
	while( (bit_pos < (ebdr_disk_target_obj[pid].bitmap->bitmap_size * 8)) &&
			all_partner_clients[pid].obj_state != PARTNER_OBJ_PAUSE)
	{
		if(is_bit_set(ebdr_disk_target_obj[pid].bitmap->bitmap_area, bit_pos) || (mode == FULL_RESYNC))
		{
			for (chunk_id = 0; chunk_id < total_iterations; chunk_id++)
			{
				replic_hdr_client_obj[pid].curr_pos = bit_pos;
				replic_hdr_client_obj[pid].chunk_pos = chunk_id;
				replic_hdr_client_obj[pid].partner_id = pid;
		
				replic_hdr_client_obj[pid].opcode = DATA;
				ebdr_log(INFO, "[resync_start(%d)] #### bit_pos: [%lu] for CLIENT[%d]: [%d] ####\n", pid, 
						replic_hdr_client_obj[pid].curr_pos,
						pid, all_partner_clients[pid].socket_fd);
				/* send replc hdr to server */
				io_client_obj[pid].ops = &io_client_ops; // roy


				if(all_partner_clients[pid].obj_state == PARTNER_OBJ_PAUSE)
				{
					printf("break from while loop \n");
					break;
				}

				if( replic_client_obj[pid].rep_conn->conn_ops->do_ebdr_conn_send(&replic_hdr_client_obj[pid],
							sizeof(replic_header), all_partner_clients[pid].socket_fd, PROTO_TYPE) < 0)
				{
					ebdr_log(INFO, "[resync_start(%d)] cancelling pthread and PAUSING conn client object due to send error...\n", pid);
					shutdown(all_partner_clients[pid].socket_fd, SHUT_RDWR);
					close(all_partner_clients[pid].socket_fd);
					all_partner_clients[pid].socket_fd = -1;
					sleep(30);
					ebdr_conn_client[pid].obj_state = CONN_OBJ_PAUSE;
                    return; //Abondon the resync process
				}

				/* got data from server...read from sock and write to file */
				start_lba = (bit_pos * replic_hdr_client_obj[pid].grain_size) + 
					(chunk_id * replic_hdr_client_obj[pid].chunk_size);

				ebdr_log(INFO, "[resync_start(%d)] grain_size: %lu chunk_size:%lu\n", pid,
						replic_hdr_client_obj[pid].grain_size,
						replic_hdr_client_obj[pid].chunk_size);	

				if((io_client_obj[pid].ops->chunk_read(all_partner_clients[pid].socket_fd,
							buff, size, start_lba)) < 0)
				{
					ebdr_log(INFO, "[resync_start(%d)] cancelling pthread and PAUSING conn client object due to read error...\n", pid);
					shutdown(all_partner_clients[pid].socket_fd, SHUT_RDWR);
					close(all_partner_clients[pid].socket_fd);
					all_partner_clients[pid].socket_fd = -1;
					sleep(30);
					ebdr_conn_client[pid].obj_state = CONN_OBJ_PAUSE;
					return; //Abandon the resync process
				}

				if( (io_client_obj[pid].ops->chunk_write(ebdr_disk_target_obj[pid].disk_fd, buff, size, start_lba)) < 0)
				{
					ebdr_disk_target_obj[pid].obj_state = DISK_OBJ_PAUSE;
					ebdr_log(INFO, "[resyn_start(%d)] write  is paused \n", pid);
					pthread_cancel(resync_tid[pid]);
				}
			}
			//ebdr_log(INFO, "[resync_start(%d)] *** completed_bits[%d] = [%lu] *** \n", pid, pid, completed_bits[pid]++);
			update_into_resync(pid, ebdr_disk_target_obj[pid].name, bit_pos, "ebdrdbc"); /* resync table 09-10-14 */
		}
		replic_client_obj[pid].last_resynced_bit = bit_pos;
		bit_pos++;
	}

	if(all_partner_clients[pid].obj_state == PARTNER_OBJ_PAUSE)
	{
		ebdr_log(INFO, "[resync_start(%d)]====== Resync paused at bit_pos:%lu =====\n", pid, replic_client_obj[pid].last_resynced_bit);
		ebdr_log(INFO, "[resync_start(%d)] grain_size: %lu chunk_size:%lu\n", pid,
				replic_hdr_client_obj[pid].grain_size,
				replic_hdr_client_obj[pid].chunk_size);	
		pthread_cancel(resync_tid[pid]);
	}

	//We have finished replication lets verify
	if (0){ 	
		replic_verify_start(pid);
        }
	else
	{
		replication_terminate(pid);
	}	
			

}
