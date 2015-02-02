#ifndef __ATDR_SERVER_BITMAP_H
#define __ATDR_SERVER_BITMAP_H

//static int server_bitmap_setup(struct atdr_user_bitmap *ubitmap);
//static void server_bitmap_cleanup(struct atdr_user_bitmap *ubitmap){}
int get_total_bits_set(unsigned long int *);
int atdr_check_bit_set(unsigned long *, int);
int atdr_test_bit2(int, char *);

#endif
