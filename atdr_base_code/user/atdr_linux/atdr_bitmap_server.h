#ifndef __EBDR_SERVER_BITMAP_H
#define __EBDR_SERVER_BITMAP_H

#define PAGE_SHIFT              12
#define PAGE_SIZE               (1UL << PAGE_SHIFT)
#define NUM_BITS_PER_PAGE           (1UL << (PAGE_SHIFT + 3))
#define NUM_BITS_PER_PAGE_MASK          (NUM_BITS_PER_PAGE - 1)
#define BITS_PER_LONG               32


//static int server_bitmap_setup(struct ebdr_user_bitmap *ubitmap);
//static void server_bitmap_cleanup(struct ebdr_user_bitmap *ubitmap){}
int get_total_bits_set(unsigned long int *);
int ebdr_check_bit_set(unsigned long *, int);
int ebdr_test_bit2(int, char *);

#endif
