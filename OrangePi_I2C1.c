/*
 * Copyright@2015 Buddy.Zhang
 */

/*
 * The PCF8591 is a single-chip,single-supply low power 8-bit COMS data 
 * acquisition device with four analog inputs,one analog output and a 
 * serial I2C-bus interface.Three address pins A0,A1 and A2 are used for 
 * programming the hardware address,allowing the use of up to eight 
 * devices connect to the I2C-bus without additional hardware.Address,
 * control and data to and from the device are transferred serially via 
 * the two-line bidirectional I2C-bus.The function of the device include 
 * analog input muliplexing,on-chip track and hold function,8-bit 
 * analog-to-digital conversion and an 8-bit digital to analog conversion.
 * The maximum conversion rate is given by the maximum speed of the 
 * I2C-bus.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/slab.h>

/*
 * Addressing
 * Each PCF8591 device in an I2C-bus system is activated by sending a valid
 * address to the deivce.The address consists of a fixed part and a program
 * -mable part.The programmable part must be set according to the address
 * pins A0,A1 and A2.The address always has to be sent as the first byte af
 * -ter the start condition in the I2C-bus protocol.The last bit of the adr
 * -ess byte is the read/write-bit which sets the direction of the following 
 * data transfer.
 */
#define DEV_NAME        "I2C1P"
/*
 * A0,A1 and A2 connect GND,so address are 1001 000X.
 */
#define I2C_ADDRESS     0x48

#define DEBUG          1

static struct class *i2c_class;
static struct i2c_client *my_client;
static int i2c_major;

#if DEBUG
static void buddy_debug(struct i2c_client *client,char *name)
{
	printk(KERN_INFO "=====================%s=====================\n=>client->name:%s\n=>client->addr:%08x\n=>adapter->name:%s\n",
			name,client->name,client->addr,client->adapter->name);
}
#endif
/*
 * i2c_read
 */
static int buddy_i2c_read(struct i2c_client *client,
		char *buf,int len)
{
	int ret;
	struct i2c_msg msg[1];
	/* read initial */
	msg[0].addr    = client->addr;
	msg[0].flags   = client->flags | I2C_M_TEN;
	msg[0].flags  |= I2C_M_RD;
	msg[0].buf     = buf;
	msg[0].len     = len;

	ret = i2c_transfer(client->adapter,msg,2);
	if(ret != 2)
	{
		printk(KERN_INFO "Unable read data from i2c bus\n");
		return -1;
	}else
	{
		//printk(KERN_INFO "Succeed to read data from i2c bus\n");
		return 0;
	}
}
/*
 * i2c write
 * @ctr:control byte
 * @data:data for writing
 * @len:size of data
 */
static int buddy_i2c_write(struct i2c_client *client,char ctr,
		char *data,int len)
{	
	int ret;
	char tmp[len+1];
	struct i2c_msg msg;

	tmp[0] = ctr;
	for(ret = 1;ret <= len;ret++)
	{
		tmp[ret] = data[ret -1];
	}
	
	/* write initial */
	msg.addr    = client->addr;
	msg.flags   = client->flags | I2C_M_TEN;
	msg.buf     = tmp;
	msg.len     = len+1;

	ret = 0;
	ret = i2c_transfer(client->adapter,&msg,1);
	if(ret != 1)
	{
		printk(KERN_INFO "Unable to write data to i2c bus\n");
		return -1;
	}else
	{
		printk(KERN_INFO "Succeed to write data to i2c bus\n");
		return 0;
	}
}

static int hal_read(struct i2c_client *client, uint8_t addr, uint16_t *val)
{
	int ret = -1;
	char buf[2] = {0};

	/* first, send addr */
	ret = i2c_master_send(client, (char *)&addr, 1);
	if (ret < 0) {
		printk("HAL read send: error\n");
		return -1;
	}
	/* Second, receive two byte from device */
	ret = i2c_master_recv(client, buf, 2);
	if (ret < 0) {
		printk("HAL read recv: error\n");
		return -1;
	}

	*val = (uint16_t)(buf[0] << 8 | buf[1]);

	return 0;
}

/*
 * Hal write 
 */
static int hal_write(struct i2c_client *client, uint8_t addr, uint16_t val)
{
	int ret;
	char buf[3];

	buf[0] = addr;
	buf[1] = (char)(val >> 8);
	buf[2] = (char)(val & 0xFF);
	
	while (1)
		ret = i2c_master_send(client, buf, 3);
	if (ret < 0) {
		printk("Hal write send: error\n");
		return -1;
	}	

	return 0;
}

