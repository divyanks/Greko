#ifndef __EBDR_RPLC_PROTOCOL_H 
#define __EBDR_RPLC_PROTOCOL_H

#define MAX_HDR 2048

enum rep_mode_t
{ 
  NO_RESYNC,
  FULL_RESYNC
};

enum rep_hdr_state_t
{
	REP_HDR_OBJ_FREE,
	REP_HDR_OBJ_IN_USE,
	REP_HDR_OBJ_RELEASED
};

enum rep_hdr_role_t
{
	REP_HDR_SERVER,
	REP_HDR_CLIENT
};

enum proto_opcode_t 
{
	SYN,
	SYN_ACK,
	ACK,
	BITMAP_METADATA,
	DATA,
	RPLC_TERMINATION,
	MAKE_PARTNER,
	REMOVE_PARTNER,
	RM_RELATION,
	RM_RELATION_PEER,
	DATA_REQ,
	DATA_PREP,
	DATA_START,
	DATA_DONE,
  VERIFY_DATA,
  VERIFY_DATA_RSP,
  VERIFY_DATA_IGN

};

struct replication_header;

struct replication_header_operations
{
	void (*replic_hdr_create)(enum rep_hdr_role_t rep_hdr_role, int pid);
	void (*replic_hdr_setup)(unsigned long int bitmap_size, unsigned long int grain_size,
		   	unsigned long int chunk_size, int pid);
	struct replication_header * (*replic_hdr_nego)(struct replication_header *rep_hdr_obj, int sockfd, int pid);
	void (*replic_hdr_destroy)(struct replication_header *rep_hdr_obj);

};

extern struct replication_header_operations replic_hdr_server_ops;
extern struct replication_header_operations replic_hdr_client_ops;

struct replication_header
{
  char replic_version;
  enum proto_opcode_t opcode;
  unsigned long int available_size;
  unsigned long int bitmap_size;
  unsigned long int grain_size;
  unsigned long int chunk_size;
  unsigned long int bandwidth;
  unsigned long int curr_pos;
  unsigned long int chunk_pos;
  char md5[64];
  char dest_ip[64];
  int partner_id;
  int relation_id;
  enum rep_hdr_role_t role;
  enum rep_hdr_state_t obj_state;
  char disk[64];
  struct replication_header_operations *ops;
};

typedef struct replication_header replic_header;
replic_header replic_hdr_obj;

replic_header replic_hdr_server_obj[MAX_HDR];
replic_header replic_hdr_client_obj[MAX_HDR];
void replication_header_init(enum rep_hdr_role_t role, int pid);

#endif /*__EBDR_RPLC_PROTOCOL_H*/
