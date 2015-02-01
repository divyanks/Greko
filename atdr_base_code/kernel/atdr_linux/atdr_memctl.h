
#define pgalloc_getvmpgoff(vma)			((vma)->vm_pgoff)
#define MAX_LENGTH                      	4096

#define EBDR_KERNEL_MEMCTL_DEVICE   		"ebdr_memctrl"
#define EBDR_MAX_KMALLOCS_PER_DEVICE  	 	4
#define PLKT_MAX_KMALLOCS         		1
#define PLKT_MAX_KMALLOC_BYTES  		(PLKT_MAX_KMALLOCS*4*4096)   
#define PLKT_REQUIRED_KMALLOCS               	1  

#define pgalloc_kmallocpagetobusaddr(x) 	(__pa(x)>>PAGE_SHIFT)
#define pgalloc_kmallocpageinccount(x) 		get_page( virt_to_page(x) )
#define pgalloc_busaddrtopage(x) 		pfn_to_page(x)
#define pgalloc_getvmoffset(vma) 		((vma)->vm_pgoff<<PAGE_SHIFT)
#define pgalloc_kmallocpagedeccount(x) 		put_page_testzero( virt_to_page(x) )

#define EBDR_TRACE_LOG_SIZE                 	4*4096  

//long unsigned int vmalloc_area_sz;

/* ebdr_memctl */
extern char *ebdr_device_buf;
extern char *plkt_memory;

extern unsigned long ebdr_devmem_size;
extern char kernel_memctl_device_name[20];