/*
 * Control byte
 * The second byte sent to a PCF8591 device will be stored in its control 
 * register and is required to control the device function.The upper nibble
 * of the control register is used for enabling the analog output,and for 
 * programming the analog inputs as single-ended or differential inputs.The
 * lower nibble selects one of the analog input channels defined by the upp
 * -er nibble.If the auto-increment flag is set,the channel number is incre
 * -mented automatically after each A/D conversion.
 * If the auto-increment mode is desired in applications where the internal
 * oscillator is used,the analog output enable flag in the control byte sho
 * -uld be set.This allows the internal oscillator to run continously,there
 * -by preventing conversion error resulting from oscillator start-up delay
 * The analog output enable flag may be reset at other times to reduce quie
 * -scent power consumption.
 * The selection of a non-existing input channel results in the highest a 
 * available channel number being allocated.Therefore,if the auto-increment
 * flag is set,the next selected channel will be always channel 0.The most
 * significant bits of both nibbles are reserved for future functions and 
 * have to be set to logic 0.After a Power-on reset condition all bits of
 * the control register are reset to logic 0.The D/A converter and the osc
 * -illator are disabled for power saving.The analog output is switched to 
 * a high-impedance state.
 */
/*
 * Choose A/D channel
 */
static int PCF8591_Choose_Channel(int ch,int *cmd)
{
	if(ch > 3 || ch < 0)
	{
		printk(KERN_INFO "PCF8591:Unable Match Channel\n");
		return -1;
	}
	(*cmd) = (*cmd) & 0xFC;
	switch(ch)
	{
		/*
		 * channel 0
		 */
		case 0:
			(*cmd) = (*cmd) | 0x00;
			break;
		/*
		 * channel 1
		 */
		case 1:
		    (*cmd) = (*cmd) | 0x01;
			break;
		/*
		 * channel 2
		 */
		case 2:
			(*cmd) = (*cmd) | 0x02;
			break;
		/*
		 * channel 3
		 */
		case 3:
			(*cmd) = (*cmd) | 0x03;
			break;
		default:
			break;
	}
	return 0;
}
/*
 * Enable auto increament 
 */
static int PCF8591_Auto_Inc(int num,int *cmd)
{
	if(num > 1 || num < 0)
	{
		printk("PCF8591:Unable to set Auto-Increament\n");
		return -1;
	}
	(*cmd) = (*cmd) & 0xFB;
	switch(num)
	{
		/*
		 * Disable Auto incream
		 */
		case 0:
			(*cmd) = (*cmd) | 0x00;
			break;
		/*
		 * Enable Auto incream
		 */
		case 1:
			{
				(*cmd) = (*cmd) | 0x04;
				(*cmd) = (*cmd) | 0x40;
				break;
			}
	}
	return 0;
}
/*
 * Set Analogue input 
 */
static int PCF8591_Set_Input(int type,int *cmd)
{
	if(type > 3 || type < 0)
	{
		printk(KERN_INFO "PCF8591:Unable to set input type\n");
		return -1;
	}
	(*cmd) = (*cmd) & 0xCF;
	switch(type)
	{
		/*
		 * Four single-enable inputs
		 */
		case 0:
			break;
		/*
		 * Three differential inputs
		 */
		case 1:
			(*cmd) = (*cmd) | 0x10;
			break;
		/*
		 * Single-enable and differential mixed
		 */
		case 2:
			(*cmd) = (*cmd) | 0x20;
			break;
		/*
		 * Two differential inputs
		 */
		case 3:
			(*cmd) = (*cmd) | 0x30;
			break;
	}
	return 0;
}
/*
 * Enable analogue output 
 */
static int PCF8591_Enable_Output(int ch,int *cmd)
{
	if(ch > 1 || ch < 0)
	{
		printk(KERN_INFO "PCF8591:Unable enable output\n");
		return -1;
	}
	(*cmd) = (*cmd) & 0x7F;
	switch(ch)
	{
		/*
		 * Disable output
		 */
		case 0:
			(*cmd) = (*cmd) | 0x00;
			break;
		case 1:
			(*cmd) = (*cmd) | 0x80;
			break;
	}
	return 0;
}
/*
 * open device
 */
static int buddy_open(struct inode *inode,struct file *filp)
{
	printk(KERN_INFO ">>>>>open<<<<<\n");
	filp->private_data = my_client;

	return 0;
}
/*
 * =========================Register Operation=========================
 */
/*
 * User API
 */
static void Use_API(struct i2c_client *client)
{
	int cmd = 0;
	char ch;
	PCF8591_Choose_Channel(3,&cmd);
	PCF8591_Set_Input(0,&cmd);
	printk(KERN_INFO "PCF8591:Control word are[%lx]\n",cmd);
	while (1)
		hal_write(client, 0x20, 0x010);
	buddy_i2c_write(client,cmd,NULL,0);
	while(1)
	{
		buddy_i2c_read(client,&ch,1);
		printk(KERN_INFO "The data are[%d]\n",ch);
	}
}
static void Test(struct i2c_client *client)
{
	Use_API(client);
}
/*
 * release device
 */
