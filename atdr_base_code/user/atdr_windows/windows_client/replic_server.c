#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <WinSock2.h>

#include "..\Include\atdr_relation.h"
#include "..\Include\replic_server.h"
#include "..\Include\atdr_io_engine.h"
#include "..\Include\atdr_replication.h"
#include "..\Include\atdr_conn.h"
#include "..\Include\atdr_db.h"

extern int total_conn;
extern int partner_created[MAX_PID];

extern int sock_recovery[MAX_CONN];
#if 0
extern pthread_t send_recv_tid;
extern pthread_t resync_tid[MAX_DISKS];
#endif
int calculate_md5(char *disk_name, replic_header *rep_hdr_rcvd, char *md5);

int server_send_hdr(char *buf, int size, SOCKET sockfd, replic_header *rep_hdr_rcvd)
{
	int ret;

	rep_hdr_rcvd = (replic_header *)buf;
	ret = send(sockfd, (char *)rep_hdr_rcvd, sizeof(replic_header), 0);
	if( ret < 0)
	{
		atdr_log(ERROR, "atdr conn server send hdr error returning -1\n");
		return -1;
		//stop_work("[server_send_hdr] conn server send error ");
	}

	return 0;
}


int server_send_data(char *buf, int size, SOCKET sockfd, replic_header *rep_hdr_rcvd)
{
	int ret;

	ret = send(sockfd, buf, size, 0);
	if( ret < 0)
	{
		atdr_log(ATDR_ERROR, "[data_type] atdr conn server send data error =%d returning -1\n", GetLastError());
		return -1;
		//stop_work("[server_send_data] conn server send error ");
	}
	printf("[server_send_data] ........ sent %d bytes ....... \n", ret);

	return 0;
}

int get_bitmap(SOCKET sockfd, replic_header *rep_hdr_rcvd)
{
	int pid;
	pid = rep_hdr_rcvd->partner_id;

	if (all_partner_servers[pid].obj_state == PARTNER_OBJ_RELEASED)
	{
		all_partner_servers[pid].obj_state = PARTNER_OBJ_IN_USE;
		all_relation_servers[pid].obj_state = RELATION_OBJ_IN_USE;
		atdr_disk_src_obj[pid].obj_state = DISK_OBJ_IN_USE;

		update_into_partner(pid, all_partner_servers[pid].obj_state, "atdrdbs");
		update_relation_state(pid, all_relation_servers[pid].obj_state, "atdrdbs");
		update_disk_state(pid, atdr_disk_src_obj[pid].obj_state, "atdrdbs");
	}

	atdr_user_bitmap_init(LOCAL, pid);
	atdr_log(ATDR_INFO, "[get_bitmap] partner_id : [%d] \n", pid);
	atdr_disk_src_obj[pid].bitmap = &bitmap_server_obj[pid];


	bitmap_server_obj[pid].ops->atdr_bitmap_setup(atdr_disk_src_obj[pid].bitmap, pid);
	atdr_log(ATDR_INFO, "[get_bitmap] after atdr_bitmap_setup... \n");
	atdr_log(ATDR_INFO, "[get_bitmap] server sockfd : [%d]\n", all_partner_servers[pid].socket_fd);
	if (replic_server_obj[pid].rep_conn->conn_ops->do_atdr_conn_send(atdr_disk_src_obj[pid].bitmap->bitmap_area,
		atdr_disk_src_obj[pid].bitmap->bitmap_size, all_partner_servers[pid].socket_fd, DATA_TYPE) < 0)
	{
		atdr_log(ERROR, "Metadata failed to send ...\n");
		return -1;
	}

	atdr_log(ATDR_INFO, "Recieved metadata req, sent metadata 0x%lx\n",*(atdr_disk_src_obj[pid].bitmap->bitmap_area));
	io_engine_init(IO_SERVER, pid);
	return 0;
}

