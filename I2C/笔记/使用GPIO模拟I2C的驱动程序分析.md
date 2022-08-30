## 使用GPIO模拟I2C的驱动程序分析

参考资料：

* i2c_spec.pdf
* Linux文档
  * `Linux-5.4\Documentation\devicetree\bindings\i2c\i2c-gpio.yaml`
  * `Linux-4.9.88\Documentation\devicetree\bindings\i2c\i2c-gpio.txt`
* Linux驱动源码
  * `Linux-5.4\drivers\i2c\busses\i2c-gpio.c`
  * `Linux-4.9.88\drivers\i2c\busses\i2c-gpio.c`

### 1. 回顾I2C协议

#### 1.1 硬件连接

I2C在硬件上的接法如下所示，主控芯片引出两条线SCL,SDA线，在一条I2C总线上可以接很多I2C设备，我们还会放一个上拉电阻（放一个上拉电阻的原因以后我们再说）。

![](E:/QRS/doc_and_source_for_drivers/IMX6ULL/doc_pic/04_I2C/pic/04_I2C/001_i2c_hardware_block.png)

#### 1.2 I2C信号

I2C协议中数据传输的单位是字节，也就是8位。但是要用到9个时钟：前面8个时钟用来传输8数据，第9个时钟用来传输回应信号。传输时，先传输最高位(MSB)。

* 开始信号（S）：SCL为高电平时，SDA山高电平向低电平跳变，开始传送数据。
* 结束信号（P）：SCL为高电平时，SDA由低电平向高电平跳变，结束传送数据。
* 响应信号(ACK)：接收器在接收到8位数据后，在第9个时钟周期，拉低SDA
* SDA上传输的数据必须在SCL为高电平期间保持稳定，SDA上的数据只能在SCL为低电平期间变化

I2C协议信号如下：

![image-20210220151524099](E:/QRS/doc_and_source_for_drivers/IMX6ULL/doc_pic/04_I2C/pic/04_I2C/009_i2c_signal.png)



#### 1.3 协议细节

* 如何在SDA上实现双向传输？
  主芯片通过一根SDA线既可以把数据发给从设备，也可以从SDA上读取数据，连接SDA线的引脚里面必然有两个引脚（发送引脚/接受引脚）。

* 主、从设备都可以通过SDA发送数据，肯定不能同时发送数据，怎么错开时间？
  在9个时钟里，
  前8个时钟由主设备发送数据的话，第9个时钟就由从设备发送数据；
  前8个时钟由从设备发送数据的话，第9个时钟就由主设备发送数据。
* 双方设备中，某个设备发送数据时，另一方怎样才能不影响SDA上的数据？
  设备的SDA中有一个三极管，使用开极/开漏电路(三极管是开极，CMOS管是开漏，作用一样)，如下图：
  ![image-20210220152057547](E:/QRS/doc_and_source_for_drivers/IMX6ULL/doc_pic/04_I2C/pic/04_I2C/010_i2c_signal_internal.png)

真值表如下：
![image-20210220152134970](E:/QRS/doc_and_source_for_drivers/IMX6ULL/doc_pic/04_I2C/pic/04_I2C/011_true_value_table.png)



从真值表和电路图我们可以知道：

* 当某一个芯片不想影响SDA线时，那就不驱动这个三极管
* 想让SDA输出高电平，双方都不驱动三极管(SDA通过上拉电阻变为高电平)
* 想让SDA输出低电平，就驱动三极管



从下面的例子可以看看数据是怎么传的（实现双向传输）。
举例：主设备发送（8bit）给从设备

* 前8个clk
  * 从设备不要影响SDA，从设备不驱动三极管
  * 主设备决定数据，主设备要发送1时不驱动三极管，要发送0时驱动三极管

* 第9个clk，由从设备决定数据
  * 主设备不驱动三极管
  * 从设备决定数据，要发出回应信号的话，就驱动三极管让SDA变为0
  * 从这里也可以知道ACK信号是低电平



从上面的例子，就可以知道怎样在一条线上实现双向传输，这就是SDA上要使用上拉电阻的原因。



为何SCL也要使用上拉电阻？
在第9个时钟之后，如果有某一方需要更多的时间来处理数据，它可以一直驱动三极管把SCL拉低。
当SCL为低电平时候，大家都不应该使用IIC总线，只有当SCL从低电平变为高电平的时候，IIC总线才能被使用。
当它就绪后，就可以不再驱动三极管，这是上拉电阻把SCL变为高电平，其他设备就可以继续使用I2C总线了。



### 2. 使用GPIO模拟I2C的要点

* 引脚设为GPIO
* GPIO设为输出、开极/开漏(open collector/open drain)
* 要有上拉电阻

 

### 3. 驱动程序分析


#### 3.1 平台总线设备驱动模型

![image-20210312115457885](E:/QRS/doc_and_source_for_drivers/IMX6ULL/doc_pic/04_I2C/pic/04_I2C/063_i2c-gpio_module.png)

