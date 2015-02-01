#include <linux/fs.h>
#include <linux/version.h>
#include <linux/bio.h>
#include "ebdr_req.h"
#include "ebdr_device.h"


//void complete_master_bio(struct ebdr_device *device, struct bio_and_error *m)
void complete_master_bio(struct bio_and_error *m)
{
	bio_endio(m->bio, m->error);
}

void ebdr_request_endio(struct bio *bio, int error)
{
	struct bio_and_error m;
	struct ebdr_request *req = bio->bi_private;

	bio_put(req->private_bio);

	m.error = 0;
	m.bio = req->master_bio;
	if(m.bio)
		complete_master_bio(&m);
		//complete_master_bio(device, &m);
}

static void ebdr_submit_req_private_bio(struct bio *bio, struct ebdr_device *device)
{
	bio->bi_bdev = device->ldev->backing_bdev;
	/* Send bio to the lower level device */
	generic_make_request(bio);
}

static inline void ebdr_req_make_private_bio(struct ebdr_request *req, struct bio *bio_src)
{
	struct bio *bio;
	bio = bio_clone(bio_src, GFP_NOIO);

	req->private_bio = bio;
	bio->bi_private  = req;
	bio->bi_end_io   = ebdr_request_endio;
	bio->bi_next     = NULL;
}

static struct ebdr_request *ebdr_req_new(struct ebdr_device *device,
		struct bio *bio_src)
{
	struct ebdr_request *req;

	req = mempool_alloc(ebdr_request_mempool, GFP_NOIO);
	if (!req)
		return NULL;

	ebdr_req_make_private_bio(req, bio_src);
	req->device   = device;
	req->master_bio  = bio_src;
	req->epoch       = 0;

	return req;
}

struct ebdr_request *
ebdr_request_prepare(struct ebdr_device *device, struct bio *bio, unsigned long start_time)
{
	struct ebdr_request *req;

	req = ebdr_req_new(device, bio);
	if (!req) {
		return 0;
	}
	req->start_time = start_time;
	return req;
}

void __ebdr_make_request(struct ebdr_device *device, struct bio *bio, unsigned long start_time)
{
//	struct ebdr_request *req = ebdr_request_prepare(device, bio, start_time);
//	if (IS_ERR_OR_NULL(req))
//		return;
	//printk("Recieved make req bd_dev name %s\n", bio->bi_bdev->bd_disk->disk_name);
	ebdr_update_bmap_and_sync(device, bio); /* set ebdr bitmap and sync to disk */
	ebdr_submit_req_private_bio(bio, device);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
int ebdr_make_request(struct request_queue *q, struct bio *bio)
{
	struct ebdr_device *device = (struct ebdr_device *) q->queuedata;
	unsigned long start_time;

	start_time = jiffies;
	__ebdr_make_request(device, bio, start_time);
	return 0;
}
#else
void ebdr_make_request(struct request_queue *q, struct bio *bio)
{
	struct ebdr_device *device = (struct ebdr_device *) q->queuedata;
	unsigned long start_time;

	start_time = jiffies;
	__ebdr_make_request(device, bio, start_time);
}
#endif
