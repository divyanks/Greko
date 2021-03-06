#ifndef EBDR_RELATION_H
#define EBDR_RELATION_H

#define MAX_RELATIONS 2048

enum rel_obj_state_t
{
	RELATION_OBJ_FREE,
	RELATION_OBJ_IN_USE,
	RELATION_OBJ_RELEASED
};

enum relation_mode_t
{
    EBDR_RELATION_SERVER,
    EBDR_RELATION_CLIENT
};

enum relation_role_t
{
	RELATION_PRIMARY,
	RELATION_SECONDARY
};

enum relationship_status
{ 
    RELATED,
    UNRELATED,
};
struct relation;

typedef struct relation_operations
{
    void (*relation_obj_create)(struct relation *relation_obj, int pid);
    int  (*make_relation)(int role, int id, char *disk);
    void (*relation_nego)(struct relation *ptnr_req);
    void (*list_relations)(void);
    void (*relation_obj_destroy)(int rid);
}relation_operations_t;

extern relation_operations_t relation_server_ops;
extern relation_operations_t relation_client_ops;


typedef struct relation
{
    enum relation_role_t role;
    int relation_id;
    int partner_id;
    int relation_status;
    enum rel_obj_state_t obj_state;	
    char device[64];
    unsigned long int capacity;
    unsigned long int grain_size;
    unsigned long int bitmap_size;
    unsigned long int chunk_size;

    relation_operations_t *ops;
} relation_t;

relation_t all_relation_servers[MAX_RELATIONS];
relation_t all_relation_clients[MAX_RELATIONS];

extern int ps_count;
extern int pc_count;
#endif