#### 3.2 设备树

对于GPIO引脚的定义，有两种方法：

* 老方法：gpios
* 新方法：sda-gpios、scl-gpios

![image-20210312104844329](E:/QRS/doc_and_source_for_drivers/IMX6ULL/doc_pic/04_I2C/pic/04_I2C/062_i2c-gpio_node.png)


#### 3.3 驱动程序分析

##### 1. I2C-GPIO驱动层次

![image-20210312120002847](E:/QRS/doc_and_source_for_drivers/IMX6ULL/doc_pic/04_I2C/pic/04_I2C/064_i2c-gpio_level.png)



#### i2c-gpio驱动分析

###### of_device_id结构体

```c
static const struct of_device_id i2c_gpio_dt_ids[] = {
	{ .compatible = "i2c-gpio", },
	{ /* sentinel */ }
};
```

##### 主要结构体

```c
struct i2c_gpio_private_data {
	struct i2c_adapter adap;
	struct i2c_algo_bit_data bit_data;
	struct i2c_gpio_platform_data pdata;
};
struct i2c_gpio_platform_data {
	unsigned int	sda_pin;	/* sda引脚 */
	unsigned int	scl_pin;	/* scl引脚 */
	int		udelay;		
	int		timeout;
	unsigned int	sda_is_open_drain:1;
	unsigned int	scl_is_open_drain:1;
	unsigned int	scl_is_output_only:1;
};
struct i2c_algo_bit_data {
	void *data;		/* private data for lowlevel routines */
	void (*setsda) (void *data, int state);	
	void (*setscl) (void *data, int state);
	int  (*getsda) (void *data);
	int  (*getscl) (void *data);
	int  (*pre_xfer)  (struct i2c_adapter *);
	void (*post_xfer) (struct i2c_adapter *);

	/* local settings */
	int udelay;		/* half clock cycle time in us,
				   minimum 2 us for fast-mode I2C,
				   minimum 5 us for standard-mode I2C and SMBus,
				   maximum 50 us for SMBus */
	int timeout;		/* in jiffies */
};
```

###### i2c_gpio_probe函数分析

```c
	/* i2c_gpio_probe函数分析 */
	
	if (pdev->dev.of_node) {
		ret = of_i2c_gpio_get_pins(pdev->dev.of_node, &sda_pin, &scl_pin);	/* 从设备树获得引脚信息 方式一 */
	} else {
		if (!dev_get_platdata(&pdev->dev))
			return -ENXIO;
		pdata = dev_get_platdata(&pdev->dev);	/* 从dev获得引脚信息 */
		sda_pin = pdata->sda_pin;
		scl_pin = pdata->scl_pin;
	}
	ret = devm_gpio_request(&pdev->dev, sda_pin, "sda");	/* 从设备树获得引脚信息 方式二 */
	ret = devm_gpio_request(&pdev->dev, scl_pin, "scl");	/* 从设备树获得引脚信息 方式二 */
	
	if (pdata->sda_is_open_drain) {
		gpio_direction_output(pdata->sda_pin, 1);
		bit_data->setsda = i2c_gpio_setsda_val;		/* 设置sda值函数 */
	} else {
		gpio_direction_input(pdata->sda_pin);
		bit_data->setsda = i2c_gpio_setsda_dir;		/* 设置sda值函数 */
	}

	if (pdata->scl_is_open_drain || pdata->scl_is_output_only) {
		gpio_direction_output(pdata->scl_pin, 1);
		bit_data->setscl = i2c_gpio_setscl_val;		/* 设置scl值函数 */
	} else {
		gpio_direction_input(pdata->scl_pin);
		bit_data->setscl = i2c_gpio_setscl_dir;		/* 设置scl值函数 */
	}
	if (!pdata->scl_is_output_only)
		bit_data->getscl = i2c_gpio_getscl;			/* 获取scl值函数 */
	bit_data->getsda = i2c_gpio_getsda;				/* 获取sda值函数 */

	/* 添加总线 */
	ret = i2c_bit_add_numbered_bus(adap);

```

###### i2c_bit_add_numbered_bus函数分析

```c
i2c_bit_add_numbered_bus
	__i2c_bit_add_bus
	/* 程序分析 */
static int __i2c_bit_add_bus(struct i2c_adapter *adap, int (*add_adapter)(struct i2c_adapter *))
{
	struct i2c_algo_bit_data *bit_adap = adap->algo_data;
	int ret;
	/* register new adapter to i2c module... */
	adap->algo = &i2c_bit_algo;		/* I2C传输重要结构体 */
	adap->retries = 3;
	if (bit_adap->getscl == NULL)
		adap->quirks = &i2c_bit_quirk_no_clk_stretch;

	ret = add_adapter(adap);	/* 添加适配器 */
	if (ret < 0)
		return ret;

	/* Complain if SCL can't be read */
	if (bit_adap->getscl == NULL) {
		dev_warn(&adap->dev, "Not I2C compliant: can't read SCL\n");
		dev_warn(&adap->dev, "Bus may be unreliable\n");
	}
	return 0;
}
```

