#include <linux/cdev.h>

#define IOC_MAJOR       0       /* dynamic major by default */
#define IOC_DEV_COUNT   4       /* 4 devices by default */
#define IOC_TIMER       1000    /* output timer for log */
#define IOC_WORD_DLM    1       /* word delimiter (separates by whitespace if > 0) */

/**
 * struct ioc_dev - ioc device data structure.
 * @list: list item
 * @pos: current log position in data
 * @data: char data
 * @cdev: cdev data
 * @work: work queue item
 * @mutex: mutex
 * 
 */
struct ioc_dev 
{
    struct list_head    list;  
    size_t              pos; 
    char                *data;  
	struct cdev         cdev;
    struct work_struct  work; 
    struct mutex        mutex;
};

/**
 * Linked list of char devices.
 */
extern struct ioc_dev           *ioc_devices;
/**
 * File operations for ioc_dev.
 */
extern struct file_operations   ioc_fops;
/* 
* Module parameter: major number. 
*/ 
extern int ioc_major; 
/* 
* Module parameter: number of devices. 
*/ 
extern int ioc_dev_count; 
/* 
* Module parameter: output timer. 
*/
extern int ioc_timer; 
/* 
* Module parameter: word delimiter. 
*/
extern int ioc_word_dlm; 