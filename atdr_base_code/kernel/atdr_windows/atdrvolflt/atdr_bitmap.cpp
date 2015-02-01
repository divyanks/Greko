#include "atdr_bitmap.h"
#define set_bit(nr, addr)    (void)test_and_set_bit(nr, addr)



 int test_and_set_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned int *adr = (unsigned int *)addr;

	
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);

	retval = (mask & *adr) != 0;
	*adr |= mask;
	
	return retval;
}

void ebdr_set_bits(PEBDR_BITMAP bitmap, int first, int last)
{
	int i = 0;
	unsigned long *bmap = NULL;
	KIRQL Irql;
	//Lock the access it may get changed due to SET_GRAIN IOCTL
//	spin_lock(&device->bitmap->bm_lock);
	KeAcquireSpinLock(&bitmap->Lock_bm, &Irql);
	bmap = (long unsigned int *)bitmap->bitmap_area;
	for (i = first; i <= last; i++)
	{
		set_bit(i, bmap);
	}
//	spin_unlock(&device->bitmap->bm_lock);
	KeReleaseSpinLock(&bitmap->Lock_bm, Irql);

	

}



//This address is in sector hence convert addr = address >> 9;
//Grain_size is in bytes
UINT32 ebdr_find_pos( EBDR_BITMAP *bitmap, sector_t addr)
{
	UINT32 cur =  0;
	UINT32 	 grain_size = bitmap->grain_size;
	UINT32 num_bits = bitmap->total_num_bits;

	for (cur = 0; cur < num_bits; cur++)
	{
		if (addr < ((cur + 1) * (grain_size >> 9)))
		{
			return cur;
		}
	}
	return cur;
}

