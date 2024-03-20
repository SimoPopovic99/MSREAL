#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/io.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/semaphore.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Deskew driver");


#define DEVICE_NAME "deskew" 
#define DRIVER_NAME "deskew_driver"


//buffer size
//#define BUFF_SIZE 784

//addresses for registers
#define START	0x00
#define READY	0x04

//*************************************************************************
static int deskew_probe(struct platform_device *pdev);
static int deskew_open(struct inode *i, struct file *f);
static int deskew_close(struct inode *i, struct file *f);
static ssize_t deskew_read(struct file *f, char __user *buf, size_t len, loff_t *off);
static ssize_t deskew_write(struct file *f, const char __user *buf, size_t count, loff_t *off);

static int __init deskew_init(void);
static void __exit deskew_exit(void);
static int deskew_remove(struct platform_device *pdev);

unsigned int block_type, ch, gr;
unsigned int ready, start;
int done;
int endRead = 0;

//DECLARE_WAIT_QUEUE_HEAD(readQ); ne znam za sta je ovo
//DECLARE_WAIT_QUEUE_HEAD(writeQ);

struct file_operations my_fops =
{
	.owner = THIS_MODULE,
	.open = deskew_open,
	.read = deskew_read,
	.write = deskew_write,
	.release = deskew_close,
};


static struct of_device_id device_of_match[] = {
	{ .compatible = "xlnx,Deskew-1.0", },
	{ .compatible = "xlnx,axi-bram-ctrl-4.1", }, 
	{ /* end of list */ },
};

static struct platform_driver my_driver = {
    .driver = {
	.name = DRIVER_NAME,
	.owner = THIS_MODULE,
	.of_match_table	= device_of_match,
    },
    .probe		= deskew_probe,
    .remove	= deskew_remove,
};

struct device_info {
    unsigned long mem_start;
    unsigned long mem_end;
    void __iomem *base_addr;
};

static struct device_info *deskew = NULL;
static struct device_info *bram = NULL;

MODULE_DEVICE_TABLE(of, device_of_match);

static dev_t my_dev_id;
static struct class *my_class;
static struct cdev *my_cdev;
//NADAM SE DA JE DOVDE DOBRO
//********************************************************************************
//			PROBE & REMOVE
//********************************************************************************

int device_fsm = 0;

static int deskew_probe(struct platform_device *pdev)
{
    
    struct resource *r_mem;
    int rc = 0;

    printk(KERN_INFO "Probing\n");

    r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!r_mem) {
	printk(KERN_ALERT "invalid address\n");
	return -ENODEV;
    } //OVAJ DEO JE VIMA ISTI 
	
	
	switch (device_fsm)
    {
		case 0: // device deskew
		  deskew = (struct device_info *) kmalloc(sizeof(struct device_info), GFP_KERNEL);
		  if (!deskew)
			{
			  printk(KERN_ALERT "Could not allocate deskew device\n");
			  return -ENOMEM;
			}
		  deskew->mem_start = r_mem->start;
		  deskew->mem_end   = r_mem->end; 
		  if(!request_mem_region(deskew->mem_start, deskew->mem_end - deskew->mem_start+1, DRIVER_NAME))
			{
			  printk(KERN_ALERT "Couldn't lock memory region at %p\n",(void *)deskew->mem_start);
			  rc = -EBUSY;
			  goto error1;
			}
		  deskew->base_addr = ioremap(deskew->mem_start, deskew->mem_end - deskew->mem_start + 1);
		  if (!deskew->base_addr)
			{
			  printk(KERN_ALERT "[PROBE]: Could not allocate deskew iomem\n");
			  rc = -EIO;
			  goto error2;
			}
      ++device_fsm; 
		  printk(KERN_INFO "[PROBE]: Finished probing deskew.\n");
		  return 0;
		  error2:
			release_mem_region(deskew->mem_start, deskew->mem_end - deskew->mem_start + 1);
		  error1:
			return rc;
		  break;
		  		case 1: // device bram
		  bram = (struct device_info *) kmalloc(sizeof(struct device_info), GFP_KERNEL);
		  if (!bram)
			{
			  printk(KERN_ALERT "Could not allocate bram device\n");
			  return -ENOMEM;
			}
		  bram->mem_start = r_mem->start;
		  bram->mem_end   = r_mem->end;
		  if(!request_mem_region(bram->mem_start, bram->mem_end - bram->mem_start+1, DRIVER_NAME))
			{
			  printk(KERN_ALERT "Couldn't lock memory region at %p\n",(void *)bram->mem_start);
			  rc = -EBUSY;
			  goto error3;
			}
		  bram->base_addr = ioremap(bram->mem_start, bram->mem_end - bram->mem_start + 1);
		  if (!bram->base_addr)
			{
			  printk(KERN_ALERT "[PROBE]: Could not allocate bram iomem\n");
			  rc = -EIO;
			  goto error4;
			}
		  printk(KERN_INFO "[PROBE]: Finished probing bram.\n");
		  return 0;
		  error4:
			release_mem_region(bram->mem_start, bram->mem_end - bram->mem_start + 1);
		  error3:
			return rc;
		  break;
          default:
		  printk(KERN_INFO "[PROBE] Device FSM in illegal state. \n");
		  return -1;
	}
  printk(KERN_INFO "Succesfully probed driver\n");
  return 0;
}
	
static int deskew_remove(struct platform_device *pdev)
{
  
  switch (device_fsm)
    {
    case 0: //device deskew
      printk(KERN_ALERT "deskew device platform driver removed\n");
      iowrite32(0, deskew->base_addr);
      iounmap(deskew->base_addr);
      release_mem_region(deskew->mem_start, deskew->mem_end - deskew->mem_start + 1);
      kfree(deskew);
      break;

    case 1: //device bram
      printk(KERN_ALERT "bram device platform driver removed\n");
      iowrite32(0, bram->base_addr);
      iounmap(bram->base_addr);
      release_mem_region(bram->mem_start, bram->mem_end - bram->mem_start + 1);
      kfree(bram);
      --device_fsm;
      break;
     default:
      printk(KERN_INFO "[REMOVE] Device FSM in illegal state. \n");
      return -1;
    }
  printk(KERN_INFO "Succesfully removed driver\n");
  return 0;
}

//********************************************************
// OPEN & CLOSE
//********************************************************

static int deskew_open(struct inode *i, struct file *f)
{
    printk("deskew opened\n");
    return 0;
}
static int deskew_close(struct inode *i, struct file *f)
{
    printk("deskew closed\n");
    return 0;
}

//*********************************************************
//	READ & WRITE
//*********************************************************
