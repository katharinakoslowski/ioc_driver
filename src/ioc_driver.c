#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>               
#include <linux/uaccess.h>              
#include <linux/err.h>
#include <linux/workqueue.h> 
#include <linux/delay.h>
#include <linux/mutex.h>
#include "ioc_driver.h"

int ioc_major       = IOC_MAJOR;
int ioc_dev_count   = IOC_DEV_COUNT;
int ioc_timer       = IOC_TIMER;
int ioc_word_dlm    = IOC_WORD_DLM;	

module_param(ioc_major, int, 0);
module_param(ioc_dev_count, int, 0);
module_param(ioc_timer, int, 0);
module_param(ioc_word_dlm, int, 0);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Katharina Koslowski");
MODULE_DESCRIPTION("Demo char driver for text input / output");

/********** API FUNCTION PROTOTYPES **********/
static int      __init ioc_driver_init(void);
static void     __exit ioc_driver_exit(void);
static int      ioc_open(struct inode *p_inode, struct file *p_filp);
static int      ioc_release(struct inode *p_inode, struct file *p_filp);
static ssize_t  ioc_read(struct file *p_filp, char __user *p_buf, size_t p_len, loff_t *p_off);
static ssize_t  ioc_write(struct file *p_filp, const char __user *p_buf, size_t p_len, loff_t *p_off);

/********** HELPER FUNCTION PROTOTYPES **********/
static int      ioc_setup_cdev(struct ioc_dev *p_dev, int p_index);
static void     ioc_workqueue_handler(struct work_struct *p_work); 

/********** DECLARATIONS / DEFINITIONS **********/
static struct class             *dev_class;
static struct workqueue_struct  *workqueue;
struct ioc_dev                  *ioc_devices;
LIST_HEAD(Head_Node);

struct file_operations ioc_fops = 
{
	.owner          = THIS_MODULE,
    .read           = ioc_read,
    .write          = ioc_write,
    .open           = ioc_open,
    .release        = ioc_release,
};

static void ioc_workqueue_handler(struct work_struct *p_work)
{
    // TODO clean up
    struct ioc_dev *dev;
    int workpos = 0; 
    int datalen = 0;
    dev = container_of(p_work, struct ioc_dev, work);
    pr_debug("Enter workqueue in ioc_device%d", MINOR(dev->cdev.dev));
    do
    {
        if(mutex_trylock(&dev->mutex))
            continue;

        datalen = strlen(dev->data);
        if (dev->pos == workpos && workpos < datalen) // data ok
        {
            if (ioc_word_dlm <= 0) // get next char
            {
                pr_info("'%c' in ioc_device%d", dev->data[dev->pos], MINOR(dev->cdev.dev));
                workpos += 1;
            }
            else // get next word
            {
                char* res = (char*)kmalloc(datalen - workpos, GFP_KERNEL);
                for (; workpos < datalen; workpos++)
                { 
                    if (dev->data[workpos] != ' ')
                    {
                        res[workpos - dev->pos] = dev->data[workpos];
                    }
                    else // delimiter, increment workpos
                    {
                        workpos += 1;
                        break;
                    }
                }
                pr_info("\"%s\" in ioc_device%d", res, MINOR(dev->cdev.dev));// " in ioc_device%d", MINOR(dev->cdev.dev));
                kfree(res);
            }
            dev->pos = workpos;
            mutex_unlock(&dev->mutex);
            msleep(ioc_timer);
        }
        else // data changed / end of data
        {
            mutex_unlock(&dev->mutex);
            break;
        }
    } while (1);
    pr_debug("Worker done in ioc_device%d", MINOR(dev->cdev.dev));
}

static int ioc_setup_cdev(struct ioc_dev *p_dev, int p_index)
{
    pr_debug("Setting up ioc_device%d", p_index);
	int err, devno = MKDEV(ioc_major, p_index);
    cdev_init(&p_dev->cdev, &ioc_fops);
	p_dev->cdev.owner = THIS_MODULE;
	p_dev->cdev.ops = &ioc_fops;
	err = cdev_add(&p_dev->cdev, devno, 1);
	if (err)
    {
        pr_err("Error %d adding ioc_device%d", err, p_index);
    }
    return err;	
}

static int ioc_open(struct inode *p_inode, struct file *p_filp)
{
    // TODO: check file flags 
    pr_debug("Opening ioc_device%d.", iminor(file_inode(p_filp)));
	struct ioc_dev *dev; 
	dev = container_of(p_inode->i_cdev, struct ioc_dev, cdev);
	p_filp->private_data = dev;
	return 0; 
}

static int ioc_release(struct inode *p_inode, struct file *p_filp)
{
    pr_debug("Closing ioc_device%d.", iminor(file_inode(p_filp)));
    return 0;
}

