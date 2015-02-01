#ifndef EBDR_PARTNER_H
#define EBDR_PARTNER_H

#define MAX_PARTNERS 2048

enum partner_obj_state_t
{
	PARTNER_OBJ_FREE,
	PARTNER_OBJ_IN_USE = 1,
	PARTNER_OBJ_RELEASED,
	PARTNER_OBJ_PAUSE,
	PARTNER_OBJ_RECOVERY, 
	PARTNER_OBJ_RESUME
};

enum partner_mode_t
{
    PARTNER_SERVER,
    PARTNER_CLIENT
};
enum partnership_status
{ 
    FAILED,
    ACTIVE,
    DEGRADED
};
struct partner;

typedef struct partner_operations
{
    void (*partner_obj_create)(struct partner *);
    int  (*make_partner)(char *ip, unsigned long int bandwidth, int sfd, int partnerid);
    void (*partner_nego)(struct partner *ptnr_req);
    void (*list_partners)(void);
    void (*partner_obj_destroy)(int pid);
}partner_operations_t;

extern partner_operations_t partner_server_ops;
extern partner_operations_t partner_client_ops;


typedef struct partner
{
    int id; 
    char ip[64];
    unsigned long int bandwidth;
    int status;
	enum partner_obj_state_t obj_state;
	int socket_fd;
    partner_operations_t *ops;
} partner_t;


partner_t all_partner_servers[MAX_PARTNERS];
partner_t all_partner_clients[MAX_PARTNERS];

extern int ps_count;
extern int pc_count;

#endif
