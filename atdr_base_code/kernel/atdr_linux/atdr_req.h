#include <linux/rbtree.h>
#include <linux/kref.h>
#include <linux/blkdev.h>
#include "ebdr_int.h"

struct bio_and_error
{
        struct bio *bio;
        int error;
};

struct ebdr_work {
        struct list_head list;
        int (*cb)(struct ebdr_work *, int cancel);
};


struct ebdr_interval {
        struct rb_node rb;
        sector_t sector;        /* start sector of the interval */
        unsigned int size;      /* size in bytes */
        sector_t end;           /* highest interval end in subtree */
        int local:1             /* local or remote request? */;
        int waiting:1;
};

struct ebdr_request {
        struct ebdr_work w;
        struct ebdr_device *device;
        struct bio *private_bio;
        struct ebdr_interval i;
        unsigned int epoch;
        struct list_head tl_requests; /* ring list in the transfer log */
        struct bio *master_bio;       /* master bio pointer */
        unsigned long start_time;
        /* once it hits 0, we may complete the master_bio */
        atomic_t completion_ref;
        /* once it hits 0, we may destroy this drbd_request object */
        struct kref kref;
        unsigned rq_state; /* see comments above _req_mod() */
};

extern struct ebdr_device device[MAX_MINOR];
extern mempool_t *ebdr_request_mempool;
extern void ebdr_update_bmap_and_sync(struct ebdr_device *device, struct bio *bio);
