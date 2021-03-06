#include "sys_common.h"
#include "replic_client.h"
#include "ebdr_replication.h"
#include "ebdr_partner.h"
#include "ebdr_relation.h"
#include "ebdr_log.h"
#include "ebdr_replication_header.h"

int find_size(int pid)
{
    replic_hdr_client_obj[pid].opcode = DATA_REQ;

    replic_client_obj[pid].rep_conn->conn_ops->do_ebdr_conn_send(&replic_hdr_client_obj[pid],
            sizeof(replic_header), all_partner_clients[pid].socket_fd, PROTO_TYPE);

    replic_client_obj[pid].rep_conn->conn_ops->do_ebdr_conn_recv(&replic_hdr_client_obj[pid],
            sizeof(replic_header), all_partner_clients[pid].socket_fd, DATA_PREP, pid);
    
	return replic_hdr_client_obj[pid].available_size;
}

int send_hdr(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd)
{
    int ret;

    rep_hdr_rcvd = (replic_header *)buf;
    ret = send(sockfd, rep_hdr_rcvd, sizeof(replic_header), 0);
    if( ret < 0)
    {
        ebdr_log(ERROR, "ebdr conn client send hdr error...returning -1\n ");
		return -1;
		//stop_work("[send_hdr] conn client send hdr error ");
    }

    return 0;
}
		

int send_data_start(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd)
{
    int ret;

    rep_hdr_rcvd = (replic_header *)buf;
    ret = send(sockfd, rep_hdr_rcvd, sizeof(replic_header), 0);
    if( ret < 0)
    {
        ebdr_log(ERROR, "ebdr conn client send data start error return -1");
		return -1;
		//stop_work("[send_data_start] conn client send data error ");
    }
    return 0;
}



int send_data(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd)
{
#if 0
    struct ebdr_user_bitmap *usr_bitmap_rcvd;
    int ret;

    usr_bitmap_rcvd = (struct ebdr_user_bitmap *)buf;
    ret = send(sockfd, usr_bitmap_rcvd, size, 0);
    if( ret < 0)
    {
        ebdr_log(INFO, "[data_type] ebdr conn server send error");
        exit(0);
    }

#endif
    return 0;
}


void remove_relationpeer(int rid)
{
	ebdr_log(INFO, "[rm_relationpeer] rid = %d\n", rid);
	if(rid > 0)
	{
		ebdr_log(INFO, "[rm_relationpeer] calling CLIENT relation_obj_destroy \n");
		all_relation_clients[rid].ops->relation_obj_destroy(rid);
	}
}

int recv_hdr(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd)
{
  int ret;
  struct ebdr_user_bitmap *usr_bitmap_rcvd;
  int pid, rid;

  ebdr_log(INFO, "[client_recv_hdr] receiving replic header \n");
  rep_hdr_rcvd = (replic_header *)buf;
  ebdr_log(INFO, "[client_recv_hdr] opcode = %d\n", rep_hdr_rcvd->opcode);
  ret = recv(sockfd, rep_hdr_rcvd, sizeof(struct replication_header), MSG_WAITALL);
  if( ret < 0)
  {
    ebdr_log(ERROR, "ebdr conn client recv error");
    stop_work("[recv_hdr] conn client recv error ");
  }
  switch(rep_hdr_rcvd->opcode)
  {
    case SYN_ACK:
      ebdr_log(INFO, "[client_recv_hdr] ========SYN_ACK recevied from server =====\n");	
      pid = rep_hdr_rcvd->partner_id;
      ebdr_log(INFO, "[client_recv_hdr] partner_id = %d\n", pid);
      replic_hdr_client_obj[pid].ops = &replic_hdr_client_ops;
      replic_hdr_client_obj[pid].ops->replic_hdr_nego(rep_hdr_rcvd, all_partner_clients[pid].socket_fd, pid);
      break;
    case RM_RELATION_PEER:
      ebdr_log(INFO, "[client_recv_hdr] ====== RM_RELATION_PEER received from server =====\n");
      rid = rep_hdr_rcvd->relation_id;
      remove_relationpeer(rid);
      break;
    case VERIFY_DATA_RSP:
      pid = rep_hdr_rcvd->partner_id;
      verify_data_resp(pid, rep_hdr_rcvd);
    case VERIFY_DATA_IGN:
      pid = rep_hdr_rcvd->partner_id;
      replication_terminate(pid);
      break;
    default: 
      ebdr_log(INFO, "illegal opcode\n");
      break;
  }

  return 0;
}

//rohit
int verify_data_resp(int pid, replic_header * rep_hdr_rcvd)
{
  unsigned long  int bit_pos = 0;
  char md5[64] ;
  int ret;

  //Extract the md5 the comes in teh rep_hdr_rcvd
  //Compare it with the  target_disk @ pid referred to by the rep_hdr_rcvd

  memset(md5, '\0', sizeof(md5));

  if(calculate_md5(ebdr_disk_target_obj[pid].name, rep_hdr_rcvd, md5) < 0) {
    //Skip comparison some error happened
    ebdr_log(FATAL, "error md5 calculation:");
    goto out;
  }

  if(strcmp(rep_hdr_rcvd->md5, md5) != 0) {
    bit_pos = 0;
    restart_resync(pid, bit_pos, FULL_RESYNC);
  } 

out:
  replication_terminate(pid) ;

  return 0;
}

int recv_data(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd)
{
    int ret;
    struct ebdr_user_bitmap *usr_bitmap_rcvd;

    ret = recv(sockfd, buf, size, MSG_WAITALL);
    if(ret < 0)
    {
        ebdr_log(INFO, "[client]recv error");
		stop_work("[recv_data] client recv error");
    }
    ebdr_log(INFO, "[***Client_data_type] usr_bitmap_rcvd->bitmap_area= 0x%lx\n",*(unsigned long int *)buf);
    return 0;
}

int recv_data_resp(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd)
{
    int ret;
    struct ebdr_user_bitmap *usr_bitmap_rcvd;

	rep_hdr_rcvd = (replic_header *)buf;
	ret = recv(sockfd, rep_hdr_rcvd, sizeof(struct replication_header), 0);
	if( ret < 0)
	{
		ebdr_log(ERROR, "ebdr conn client recv error");
		stop_work("[recv_data_resp] conn client recv error");
	}
	if(rep_hdr_rcvd->opcode == DATA_PREP)
	{
		/* write code which will calculate available size */
		//ebdr_log(INFO, "client recieved DATA_PREP ############## size=%lu\n", rep_hdr_rcvd->available_size);
		//return rep_hdr_rcvd->available_size;
	}


    return 0;
}


