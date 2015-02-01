#include "ebdr_device.h"
#include "ebdr_int.h"
#include "ebdr_req.h"
#include "ebdr_bitmap.h"
#include "ebdr_memctl.h"


struct ebdr_bitmap *bitmap;
extern char *vmalloc_area[MAX_MINOR+1];
unsigned long grain_size;
//extern unsigned long u_grain_size;
extern IOCTL ioctl_obj;
extern unsigned long vmalloc_area_sz[MAX_MINOR+1];
int ebdr_print_bmap(struct ebdr_device *device)
{
	char *bitmap = device->bitmap->bitmap_area;
	size_t size = device->bitmap->bitmap_size/8, i;
	for (i = 0; i < size; i++) {
		printk("%x %c", bitmap[i]&0xff, ((i+1)%24)?' ':'\n');
	}
	return (0);
}

void ebdr_print_bmap_range(struct ebdr_device *device, ulong_t start, ulong_t end)
{
	ulong_t i;
	char *bitmap = device->bitmap->bitmap_area;
	int size = device->bitmap->bitmap_size;
	if ((start < 0) || (end >= size))
		printk("ebdr_print_bmap_range: Invalid bitmap range\n");
	for (i = start/8; i <= end/8; i++) {
		printk("%x %c", bitmap[i]&0xff, (i%8)?' ':'\n');
	}
	printk("\n");
}

void ebdr_clear_bmap(struct ebdr_device *device)
{
	struct ebdr_bitmap *b = device->bitmap;
	ulong_t *val = (ulong_t *)b->bitmap_area;
	int i = 0, size = b->bitmap_size/8;
	for (i = 0; i < size/sizeof (ulong_t); i++)
		val[i] = 0x0;
}

void ebdr_clear_bmap_range(struct ebdr_device *device, ulong_t begin, ulong_t end)
{
	struct ebdr_bitmap *b = device->bitmap;
	char *val = b->bitmap_area;
	int size = b->bitmap_size;
	ulong_t i = 0;
	if ((begin < 0) || (end >= size)) {
		printk("ebdr_clear_bmap_range: invalid range begin %lu end %lu",
		    begin, end);
		return;
	}
	for (i = begin/8; i <= end/8; i++)
		val[i] = 0x0;
}

int ebdr_init_bmap(struct ebdr_device *device, unsigned long bitmap_page_size)
{
	bitmap = kzalloc(sizeof(struct ebdr_bitmap), GFP_KERNEL);
	if (!bitmap) {
		return (-ENOMEM);
	}
	memset(bitmap, '0', sizeof(struct ebdr_bitmap));
	spin_lock_init(&bitmap->bm_lock);
	bitmap->bitmap_area = vmalloc_area[device->minor];
	bitmap->bmap_user = KERNEL;
//	bitmap->bitmap_size = PLKT_MAX_KMALLOC_BYTES*8;
	bitmap->device = device;
	device->bitmap = bitmap;
	bitmap->grain_size = ioctl_obj.u_grain_size; //set by ioctl using ./app -g
	bitmap->bitmap_size = (vmalloc_area_sz[device->minor] * 8); // /set by ioctl using ./app -g 
	printk("bitmap->grain_size = %lu \n", bitmap->grain_size);
	printk("device->bitmap = 0x%p \n", device->bitmap);
	ebdr_clear_bmap(device);
	return (0);
}

void ebdr_free_bitmap(struct ebdr_device *device)
{
	/* Inform Memory pool to free bmap area */
	kfree(device->bitmap);
	device->bitmap = NULL;
}

/* Set Words in a given range except last word */
int ebdr_bm_set_words(char *bmap, ulong_t pno, ulong_t start, ulong_t end)
{
	ulong_t *padd = (ulong_t *)ebdr_get_page_ptr(bmap, pno);
	ulong_t i;
	if (!padd)
		return (-EFAULT);

	for (i = start; i < end; i++) {
		padd[i] = ~0x0;
		/* Update #bits set in nset_bits */
	}
	return (0);
}

inline int ebdr_bm_bit_to_page(ulong_t bnum)
{
	return (bnum >> (PAGE_SHIFT+3));
}

