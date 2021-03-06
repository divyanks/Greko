#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <WinSock2.h>
#include<io.h>
#include<stdio.h>
#include<winsock2.h>

#include "..\Include\atdr_io_engine.h"
#include "..\Include\atdr_bitmap_user.h"
#include "..\Include\atdr_replication_header.h"
#include "..\Include\atdr_replication.h"
#include "..\Include\atdr_partner.h"
#include "..\Include\atdr_disk.h"
#include "..\Include\atdr_conn.h"
#include "..\Include\atdr_log.h"
#include "..\Include\atdr_daemon.h"
#include "..\Include\atdr_db.h"
#include "..\Include\atdr_relation.h"

extern struct atdr_thread_t thread_params[MAX_DISKS+CNTRL_THREADS];
extern HANDLE resync_tid[MAX_DISKS]; 
extern int recovery_mode[MAX_PID];
int update_into_resync(int pid, char *target_disk, unsigned long int last_resynced_bit, int state, char *db_name);
int insert_into_resync(int pid, char *target_disk, unsigned long int last_resynced_bit, int state, char *status, char *db_name);
int replic_verify_start(int pid)
{
	int ret = 0;

	replic_hdr_client_obj[pid].opcode = VERIFY_DATA;

	replic_client_obj[pid].rep_conn->conn_ops->do_atdr_conn_send(&replic_hdr_client_obj[pid],
			sizeof(replic_header), all_partner_clients[pid].socket_fd, PROTO_TYPE);


	replic_client_obj[pid].rep_conn->conn_ops->do_atdr_conn_recv(&replic_hdr_client_obj[pid],
			sizeof(replic_header), all_partner_clients[pid].socket_fd, PROTO_TYPE, pid);

	return ret;
}

void replication_terminate(int pid)
{
	atdr_disk_target_obj[pid].ops->disk_unsetup(&atdr_disk_target_obj[pid]);

	if (replic_client_obj[pid].rep_state != RESYNC_COMPLETED)
	{
		/* Send termination to server */
		printf("[replication_terminate] ... Sending terminate to SERVER ...\n");
		replic_hdr_client_obj[pid].opcode = RPLC_TERMINATION;
		/* After resync completed successfully, assign last resynced bit as zero*/
		if (all_partner_clients[pid].obj_state != PARTNER_OBJ_PAUSE)
		{
			printf("[resync_start] partner object state = %d\n", all_partner_clients[pid].obj_state);
			replic_client_obj[pid].last_resynced_bit = 0;
		}
		/* clear buffer */
		// ioctl(atdr_disk_target_obj[pid].disk_fd, BLKFLSBUF, 0); SANTHOSH MAJOR CHANGE
		/* close file descriptor */
		atdr_log(ATDR_INFO, "[resync_start(%d)] Closing target_disk_fd: [%d]\n", pid, atdr_disk_target_obj[pid].disk_fd);
		replic_client_obj[pid].rep_conn->conn_ops->do_atdr_conn_send(&replic_hdr_client_obj[pid],
			sizeof(replic_header), all_partner_clients[pid].socket_fd, PROTO_TYPE);
		atdr_log(ATDR_INFO, "[resync_start (%d)] Resync Completed !! \n", pid);

		/* Release all objects */
		all_partner_clients[pid].obj_state = PARTNER_OBJ_RELEASED;
		all_relation_clients[pid].obj_state = RELATION_OBJ_RELEASED;
		atdr_disk_target_obj[pid].obj_state = DISK_OBJ_RELEASED;
		io_client_obj[pid].obj_state = IO_OBJ_RELEASED;

		replic_client_obj[pid].rep_state = RESYNC_COMPLETED;
		bitmap_client_obj[pid].ops->atdr_bitmap_destroy(&bitmap_client_obj[pid], pid);
		update_into_partner(pid, all_partner_clients[pid].obj_state, "atdrdbc");
		update_resync_state(pid, replic_client_obj[pid].rep_state, "RESYNC_COMPLETED", "atdrdbc");
		//delete_from_resync(pid, "atdrdbc");
	}
}