###### i2c_bit_algo结构体

```c
const struct i2c_algorithm i2c_bit_algo = {
	.master_xfer	= bit_xfer,
	.functionality	= bit_func,
};
static u32 bit_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_NOSTART | I2C_FUNC_SMBUS_EMUL |
	       I2C_FUNC_SMBUS_READ_BLOCK_DATA |
	       I2C_FUNC_SMBUS_BLOCK_PROC_CALL |
	       I2C_FUNC_10BIT_ADDR | I2C_FUNC_PROTOCOL_MANGLING;
}

static int bit_xfer(struct i2c_adapter *i2c_adap,
		    struct i2c_msg msgs[], int num)
{
	struct i2c_msg *pmsg;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;
	int i, ret;
	unsigned short nak_ok;

	/* 开始i2c传输 (设置sda和scl为低电平) */
	i2c_start(adap);
	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];
		if (pmsg->flags & I2C_M_RD) 
			ret = readbytes(i2c_adap, pmsg); /* 将字节读入缓冲区 */
        else 
			ret = sendbytes(i2c_adap, pmsg); /* 将字节写入缓冲区 */		
	}
	ret = i;
	return ret;
}
```

###### readbytes函数分析

```c
static int readbytes(struct i2c_adapter *i2c_adap, struct i2c_msg *msg)
{
	int inval;
	int rdcount = 0;	/* counts bytes read */
	unsigned char *temp = msg->buf;
	int count = msg->len;
	const unsigned flags = msg->flags;

	while (count > 0) {
		inval = i2c_inb(i2c_adap);
		if (inval >= 0) {
			*temp = inval;
			rdcount++;
		} else {   /* read timed out */
			break;
		}

		temp++;
		count--;
	return rdcount;
}
    /* 接收字节 */
static int i2c_inb(struct i2c_adapter *i2c_adap)
{
	/* read byte via i2c port, without start/stop sequence	*/
	/* acknowledge is sent in i2c_read.			*/
	int i;
	unsigned char indata = 0;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	/* assert: scl is low */
	sdahi(adap);
	for (i = 0; i < 8; i++) {	/* 循环使scl模拟高低电平读取数据 */
		if (sclhi(adap) < 0)  /* timeout */
			return -ETIMEDOUT;
		indata *= 2;
		if (getsda(adap))	/* 高电平 | 1 */
			indata |= 0x01;	
		setscl(adap, 0);
		udelay(i == 7 ? adap->udelay / 2 : adap->udelay);
	}
	/* assert: scl is low */
	return indata;
}
```

###### sendbytes函数分析

```c

    /* 发送字节 */
static int sendbytes(struct i2c_adapter *i2c_adap, struct i2c_msg *msg)
{
	const unsigned char *temp = msg->buf;
	int count = msg->len;
	unsigned short nak_ok = msg->flags & I2C_M_IGNORE_NAK;
	int retval;
	int wrcount = 0;

	while (count > 0) 
		retval = i2c_outb(i2c_adap, *temp);		
	return wrcount;
}
/* i2c_outb函数分析 */
static int i2c_outb(struct i2c_adapter *i2c_adap, unsigned char c)
{
	int i;
	int sb;
	int ack;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	/* assert: scl is low */
	for (i = 7; i >= 0; i--) {	/* 先发送高位 */
		sb = (c >> i) & 1;
		setsda(adap, sb);
		udelay((adap->udelay + 1) / 2);
		if (sclhi(adap) < 0)  /* timed out 高电平保持数据 */	
			return -ETIMEDOUT;
		scllo(adap);	/* 低电平改变数据 */
	}
	sdahi(adap);	
	if (sclhi(adap) < 0)  /* timeout */
		return -ETIMEDOUT;	/* 传输结束将sda和scl都拉高 */
	scllo(adap);
	return ack;
}
```

### 4. 怎么使用I2C-GPIO

设置设备数，在里面添加一个节点即可，示例代码看上面：

* compatible = "i2c-gpio";

* 使用pinctrl把 SDA、SCL所涉及引脚配置为GPIO、开极

  * 可选

* 指定SDA、SCL所用的GPIO

* 指定频率(2种方法)：

  * i2c-gpio,delay-us = <5>;	/* ~100 kHz */
  * clock-frequency = <400000>;

* #address-cells = <1>;

* #size-cells = <0>;

* i2c-gpio,sda-open-drain：

  * 它表示其他驱动、其他系统已经把SDA设置为open drain了
  * 在驱动里不需要在设置为open drain
  * 如果需要驱动代码自己去设置SDA为open drain，就不要提供这个属性

* i2c-gpio,scl-open-drain：

  * 它表示其他驱动、其他系统已经把SCL设置为open drain了
  * 在驱动里不需要在设置为open drain
  * 如果需要驱动代码自己去设置SCL为open drain，就不要提供这个属性

  