inline char *ebdr_get_page_ptr(char *bitmap, ulong_t pnum)
{
	if ((pnum * NUM_BITS_PER_PAGE/8) > PLKT_MAX_KMALLOC_BYTES) {
		printk("ebdr_get_page_ptr:WARN:out of bound access\n");
		return (NULL);
	}
	return (bitmap + pnum * NUM_BITS_PER_PAGE/8);
}

int ebdr_bm_set_bit2(ulong_t num, ulong_t *arg)
{
	ulong_t temp = num%BITS_PER_LONG;
	ulong_t mask = (1UL << temp);
	ulong_t off = num/BITS_PER_LONG;
	ulong_t *var;
	if (!arg)
		return (-EFAULT);
	var = ((ulong_t *)arg + off);
	*var |= mask;
	return (0);
}

int ebdr_bm_clear_bit2(ulong_t num, ulong_t *arg)
{
	ulong_t temp = num % BITS_PER_LONG;
	ulong_t *var, mask;
	if (!arg)
		return (-EFAULT);
	var = ((ulong_t *)arg + num/BITS_PER_LONG);
	mask = (1UL << temp);
	*var &= ~mask;
	return (0);
}

int ebdr_bm_change_bits(struct ebdr_device *device, ulong_t start, ulong_t end, ulong_t arg)
{
	struct ebdr_bitmap *bmap = device->bitmap;
	ulong_t nbits = bmap->bitmap_size, i;
	int pnum, lpage = -1;
	ulong_t *paddr = NULL;
	/* arg = 1, set the bits from start to end
	 * else reset the bits from start to end
	 */
	if ((nbits > 0) && (end >= nbits)) {
		end = nbits - 1;
	}
	/* printk("ebdr_bm_change_bits: before set bits start %lu end %lu\n", start, end); */

	for (i = start; i <= end; i++) {
		pnum = ebdr_bm_bit_to_page(i);
		if (lpage != pnum) {
			paddr = (ulong_t *)ebdr_get_page_ptr(bmap->bitmap_area, pnum);
			lpage = pnum;
		}
		if (arg == BM_SET_BITS) {
			ebdr_bm_set_bit2(i & NUM_BITS_PER_PAGE_MASK, (ulong_t *)paddr);
			/* test_and_set_bit(i & NUM_BITS_PER_PAGE_MASK, paddr); */
		}
		else {
			ebdr_bm_clear_bit2(i & NUM_BITS_PER_PAGE_MASK, (ulong_t *)paddr);
			/* test_and_clear_bit(i & NUM_BITS_PER_PAGE_MASK, paddr); */
		}
	}
	/* printk("ebdr_bm_change_bits: after set bits start %lu end %lu\n", start, end); */
	/* ADD: update new #bits set in nset_bits var of bitmap structure */
	return (0);
}

void ebdr_bm_set_bits(struct ebdr_device *device, ulong_t begin, ulong_t end)
{
	struct ebdr_bitmap *bitmap = device->bitmap;
	ulong_t b1, e1;
	ulong_t fpage, lpage, pnum;
	ulong_t fword, lword;
	ulong_t delta = 3*BITS_PER_LONG;

	/* Begin is 10, end is 97 : b1 = 32, e1 = 96, fword = 1, lword = 3 */
	/* set word 1, 2 ONLY; NOT word 3 */
	b1 = NEXT_LONG_BOUNDARY(begin);
	e1 = (end + 1) & ~((ulong_t)BITS_PER_LONG - 1);
	printk("ebdr_bm_set_bits: Step1 passed\n");
	if ((end - begin) <= delta) {
		ebdr_bm_change_bits(device, begin, end, BM_SET_BITS);
		return;
	}
	/* Setting Bits from begin to (b1-1) */
	if (b1)
		ebdr_bm_change_bits(device, begin, (b1 - 1), BM_SET_BITS);


	/* Set the words from b1 to e1 */
	/* check limits of b1 and e1 */
	fpage = b1 >> (PAGE_SHIFT + 3);
	lpage = e1 >> (PAGE_SHIFT + 3);

	fword = (b1 >> NUM_BPL);
	fword %= NUM_WORDS_PER_PAGE;

	lword = NUM_WORDS_PER_PAGE;
	/* Set Bits from b to (b1 - 1) */

	/* Set Words from b1 to e1 */
	printk("ebdr_bm_set_bits: Step2 passed\n");
	for (pnum = fpage; pnum < lpage; pnum++) {
		ebdr_bm_set_words(bitmap->bitmap_area, pnum, fword, lword);
		fword = 0;
	}
	 printk("ebdr_bm_set_bits: Step3 passed\n");

	/* Set words in last page */
	lword = (e1 >> NUM_BPL);
	lword %= NUM_WORDS_PER_PAGE;
	if (lword)
		ebdr_bm_set_words(bitmap->bitmap_area, lpage, fword, lword);

	/* Set Bits from e1 to e */
	if (e1 <= end)
		ebdr_bm_change_bits(device, e1, end, BM_SET_BITS);
	return;
}