int get_data(SOCKET sockfd, replic_header *rep_hdr_rcvd)
{
	char rbuff[BUF_SIZE_IN_BYTES];
	int pid;
	unsigned long int start_lba;
	unsigned long int grain_index = 0, chunk_index =0;
	unsigned long int size;

	memset(rbuff, '\0', BUF_SIZE_IN_BYTES);
	grain_index = rep_hdr_rcvd->curr_pos;
	chunk_index = rep_hdr_rcvd->chunk_pos;
	pid = rep_hdr_rcvd->partner_id;

	io_server_obj[pid].ops = &io_server_ops;

	start_lba = (grain_index * replic_hdr_server_obj[pid].grain_size) +
		(chunk_index * replic_hdr_server_obj[pid].chunk_size) ;
	size = BUF_SIZE_IN_BYTES;
	/* check io object state */
	if (io_server_obj[pid].obj_state == IO_OBJ_RELEASED)
	{
		//atdr_log(INFO, "[get_data] Resync of pid = %d already completed IO object is in release state \n", pid);
		return 0;
	}

	printf("[get_data] start_lba = %lu chunk_index = %lu\n", start_lba, chunk_index);


	handle_fd_t temp;
	temp.hndle = atdr_disk_src_obj[pid].disk_fd;

	if((io_server_obj[pid].ops->chunk_read(temp, rbuff, size, start_lba)) < 0) 
	{
		atdr_conn_server[pid].obj_state = CONN_OBJ_PAUSE;
		atdr_log(ATDR_INFO, "[get_data] read is paused for pid  = %d\n", pid);
		sock_recovery[pid] = 1;
		// shutdown(atdr_conn_server[pid].conn_fd, SHUT_RDWR); SANTHOSH MAJOR CHANGE
		closesocket(atdr_conn_server[pid].conn_fd);
		Sleep(60);
		atdr_conn_server[pid].conn_fd = -1;
		partner_created[pid] = 0;
	}

	temp.fd = all_partner_servers[pid].socket_fd;
	if((io_server_obj[pid].ops->chunk_write(temp, rbuff, size, start_lba)) < 0)
	{
#if 0
		int num_rows = mkpartner_from_db_on_server();
		int i;
		printf("clearing total %d partner_created objects...\n", num_rows);
		for (i = 0; i < num_rows; i++)
		{
			partner_created[i] = 0;
		}
#endif
		partner_created[pid] = 0;
		total_conn = 0;

		atdr_log(ATDR_INFO, "[get_data] conn object is paused for pid = %d \n", pid);
		atdr_conn_server[pid].obj_state = CONN_OBJ_PAUSE;
		sock_recovery[pid] = 1;
		shutdown(atdr_conn_server[pid].conn_fd, SD_SEND);
		closesocket(atdr_conn_server[pid].conn_fd);
		Sleep (60);
		atdr_conn_server[pid].conn_fd = -1;
		partner_created[pid] = 0;
		atdr_log(ATDR_INFO, "[get_data] closing send_recv_thread ...\n");
		//pthread_cancel(send_recv_tid);
	}


	return 0;
}

int make_partner( SOCKET sockfd, replic_header *rep_hdr_rcvd)
{
	int pid = rep_hdr_rcvd->partner_id;
	int ret;

	replication_init(REP_LOCAL, pid);
	replic_server_obj[pid].ops->replic_obj_setup(&replic_server_obj[pid], pid);

	memcpy(&atdr_conn_server[pid], &atdr_conn_server[MAX_CONN + 1], sizeof(atdr_conn_t));

	atdr_log(ATDR_INFO, "[conn_server] client_ip=%s bandwidth= %lu\n",  rep_hdr_rcvd->dest_ip, rep_hdr_rcvd->bandwidth);
	if (pid < MAX_PARTNERS){
		atdr_partner_init(PARTNER_SERVER, &all_partner_servers[pid]);
		all_partner_servers[pid].ops->make_partner(rep_hdr_rcvd->dest_ip, rep_hdr_rcvd->bandwidth,
				replic_server_obj[pid].rep_conn->conn_fd, pid);
	}
	else
	{
		atdr_log(ATDR_INFO, "[conn_server] max limit for partner reached");
	}

	ret = is_server_partner_id_valid(pid);
	if(ret)
	{
		//Fetch the fd back from MAX_CONN + 1 and assign it to atdr_conn_server[pid]
		make_relation_with_client(pid, 0, rep_hdr_rcvd->disk, rep_hdr_rcvd->bitmap_size, rep_hdr_rcvd->grain_size, rep_hdr_rcvd->chunk_size);
	}
	else
	{
		atdr_log(ATDR_INFO, "Please check input again \n");
	}
	return 0;
}

int remove_partner(SOCKET sockfd, replic_header *rep_hdr_rcvd)
{
	int pid = rep_hdr_rcvd->partner_id;

	/* ========================== ASSUMPTION  ======================== 
	 * Assuming relation_id is equal to partner_id, this code won't work if both are different
	 * For simplicty, this code is written. Need to improve this code later
	 */

	/* check if the partner id is associated with relation id ?
	 * if it is associated, release relation id first and then
	 * release partner id;
	 * else release only partner id
	 */	

	if(all_relation_servers[pid].partner_id == pid && all_relation_servers[pid].obj_state == RELATION_OBJ_IN_USE)
	{
		atdr_log(ATDR_INFO, "[remove_partner] release relation obj of pid = [%d]\n", pid);
		all_relation_servers[pid].ops->relation_obj_destroy(pid);
	}

	atdr_log(ATDR_INFO, "[remove_partner] removing partner on server pid:%d\n", pid);
	all_partner_servers[pid].ops->partner_obj_destroy(pid);

	return 0;
}

