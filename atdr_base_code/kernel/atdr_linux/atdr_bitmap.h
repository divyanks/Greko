#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/gfp.h>
#include <linux/slab.h>

typedef unsigned long ulong_t;
#define MAX_BITS			32
#define BM_RESET_BITS			0
#define BM_SET_BITS			1
#define	LONG_SHIFT(NUM)			(NUM >> 6)
#define	LONG_MASK			((1 << 6) - 1)

#define BITMAP_SHIFT			(PAGE_SHIFT + 3 + 2)

#define	NEXT_LONG_BOUNDARY(val)		((val + BITS_PER_LONG - 1) & ~(BITS_PER_LONG - 1))

#if BITS_PER_LONG 		==	32
#define NUM_BPL				5 
#elif BITS_PER_LONG		==	64
#define NUM_BPL				6
#endif

#define NUM_BITS_PER_PAGE		(1UL << (PAGE_SHIFT + 3))
#define NUM_BITS_PER_PAGE_MASK		(NUM_BITS_PER_PAGE - 1)
#define NUM_WORDS_PER_PAGE		(PAGE_SIZE  >> (NUM_BPL - 3))

extern unsigned long grain_size;
//extern unsigned long u_grain_size; //replace above 'grain_size' with this later
/* Bitmap Life-Cycle */
typedef enum ebdr_bmap_state
{
	B_FREE			= 0x1,
	B_ALLOCATED		= 0x2,
	B_ACTIVE		= 0x3,
	B_RESYNC_POSSIBLE	= 0x4,
	B_RESYNC_STARTED	= 0x5,
	B_RESYNC_FINISHED	= 0x6,
	B_PERSISTENTLY_STORED	= 0x7,
} ebdr_bm_state_t;

/* Bitmap Flags */
typedef enum ebdr_bmap_flag
{
	BM_VMALLOCED = 0x10000,


} ebdr_bm_flag_t;

extern inline sector_t ebdr_get_capacity(struct block_device *blkdev);

struct ebdr_bitmap
{
	char *bitmap_area; /* pointer to vmalloc'ed area for bitmap */
	unsigned long bitmap_size;
	unsigned long grain_size;
	int bitmap_version;
	int bitmap_state;
	int bmap_user;
	spinlock_t bm_lock;
	struct ebdr_device *device;
};
int ebdr_print_bmap(struct ebdr_device *);
ulong_t ebdr_find_pos(struct ebdr_device *, sector_t);
int ebdr_set_bit(struct ebdr_device *, int);
void ebdr_bm_set_bits(struct ebdr_device *, ulong_t, ulong_t);
int ebdr_bm_set_words(char *, ulong_t, ulong_t, ulong_t);
inline char *ebdr_get_page_ptr(char *, ulong_t);
int ebdr_bm_change_bits(struct ebdr_device *, ulong_t, ulong_t, ulong_t);