void restart_resync(int pid , unsigned long int bit_pos, enum rep_mode_t mode)
{
	unsigned long int chunk_id = 0;
	char buff[BUF_SIZE_IN_BYTES];
	unsigned long int completed_bits[MAX_DISKS] = {0};
	unsigned long int start_lba; 
	unsigned int total_iterations = 0;

	unsigned long int size = BUF_SIZE_IN_BYTES;


	total_iterations = replic_hdr_client_obj[pid].grain_size / replic_hdr_client_obj[pid].chunk_size;

	replic_hdr_client_obj[pid].opcode = DATA;

	atdr_log(ATDR_INFO, "[resync_start] bit_pos = %lu\n", bit_pos);
	atdr_log(ATDR_INFO, "[resync_start] grain_size: %lu, chunk_size:%lu\n",
			replic_hdr_client_obj[pid].grain_size, replic_hdr_client_obj[pid].chunk_size);

	/* If recovery mode is ON, then set bit_pos to last_resynced_bit and turn OFF recovery_mode
	 * else insert into resync table
	 */
	if (recovery_mode[pid] == 1)
	{
		recovery_mode[pid] = 0;
	}
	else
	{
		if(bit_pos == 0)
		{
			insert_into_resync(pid, atdr_disk_target_obj[pid].name, bit_pos, RESYNC_IN_PROGRESS, "RESYNC_IN_PROGRESS", "atdrdbc");
			replic_client_obj[pid].rep_state = RESYNC_IN_PROGRESS;
		}
	}
	atdr_disk_target_obj[pid].ops->disk_setup(&atdr_disk_target_obj[pid]);

	while( (bit_pos < (atdr_disk_target_obj[pid].bitmap->bitmap_size * 8)) &&
			(all_partner_clients[pid].obj_state != PARTNER_OBJ_PAUSE))
	{
		if(is_bit_set(atdr_disk_target_obj[pid].bitmap->bitmap_area, bit_pos) || (mode == FULL_RESYNC))
		{
			for (chunk_id = 0; chunk_id < total_iterations; chunk_id++)
			{
				replic_hdr_client_obj[pid].curr_pos = bit_pos;
				replic_hdr_client_obj[pid].chunk_pos = chunk_id;
				replic_hdr_client_obj[pid].partner_id = pid;
		
				replic_hdr_client_obj[pid].opcode = DATA;
				atdr_log(ATDR_INFO, "[resync_start(%d)] #### bit_pos: [%lu] for CLIENT[%d]: [%d] ####\n", pid, 
						replic_hdr_client_obj[pid].curr_pos,
						pid, all_partner_clients[pid].socket_fd);
				/* send replc hdr to server */
				io_client_obj[pid].ops = &io_client_ops; // roy


				if(all_partner_clients[pid].obj_state == PARTNER_OBJ_PAUSE)
				{
					printf("break from while loop \n");
					break;
				}

				if (io_client_obj[pid].obj_state == IO_OBJ_RELEASED)
				{
					atdr_log(ATDR_INFO, "[resync_start(%d)] Resync already completed...IO object is in release state\n", pid);
					return;
				}

				if( replic_client_obj[pid].rep_conn->conn_ops->do_atdr_conn_send(&replic_hdr_client_obj[pid],
							sizeof(replic_header), all_partner_clients[pid].socket_fd, PROTO_TYPE) < 0)
				{
					atdr_log(ATDR_INFO, "[resync_start(%d)] cancelling pthread and PAUSING conn client object due to send error...\n", pid);
					shutdown(all_partner_clients[pid].socket_fd, SD_SEND); 
					closesocket(all_partner_clients[pid].socket_fd); 
					all_partner_clients[pid].socket_fd = -1;
					Sleep(60); 
					atdr_conn_client[pid].obj_state = CONN_OBJ_PAUSE;
                    return; //Abondon the resync process
				}

				/* got data from server...read from sock and write to file */
				start_lba = (bit_pos * replic_hdr_client_obj[pid].grain_size) + 
					(chunk_id * replic_hdr_client_obj[pid].chunk_size);

				atdr_log(ATDR_INFO, "[resync_start(%d)] grain_size: %lu chunk_size:%lu\n", pid,
						replic_hdr_client_obj[pid].grain_size,
						replic_hdr_client_obj[pid].chunk_size);	

				handle_fd_t temp;
				temp.fd = all_partner_clients[pid].socket_fd;
				if((io_client_obj[pid].ops->chunk_read(temp,
							buff, size, start_lba)) < 0)
				{
					atdr_log(ATDR_INFO, "[resync_start(%d)] cancelling pthread and PAUSING conn client object due to read error...\n", pid);
					shutdown(all_partner_clients[pid].socket_fd, SD_SEND); 
					closesocket(all_partner_clients[pid].socket_fd); 
					all_partner_clients[pid].socket_fd = -1;
					Sleep(30); 
					atdr_conn_client[pid].obj_state = CONN_OBJ_PAUSE;
					return; //Abandon the resync process
				}

				temp.hndle = atdr_disk_target_obj[pid].disk_fd;

				if( (io_client_obj[pid].ops->chunk_write(temp, buff, size, start_lba)) < 0)
				{
					atdr_disk_target_obj[pid].obj_state = DISK_OBJ_PAUSE;
					atdr_log(ATDR_INFO, "[resyn_start(%d)] write  is paused \n", pid);
					ExitThread(-2); // -2 for when write failed.
				}
			}
			//atdr_log(INFO, "[resync_start(%d)] *** completed_bits[%d] = [%lu] *** \n", pid, pid, completed_bits[pid]++);
			update_into_resync(pid, atdr_disk_target_obj[pid].name, bit_pos, replic_client_obj[pid].rep_state, "atdrdbc");
		}
		replic_client_obj[pid].last_resynced_bit = bit_pos;
		bit_pos++;
	}

	if(all_partner_clients[pid].obj_state == PARTNER_OBJ_PAUSE)
	{
		atdr_log(ATDR_INFO, "[resync_start(%d)]====== Resync paused at bit_pos:%lu =====\n", pid, replic_client_obj[pid].last_resynced_bit);
		atdr_log(ATDR_INFO, "[resync_start(%d)] grain_size: %lu chunk_size:%lu\n", pid,
				replic_hdr_client_obj[pid].grain_size,
				replic_hdr_client_obj[pid].chunk_size);	
		ExitThread(-3); // -3 is user has asked for pause.
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