int ebdr_set_bit(struct ebdr_device *device, int first)
{
	ulong_t *bmap = (ulong_t *)device->bitmap->bitmap_area;
	*bmap |= (1 << first);

	return (0);
}

int ebdr_set_bits(struct ebdr_device *device, int first, int last)
{
	int i=0;
	unsigned long *bmap = NULL;

	//Lock the access it may get changed due to SET_GRAIN IOCTL
	spin_lock(&device->bitmap->bm_lock);
        bmap = (long unsigned int *)device->bitmap->bitmap_area; 
	for( i = first; i <= last; i++)
	{
		__set_bit(i, bmap);
	}
	spin_unlock(&device->bitmap->bm_lock);

	return (0);

}

ulong_t ebdr_find_pos(struct ebdr_device *device, sector_t addr)
{
	loff_t capacity;
	ulong_t cur;
	ulong_t num_bits = device->bitmap->bitmap_size;
	//ulong_t num_bits = 32*8; //DEBUG


	capacity = device->this_bdev->bd_inode->i_size;
	grain_size = device->bitmap->grain_size;
//	grain_size = device->bitmap->grain_size = (capacity >> BITMAP_SHIFT);
	//printk("ebdr_find_pos: capacity %lld grain_size %ld\n", capacity, grain_size);
	/*
	The field 'bd_inode' (in struct block_device points to the inode in the
	bdev file system. 
	The device will be accessed as a block device type file. That 
	inode that represents that device file is stored in the bd_inodes list
	*/
	for(cur = 0; cur < num_bits; cur++)
   	{
		if(addr < ((cur + 1) * (grain_size>>9)))
	   	{
			return cur;
		}
	}
	return (num_bits-1);
}

#define bio_sectors(bio)        ((bio)->bi_size >> 9)
#define bio_end_sector(bio)     ((bio)->bi_sector + bio_sectors((bio)))

int ebdr_set_bitmap(struct ebdr_device *device, struct bio *bio)
{
	sector_t start_addr, end_addr;
	ulong_t first, last;

	start_addr = bio->bi_sector;
	end_addr = bio_end_sector(bio);

	first = ebdr_find_pos(device, start_addr);
	last =  ebdr_find_pos(device, end_addr);
	ebdr_set_bits(device, first, last);
    //printk("grainsize: %ld start addr: %llu, end_add:%llu first_pos:%ld, last_pos %ld \n ", grain_size, start_addr, end_addr, first, last);
	return (0);
}

int ebdr_write_bitmap_and_sync_disk(struct ebdr_device *device)
{
	#if 0
	//DEBUG--remove it
	char *bitmap = device->bitmap->bitmap_area;
	size_t size = device->bitmap->bitmap_size/8, i;
	size = 32;
	for (i = 0; i < size; i++) {
		printk("%x %c", bitmap[i]&0xff, ((i+1)%16)?' ':'\n');
	}
	#endif
	return (0);
}

void ebdr_update_bmap_and_sync(struct ebdr_device *device, struct bio *bio)
{
	if(bio_data_dir(bio) == WRITE) {
		ebdr_set_bitmap(device, bio);
	}
	ebdr_write_bitmap_and_sync_disk(device);
}