static ssize_t ioc_read(struct file *p_filp, char __user *p_buf, size_t p_len, loff_t *p_off)
{ 
    // TODO clean up
    pr_debug("Reading from ioc_device%d.", iminor(file_inode(p_filp)));
    struct ioc_dev *dev; 
    size_t len;
	dev = (struct ioc_dev*)p_filp->private_data;

    mutex_lock(&dev->mutex);
    len = dev->data == NULL ? 0 : strlen(dev->data);
    if (len == 0 || *p_off >= len)
    {
        mutex_unlock(&dev->mutex);
        return 0; // no data / end of file
    }
    p_len = (p_len > len - *p_off) ?  len - *p_off : p_len; // calcute remaining bytes to read
    if (copy_to_user(p_buf, dev->data, p_len) != 0)
    {
        pr_err("Failed to read from ioc_device%d.", iminor(file_inode(p_filp)));
        mutex_unlock(&dev->mutex);
        return -EFAULT;
    }
    *p_off += p_len;
    mutex_unlock(&dev->mutex);
    return p_len; 
}

static ssize_t ioc_write(struct file *p_filp, const char __user *p_buf, size_t p_len, loff_t *p_off)
{
    // TODO check p_len, p_off
    // TODO clean up
    pr_debug("Writing in ioc_device%d.", iminor(file_inode(p_filp)));
    struct ioc_dev *dev; 
	dev = (struct ioc_dev*)p_filp->private_data;
    mutex_lock(&dev->mutex);
    dev->pos = 0;

    if (p_len > 0 && dev->data != NULL) // free data memory
    {
        kfree(dev->data);
        dev->data = NULL;
    } 

    if (p_len > 0 && dev->data == NULL) // alloc data memory
    {
        dev->data = (char*)kmalloc(p_len, GFP_KERNEL);
    }
    else // no bytes to write
    {
        mutex_unlock(&dev->mutex);
        return -1; 
    }

    if (dev->data == NULL) 
    {
        pr_err("Failed to allocate memory in ioc_device%d.", iminor(file_inode(p_filp)));
        mutex_unlock(&dev->mutex);
        return -ENOMEM;
    }

    if (copy_from_user(dev->data, p_buf, p_len) != 0) 
    {
        pr_err("Failed to write in ioc_device%d.", iminor(file_inode(p_filp)));
        mutex_unlock(&dev->mutex);
        return -EFAULT;
    }

    dev->data[p_len - 1] = 0x00;
    mutex_unlock(&dev->mutex);
    queue_work(workqueue, &dev->work);
    return p_len;
}

static int __init ioc_driver_init(void)
{
    // TODO clean up
    pr_info("Initializing ioc_device driver.");
    int result, i;
	dev_t dev = MKDEV(ioc_major, 0);
	
	if (ioc_major) // register major
    {
		result = register_chrdev_region(dev, ioc_dev_count, "ioc");
    }
    else // register dynamic major
    {
		result = alloc_chrdev_region(&dev, 0, ioc_dev_count, "ioc");
		ioc_major = MAJOR(dev);
	}
	if (result < 0) 
    {
        pr_err("Failed to register major for ioc_device driver.");
        return result;
    }

	dev_class = class_create("ioc_class");
    if (IS_ERR(dev_class))
    {
        pr_err("Failed to create ioc_class.");
        class_destroy(dev_class);
        return -EFAULT;
    }	
	
    for (i = 0; i < ioc_dev_count; i++) // alloc devices
    {
	    struct ioc_dev *temp_node;  
        dev_t devno;
        temp_node = kmalloc(sizeof(struct ioc_dev), GFP_KERNEL);
        if (temp_node == NULL) 
        {
            pr_err("Failed to create ioc_device%d.", i);
            return -ENOMEM;
        }
        temp_node->data = NULL; // no data yet
        INIT_WORK(&temp_node->work, ioc_workqueue_handler);
        INIT_LIST_HEAD(&temp_node->list);
        list_add_tail(&temp_node->list, &Head_Node);
        mutex_init(&temp_node->mutex);
        ioc_setup_cdev(temp_node, i);
        devno = MKDEV(ioc_major, i);
        if(IS_ERR(device_create(dev_class,NULL,devno,NULL,"ioc_device%d", i)))
        {
            pr_err("Cannot create the Device\n");
            unregister_chrdev_region(devno,1);
            return -EFAULT;
        }
        pr_info("Loaded ioc_device%d.", i);
	}
    workqueue = create_workqueue("workqueue");
    pr_info("Initialized %d device drivers.", ioc_dev_count);
    return 0;
}

static void __exit ioc_driver_exit(void)
{
    struct ioc_dev *cursor, *temp;
    destroy_workqueue(workqueue);
	list_for_each_entry_safe(cursor, temp, &Head_Node, list) 
    {
        int minor = MINOR(cursor->cdev.dev);
        if (cursor->data != NULL)
        {
            kfree(cursor->data);
        }
        list_del(&cursor->list);
        kfree(cursor);
        unregister_chrdev_region(cursor->cdev.dev, 1);
        device_destroy(dev_class, cursor->cdev.dev);
        cdev_del(&cursor->cdev);
        pr_info("Unloaded ioc_device%d.", minor);
    }
    class_destroy(dev_class);
    unregister_chrdev_region(MKDEV(ioc_major, 0), ioc_dev_count);
    pr_info("Unloaded ioc_driver.");
}
 
module_init(ioc_driver_init);
module_exit(ioc_driver_exit);
 