int remove_relation(SOCKET sockfd, replic_header *rep_hdr_rcvd)
{
	int rid = rep_hdr_rcvd->relation_id;
	atdr_log(ATDR_INFO, "[rm_relation] calling SERVER relation_obj_destroy relation_id:%d \n", rid);
	all_relation_servers[rid].ops->relation_obj_destroy(rid);
    return 0;
}

int syn_received(SOCKET sockfd, replic_header *rep_hdr_rcvd)
{
	int pid = 0;
	pid = rep_hdr_rcvd->partner_id;

	replic_hdr_server_obj[pid].ops = &replic_hdr_server_ops;
	rep_hdr_rcvd = replic_hdr_server_obj[pid].ops->replic_hdr_nego(rep_hdr_rcvd, all_partner_servers[pid].socket_fd, pid);
	atdr_log(ATDR_INFO, "[sync_received] replic_hdr_server_obj[%d].partner_id = %d\n",
			pid, replic_hdr_server_obj[pid].partner_id);
	
	rep_hdr_rcvd->opcode = SYN_ACK;
	atdr_log(ATDR_INFO, "[syn] sending syn+ack to client opcode:%d \n", rep_hdr_rcvd->opcode);
	atdr_log(ATDR_INFO, "After Negotiation... bitmap:%lu grain:%lu chunksize:%lu \n",
			rep_hdr_rcvd->bitmap_size, rep_hdr_rcvd->grain_size, rep_hdr_rcvd->chunk_size);
	replic_server_obj[pid].rep_conn->conn_ops->do_atdr_conn_send(rep_hdr_rcvd, 
			sizeof(replic_header), all_partner_servers[pid].socket_fd, PROTO_TYPE);


	all_relation_servers[pid].bitmap_size = replic_hdr_server_obj[pid].bitmap_size;
	all_relation_servers[pid].grain_size  = replic_hdr_server_obj[pid].grain_size;
	all_relation_servers[pid].chunk_size  = replic_hdr_server_obj[pid].chunk_size;
#if 1
	insert_into_relation(all_relation_servers[pid].role, all_relation_servers[pid].partner_id, 
			all_relation_servers[pid].relation_id, RELATION_OBJ_IN_USE, all_relation_servers[pid].device,
		   	all_relation_servers[pid].bitmap_size,
	        all_relation_servers[pid].grain_size,
	        all_relation_servers[pid].chunk_size,"atdrdbs");
#endif
	return 0;
}

int ack(SOCKET sockfd, replic_header *rep_hdr_rcvd )
{

	atdr_log(ATDR_INFO, "***** protocol negotiation done on both client & server *** \n\n");
	return 0;
}

int data_req( SOCKET sockfd, replic_header *rep_hdr_rcvd )
{
	int pid = 0;
	rep_hdr_rcvd->opcode = DATA_PREP;
	rep_hdr_rcvd->available_size = 4096; /* hardcoded...write logic to calculate available size*/
	pid = rep_hdr_rcvd->partner_id;
	atdr_log(ATDR_INFO, "[server] sending DATA_PREP to client socket fd[%d]: [%d]\n", pid, all_partner_servers[pid].socket_fd);
	replic_server_obj[pid].rep_conn->conn_ops->do_atdr_conn_send(rep_hdr_rcvd, 
			sizeof(replic_header), all_partner_servers[pid].socket_fd, PROTO_TYPE);

	return 0;
}

int data_start_recieved(SOCKET sockfd, replic_header *rep_hdr_rcvd )
{
	atdr_log(ATDR_INFO, "[server] inside data_start_recieved ******** from client \n");
	stop_work("[data_start_recieved] exiting ");
	return 0;
}

