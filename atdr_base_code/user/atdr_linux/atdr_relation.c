#include "sys_common.h"
#include "ebdr_log.h"
#include "ebdr_relation.h"

void *ebdr_relation_init(enum relation_mode_t relation_mode, relation_t *relation_obj, int pid)
{

    if(relation_mode == EBDR_RELATION_SERVER)
    {   
        relation_obj->ops = &relation_server_ops;
    }   
    else
    {   
        relation_obj->ops = &relation_client_ops;
    }   

    relation_obj->ops->relation_obj_create(relation_obj, pid);
}

