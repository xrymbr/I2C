#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/log2.h>
#include <linux/bitops.h>
#include <linux/jiffies.h>
#include <linux/of.h>
#include <linux/acpi.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>

#define IOC_AT24C02_READ 100
#define IOC_AT24C02_WRITE 101
#define IOC_AT24C02_NREAD 102
#define IOC_AT24C02_NWRITE 103


static int major = 0;
static struct class *at24c02_class;
struct i2c_client *at24c02_client;

static long at24c02_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct i2c_msg msgs[2];

	unsigned char addr;	
	unsigned char number;
	unsigned char data;
    unsigned int *usr_buf = (unsigned int *)arg;
    unsigned int ker_buf[30];
	unsigned char byte_buf[30]; 
    unsigned char kern_buf[30];
	int i;
    
    if(copy_from_user(ker_buf,usr_buf,sizeof(usr_buf)))
        return -EFAULT;
        

	/* buf[0]大小,buf[1]地址 */
    number = ker_buf[0];
	addr   = ker_buf[1];

	for(i = 0;i < number + 1;i++)
		byte_buf[i] = ker_buf[i+1];

    switch(cmd)
    {
		/* 读N个数据 */
        case IOC_AT24C02_NREAD:
        {
            msgs[0].addr  = at24c02_client->addr;
            msgs[0].flags = 0;
            msgs[0].len   = 1;
            msgs[0].buf   = &addr;

            msgs[1].addr  = at24c02_client->addr;
            msgs[1].flags = I2C_M_RD;
            msgs[1].len   = number;
            msgs[1].buf   = kern_buf;

            i2c_transfer(at24c02_client->adapter,msgs,2);


            if(copy_to_user(usr_buf,kern_buf,sizeof(kern_buf)))
                return -EFAULT;

            break;
        }
		
		/* 读数据 */
        case IOC_AT24C02_READ:
        {
            msgs[0].addr  = at24c02_client->addr;
            msgs[0].flags = 0;
            msgs[0].len   = 1;
            msgs[0].buf   = &number;

            msgs[1].addr  = at24c02_client->addr;
            msgs[1].flags = I2C_M_RD;
            msgs[1].len   = 1;
            msgs[1].buf   = &data;

            i2c_transfer(at24c02_client->adapter,msgs,2);

            ker_buf[1] = data;

            if(copy_to_user(usr_buf,ker_buf,8))
                return -EFAULT;

            break;
        }
		
        /* 写数据 */
        case IOC_AT24C02_WRITE:
        {
            byte_buf[0] = number;
            byte_buf[1] = addr;

            msgs[0].addr  = at24c02_client->addr;
            msgs[0].flags = 0;
            msgs[0].len   = 2;
            msgs[0].buf   = byte_buf;
                 
			i2c_transfer(at24c02_client->adapter, msgs, 1);

            mdelay(20);
			
			break;
      
        }

		 /* 写N个数据 */
        case IOC_AT24C02_NWRITE:
        {
            //byte_buf[0] = addr;
            //byte_buf[1] = ker_buf[1];

            msgs[0].addr  = at24c02_client->addr;
            msgs[0].flags = 0;
            msgs[0].len   = number+1;
            msgs[0].buf   = byte_buf;
                 
			i2c_transfer(at24c02_client->adapter, msgs, 1);

            mdelay(20);
			
			break;
      
        }
    }
    
    return 0;
}


static const struct file_operations at24c02_fops = {
	.owner   = THIS_MODULE,
    .unlocked_ioctl =   at24c02_ioctl,
};



static int at24c02_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

    at24c02_client = client;

    major = register_chrdev(0,"at24c02test",&at24c02_fops);

    at24c02_class = class_create(THIS_MODULE,"at24c02_class");

    device_create(at24c02_class,NULL,MKDEV(major, 0), NULL, "lc_at24c02");
    
    printk("at24_probe is ok\n");
    return 0;
}

static int at24c02_remove(struct i2c_client *client)
{
    device_destroy(at24c02_class,MKDEV(major, 0));
    class_destroy(at24c02_class);
    unregister_chrdev(major,"at24c02test");
    
    return 0;
}

static const struct of_device_id at24c02_of_match[] = {
	{.compatible = "at24c02test"},
	{}
};

static const struct i2c_device_id at24c02_ids[] = {
	{ "xxxxyyy",	(kernel_ulong_t)NULL },
	{ /* END OF LIST */ }
};


static struct i2c_driver at24c02_driver = {
	.driver = {
		.name = "at24c02",
		.of_match_table = at24c02_of_match,
	},
	.probe = at24c02_probe,
	.remove = at24c02_remove,
	.id_table = at24c02_ids,
};

static int __init at24c02_init(void)
{
	return i2c_add_driver(&at24c02_driver);
}
static void __exit at24c02_exit(void)
{
    i2c_del_driver(&at24c02_driver);
}


module_init(at24c02_init);
module_exit(at24c02_exit);
MODULE_LICENSE("GPL");