static int buddy_release(struct inode *inode,struct file *filp)
{
	printk(KERN_INFO ">>>>>release<<<<<\n");
	filp->private_data = NULL;
	return 0;
}
/*
 * read device
 */
static ssize_t buddy_read(struct file *filp,char __user *buf,
		size_t count,loff_t *offset)
{
	struct i2c_client *client = filp->private_data;
#if DEBUG
	buddy_debug(client,"read");
	Test(client);
#endif
       
	return 0;
}
/*
 * file operations
 */
static struct file_operations buddy_fops = {
	.owner     = THIS_MODULE,
	.open      = buddy_open,
	.release   = buddy_release,
	.read      = buddy_read,
};
/*
 * device id table
 */
static struct i2c_device_id buddy_id[] = {
	{DEV_NAME,I2C_ADDRESS},
	{},
};
/*
 * device probe
 */
static int buddy_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int res;
	struct device *dev;
	my_client = client;
	printk(KERN_INFO ">>>>>>probe<<<<<<\n");
	dev = device_create(i2c_class,&my_client->dev,MKDEV(i2c_major,0),
			NULL,DEV_NAME);
	if(IS_ERR(dev))
	{
		res = PTR_ERR(dev);
		goto error;
	}
#if DEBUG
	buddy_debug(client,"probe");
#endif
	return 0;
error:
	return res;
}
/*
 * device remove
 */
static int buddy_remove(struct i2c_client *client)
{
	printk(KERN_INFO ">>>>remove<<<<\n");
	device_destroy(i2c_class,MKDEV(i2c_major,0));
	return 0;
}
/*
 * i2c_driver
 */
static struct i2c_driver buddy_driver = {
	.driver   = {
		.name = DEV_NAME,
	},
	.probe    = buddy_probe,
	.id_table = buddy_id,
	.remove   = buddy_remove,
};
/*
 * init module
 */
static __init int buddy_init(void)
{
	int ret;
	/*
	 * add i2c-board information
	 */
	struct i2c_adapter *adap;
	struct i2c_board_info i2c_info;

	adap = i2c_get_adapter(1);
	if(adap == NULL)
	{
		printk(KERN_INFO "Unable to get i2c adapter\n");
		ret = -EFAULT;
		goto out;
	}
	printk(KERN_INFO "Succeed to get i2c adapter[%s]\n",adap->name);
	memset(&i2c_info,0,sizeof(struct i2c_board_info));
	strlcpy(i2c_info.type,DEV_NAME,I2C_NAME_SIZE);
	i2c_info.addr = I2C_ADDRESS;

	my_client = i2c_new_device(adap,&i2c_info);
#if DEBUG
	buddy_debug(my_client,"init");
#endif
	if(my_client == NULL)
	{
		printk(KERN_INFO "Unable to get new i2c device\n");
		ret = -ENODEV;
		i2c_put_adapter(0);
		goto out;
	}
	i2c_put_adapter(adap);
	printk(KERN_INFO ">>>>>>>>module init<<<<<<<<<<<<\n");
	/*
	 * Register char device
	 */
	i2c_major = register_chrdev(0,DEV_NAME,&buddy_fops);
	if(i2c_major == 0)
	{
		printk(KERN_INFO "Unable to register driver into char-driver\n");
		ret = -EFAULT;
		goto out;
	}
	printk(KERN_INFO "Succeed to register driver,char-driver is %d\n",i2c_major);
	/*
	 * create device class
	 */
	i2c_class = class_create(THIS_MODULE,DEV_NAME);
	if(IS_ERR(i2c_class))
	{
		printk(KERN_INFO "Unablt to create class file\n");
		ret = PTR_ERR(i2c_class);
		goto out_unregister;
	}
	printk(KERN_INFO "Succeed to create class file\n");
	/*
	 * create i2c driver
	 */
	ret = i2c_add_driver(&buddy_driver);
	if(ret)
	{
		printk(KERN_INFO "Unable to add i2c driver\n");
		ret = -EFAULT;
		goto out_class;
	}
	printk(KERN_INFO "Succeed to add i2c driver\n");
	return 0;
out_class:
	class_destroy(i2c_class);
out_unregister:
	unregister_chrdev(i2c_major,DEV_NAME);
out:
	return ret;
}
/*
 * exit module
 */
static __exit void buddy_exit(void)
{
	printk(KERN_INFO ">>>>>>>exit<<<<<<<");
	i2c_del_driver(&buddy_driver);
	class_destroy(i2c_class);
	unregister_chrdev(i2c_major,DEV_NAME);
}

module_init(buddy_init);
module_exit(buddy_exit);

MODULE_AUTHOR("Buddy Zhang<51498122@qq.com>");
MODULE_DESCRIPTION(DEV_NAME);
MODULE_LICENSE("GPL");

