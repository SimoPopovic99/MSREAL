#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
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

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver for deskew ouvput");
#define DEVICE_NAME "deskew"
#define DRIVER_NAME "deskew_driver"

//*************************************************************************
static int deskew_probe(struct platform_device *pdev);
static int deskew_open(struct inode *i, struct file *f);
static int deskew_close(struct inode *i, struct file *f);
static ssize_t deskew_read(struct file *f, char __user *buf, size_t len, loff_t *off);
static ssize_t deskew_write(struct file *f, const char __user *buf, size_t count, loff_t *off);
static int __init deskew_init(void);
static void __exit deskew_exit(void);
static int deskew_remove(struct platform_device *pdev);


static char chToUpper(char ch);
static unsigned long strToInt(const char* pStr, int len, int base);

//*********************GLOBAL VARIABLES*************************************
static struct file_operations deskew_fops =
{
    .owner = THIS_MODULE,
    .open = deskew_open,
    .release = deskew_close,
    .read = deskew_read,
    .write = deskew_write
};
static struct of_device_id deskew_of_match[] = {
    { .compatible = "xlnx,gpio_bram_control", },
    { /* end of list */ },
};

static struct platform_driver deskew_driver = {
    .driver = {
	.name = DRIVER_NAME,
	.owner = THIS_MODULE,
	.of_match_table	= deskew_of_match,
    },
    .probe		= deskew_probe,
    .remove	= deskew_remove,
};

struct deskew_info {
    unsigned long mem_start;
    unsigned long mem_end;
    void __iomem *base_addr;
};

static struct deskew_info *vp = NULL;

MODULE_DEVICE_TABLE(of, deskew_of_match);

static struct cdev c_dev;
static dev_t first;
static struct class *cl;
static int int_cnt;

//***************************************************
// PROBE AND REMOVE

static int deskew_probe(struct platform_device *pdev)
{
    struct resource *r_mem;
    int rc = 0;

    printk(KERN_INFO "Probing\n");

    r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!r_mem) {
	printk(KERN_ALERT "invalid address\n");
	return -ENODEV;
    }
    vp = (struct deskew_info *) kmalloc(sizeof(struct deskew_info), GFP_KERNEL);
    if (!vp) {
	printk(KERN_ALERT "Cound not allocate deskew device\n");
	return -ENOMEM;
    }

    vp->mem_start = r_mem->start;
    vp->mem_end = r_mem->end;


    if (!request_mem_region(vp->mem_start,vp->mem_end - vp->mem_start + 1, DRIVER_NAME))
    {
	printk(KERN_ALERT "Couldn't lock memory region at %p\n",(void *)vp->mem_start);
	rc = -EBUSY;
	goto error1;
    }
    else {
	printk(KERN_INFO "deskew_init: Successfully allocated memory region for deskew\n");
    }
    /* 
     * Map Physical address to Virtual address
     */

    vp->base_addr = ioremap(vp->mem_start, vp->mem_end - vp->mem_start + 1);
    if (!vp->base_addr) {
	printk(KERN_ALERT "deskew: Could not allocate iomem\n");
	rc = -EIO;
	goto error2;
    }

    printk("probing done");
error2:
    release_mem_region(vp->mem_start, vp->mem_end - vp->mem_start + 1);
error1:
    return rc;

}

static int deskew_remove(struct platform_device *pdev)
{
    // Exit Device Module
    iounmap(vp->base_addr);
    //release_mem_region(vp->mem_start, vp->mem_end - vp->mem_start + 1);
    return 0;
}

//***************************************************
// IMPLEMENTATION OF FILE OPERATION FUNCTIONS

