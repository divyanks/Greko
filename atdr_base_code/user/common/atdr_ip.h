#ifndef EBDR_IP_H
#define EBDR_IP_H

#define MAX_IPS 2048

typedef enum 
{
    CONN_DOWN,
    CONN_UP,
    IF_DOWN,
    IF_UP,
	NETLINK_SERVER,
	NETLINK_CLIENT
} ip_mode;


typedef enum
{
	IP_OBJ_IN_USE,
	IP_OBJ_PAUSE,
	IP_OBJ_RELEASED
}ip_state_t;


typedef struct partner_list
{
    void *obj;//partner object downcasted to void *
    struct partner_list *next;
} ptnr_list_t;

/* Only one type clients are added to the list.
 * Should we decide to change it we will need to 
 * add ops argument to the list_t 
 * ToDo: 
 *   Think when should we create/Delete/modify this object
 */

typedef struct ebdr_ip
{
	char ip[64];
	ptnr_list_t *client; //Each list of partners connected
	ip_mode type; //type could be server/client     //up /down
    ip_state_t state;	
	//Let only one client register their operations on this object
	struct ebdr_ip_operations *ops;
}ebdr_ip_t;


typedef struct ebdr_ip_operations 
{
	void (*do_ebdr_ip_obj_init) (char *ip, ip_mode type);
  	void (*do_ebdr_ip_obj_setup) (ebdr_ip_t *ip_obj, int pid);
	void (*do_ebdr_ip_obj_unsetup) (ebdr_ip_t *ip_obj, int pid);

}ebdr_ip_operations_t;

extern struct ebdr_ip_operations ip_server_ops;
extern struct ebdr_ip_operations ip_client_ops;


ebdr_ip_t ebdr_ip_obj;

ebdr_ip_t ebdr_ip_server_obj[MAX_IPS];
ebdr_ip_t ebdr_ip_client_obj[MAX_IPS];

ebdr_ip_t *all_ips[MAX_IPS];

ebdr_ip_t all_ips_server[MAX_IPS];
ebdr_ip_t all_ips_client[MAX_IPS];

#endif