int terminate(SOCKET sockfd, replic_header *rep_hdr_rcvd )
{
	int pid = rep_hdr_rcvd->partner_id;
	if (all_partner_servers[pid].obj_state != PARTNER_OBJ_RELEASED)
	{
		/* clear src disk buffer */
		// ioctl(atdr_disk_src_obj[pid].disk_fd, BLKFLSBUF, 0); SANTHOSH MAJOR CHANGE
		atdr_log(ATDR_INFO, "Replication terminate request received from client for partnerid:[%d]\n", pid);
		atdr_log(ATDR_INFO, "[server] closing src disk fd[%d]:[%d] \n", pid, atdr_disk_src_obj[pid].disk_fd);
		atdr_disk_src_obj[pid].ops->disk_unsetup(&atdr_disk_src_obj[pid]);
		bitmap_server_obj[pid].ops->atdr_bitmap_destroy(&bitmap_server_obj[pid], pid);
		recovery_mode[pid] = 0;
		/* Release all objects */
		all_partner_servers[pid].obj_state = PARTNER_OBJ_RELEASED;
		all_relation_servers[pid].obj_state = RELATION_OBJ_RELEASED;
		atdr_disk_src_obj[pid].obj_state = DISK_OBJ_RELEASED;
		io_server_obj[pid].obj_state = IO_OBJ_RELEASED;
		update_into_partner(pid, all_partner_servers[pid].obj_state, "atdrdbs");
	}
	//atdr_log(INFO, "Closing send recv thread ...\n");
	//pthread_cancel(send_recv_tid);

	return 0;
}

verify_data(SOCKET sockfd, replic_header *rep_hdr_rcvd)
{

	int pid = rep_hdr_rcvd->partner_id;

//  memset(rep_hdr_rcvd->md5, '\0', 64);

  if( (calculate_md5(atdr_disk_src_obj[pid].snap_name, rep_hdr_rcvd, NULL) < 0) || rep_hdr_rcvd->md5[0] == '\0' )
  {
    //set opcode to IGN verify protocol
    rep_hdr_rcvd->opcode = VERIFY_DATA_IGN;
  } else {
    //set opcode to verify protocol only when all the data has been fetched
    rep_hdr_rcvd->opcode = VERIFY_DATA_RSP;
  }

	replic_server_obj[pid].rep_conn->conn_ops->do_atdr_conn_send(rep_hdr_rcvd, 
			sizeof(replic_header), all_partner_servers[pid].socket_fd, PROTO_TYPE);
	return 0;
}

#define MAX_CMD_LEN 100
#define MAX_FILENAME 64
#ifdef SERVER
#define RESYNC_FILE_MD5 "/tmp/resync_srv_"
#define RESYNC_FILE_MD5_ERROR "/tmp/resync_error_srv"
#endif
#ifdef CLIENT
#define RESYNC_FILE_MD5 "/tmp/resync_clt_"
#define RESYNC_FILE_MD5_ERROR "/tmp/resync_error_clt"
#endif
#define CMD_TO_VERIFY "/usr/bin/md5sum"

int calculate_md5(char *disk_name, replic_header *rep_hdr_rcvd, char *md5)
{
#if 0
  //Server recieved request to verify the snap disk
  int rid;
  char str[MAX_CMD_LEN];
  char filename[MAX_FILENAME];
  int fd, fd_error;
  int ret = -1, status;
  FILE *fp = NULL;
  int pid = rep_hdr_rcvd->partner_id;

  memset(filename, '\0', MAX_FILENAME);

  sprintf(filename, "%s%d", RESYNC_FILE_MD5, pid);

  fd = open(filename, O_RDWR | O_CREAT);

  //Re-using the filename string to have new file name for error
  memset(filename, '\0', MAX_FILENAME);
  sprintf(filename, "%s%d", RESYNC_FILE_MD5_ERROR, pid);

  fd_error = open(filename, O_RDWR | O_CREAT);

  if (fd < 0 || fd_error < 0) {

    atdr_log(ATDR_FATAL, "This pid %d unable to open the %s file \n", pid, filename); 
    goto out;

  } else {
    memset(str, '\0', MAX_CMD_LEN);
    sprintf(str, "%s", CMD_TO_VERIFY);

    ret = atdr_exec_redirect(str, disk_name, -1, fd, fd_error );
    if(ret < 0)
    {
      atdr_log(ATDR_FATAL, "This pid %d unable to open the %s file \n", pid, filename); 
      goto out;

    }
    do {
      if(waitpid(ret, &status, WUNTRACED | WCONTINUED) < 0 )
      {
        perror("wait error");
        ret = -1;
        goto out;
      }
    } while (!WIFEXITED(status));

  }

  if(get_last_md5_values(fd, rep_hdr_rcvd, md5) < 0) {
    atdr_log(ATDR_FATAL, "This pid %d unable to read the %s file %d \n", pid, filename, errno); 
    ret = -1;
    goto out;
  }

out:
  return ret;
#endif
  return 0;
}