static int deskew_open(struct inode *i, struct file *f)
{
    //printk("deskew opened\n");
    return 0;
}
static int deskew_close(struct inode *i, struct file *f)
{
    //printk("deskew closed\n");
    return 0;
}
static ssize_t deskew_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    //printk("deskew read\n");
    return 0;
}
static ssize_t deskew_write(struct file *f, const char __user *buf, size_t count, loff_t *off)
{	
    char buffer[count];
    char *lp='\0';
    char *rp='\0';
    int i = 0;
    unsigned int xpos=0,ypos=0,rgb=0;
    i = copy_from_user(buffer, buf, count);
    buffer[count - 1] = '\0';

    //extract position on x axis 
    lp = buffer;
    rp = strchr(lp,',');
    if(!rp)
    {
	printk("Invalid input, expected format: xpos,ypos,rgb\n");
	return count;
    }
    *rp = '\0';
    rp++;

    if(lp[0]=='0' && lp[1]=='x')
    {
	lp=lp+2;
	xpos = strToInt(lp, strlen(lp), 16);
    }
    else
	xpos = strToInt(lp, strlen(lp), 10);

    //extract position on y axis 
    lp = rp;
    rp = strchr(lp,',');
    if(!rp)
    {
	printk("Invalid input, expected format: xpos,ypos,rgb\n");
	return count;
    }
    *rp = '\0';
    rp++;

    if(lp[0]=='0' && lp[1]=='x')
    {
	lp=lp+2;
	ypos = strToInt(lp, strlen(lp), 16);
    }
    else
	ypos = strToInt(lp, strlen(lp), 10);

    //extract rgb(red,green,blue) value of pixel 
    lp = rp;
    if(!lp)
    {
	printk("Invalid input, expected format: xpos,ypos,rgb\n");
	return count;
    }
    if(lp[0]=='0' && lp[1]=='x')
    {
	lp=lp+2;
	rgb = strToInt(lp, strlen(lp), 16);
    }
    else
	rgb = strToInt(lp, strlen(lp), 10);

	
    if (xpos>=256 || ypos>=144)
    {
	printk("position of pixel is out of bounds\n");
	return count;
    }

    //printk("X %d Y %d ADDR %d V %d \n",xpos,ypos,(256*ypos + xpos)*2,rgb);

    iowrite32((256*ypos + xpos)*4, vp->base_addr + 8);
    iowrite32(rgb, vp->base_addr);



    //printk("Sucessfull write \n");
    return count;

}

//***************************************************
// HELPER FUNCTIONS (STRING TO INTEGER)


static char chToUpper(char ch)
{
    if((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
    {
	return ch;
    }
    else
    {
	return ch - ('a'-'A');
    }
}

static unsigned long strToInt(const char* pStr, int len, int base)
{
    //                      0,1,2,3,4,5,6,7,8,9,:,;,<,=,>,?,@,A ,B ,C ,D ,E ,F
    static const int v[] = {0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,0,10,11,12,13,14,15};
    int i   = 0;
    unsigned long val = 0;
    int dec = 1;
    int idx = 0;

    for(i = len; i > 0; i--)
    {
	idx = chToUpper(pStr[i-1]) - '0';

	if(idx > sizeof(v)/sizeof(int))
	{
	    printk("strToInt: illegal character %c\n", pStr[i-1]);
	    continue;
	}

	val += (v[idx]) * dec;
	dec *= base;
    }

    return val;
}


//***************************************************
// INIT AND EXIT FUNCTIONS OF THE DRIVER

static int __init deskew_init(void)
{

    int_cnt = 0;

    printk(KERN_INFO "deskew_init: Initialize Module \"%s\"\n", DEVICE_NAME);

    if (alloc_chrdev_region(&first, 0, 1, "deskew_region") < 0)
    {
	printk(KERN_ALERT "<1>Failed CHRDEV!.\n");
	return -1;
    }
    printk(KERN_INFO "Succ CHRDEV!.\n");

    if ((cl = class_create(THIS_MODULE, "chardrv")) == NULL)
    {
	printk(KERN_ALERT "<1>Failed class create!.\n");
	goto fail_0;
    }
    printk(KERN_INFO "Succ class chardev1 create!.\n");
    if (device_create(cl, NULL, MKDEV(MAJOR(first),0), NULL, "deskew") == NULL)
    {
	goto fail_1;
    }

    printk(KERN_INFO "Device created.\n");

    cdev_init(&c_dev, &deskew_fops);
    if (cdev_add(&c_dev, first, 1) == -1)
    {
	goto fail_2;
    }

    printk(KERN_INFO "Device init.\n");

    return platform_driver_register(&deskew_driver);

fail_2:
    device_destroy(cl, MKDEV(MAJOR(first),0));
fail_1:
    class_destroy(cl);
fail_0:
    unregister_chrdev_region(first, 1);
    return -1;

} 

static void __exit deskew_exit(void)  		
{

    platform_driver_unregister(&deskew_driver);
    cdev_del(&c_dev);
    device_destroy(cl, MKDEV(MAJOR(first),0));
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    printk(KERN_INFO "deskew_exit: Exit Device Module \"%s\".\n", DEVICE_NAME);
}

module_init(deskew_init);
module_exit(deskew_exit);

MODULE_AUTHOR ("FTN");
MODULE_DESCRIPTION("Test Driver for deskew output.");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("custom:deskew");