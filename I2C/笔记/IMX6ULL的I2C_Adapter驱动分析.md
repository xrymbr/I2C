## IMX6ULL的I2C_Adapter驱动分析

参考资料：

* Linux内核真正的I2C控制器驱动程序
  * IMX6ULL: `Linux-4.9.88\drivers\i2c\busses\i2c-imx.c`
  * STM32MP157: `Linux-5.4\drivers\i2c\busses\i2c-stm32f7.c`
* 芯片手册
  * IMXX6ULL：`IMX6ULLRM.pdf`
    * `Chapter 31: I2C Controller (I2C)`
  * STM32MP157：`DM00327659.pdf`
    * `52 Inter-integrated circuit (I2C) interface`

## 设备树分析

##### i2c1为例

```c
 /* i2c1为例 */
 i2c1: i2c@021a0000 {
     #address-cells = <1>;
     #size-cells = <0>;
     compatible = "fsl,imx6ul-i2c", "fsl,imx21-i2c";
     reg = <0x021a0000 0x4000>;
     interrupts = <GIC_SPI 36 IRQ_TYPE_LEVEL_HIGH>;
     clocks = <&clks IMX6UL_CLK_I2C1>;
     status = "disabled";
};
 &i2c1 {
     clock-frequency = <100000>;
     pinctrl-names = "default";
     pinctrl-0 = <&pinctrl_i2c1>;
     status = "okay";
};

```

##### IMX6ULL i2c驱动分析

###### i2c-imx.c驱动分析

probe函数分析

```c
	struct imx_i2c_struct *i2c_imx;
	struct resource *res;
	struct imxi2c_platform_data *pdata = dev_get_platdata(&pdev->dev);
	void __iomem *base;
	int irq, ret;
	dma_addr_t phy_addr;
	
	/* 获取设备中断号 */
	irq = platform_get_irq(pdev, 0);

	/* 获取虚拟地址 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	/* 保存物理地址 */
	phy_addr = (dma_addr_t)res->start;
	/* 保存私有数据 */
	if (of_id)	/* 使用of_device_id匹配 （支持设备树）*/
		i2c_imx->hwdata = of_id->data;
	else		/* 使用platform_device_id匹配 （不支持设备树）*/
		i2c_imx->hwdata = (struct imx_i2c_hwdata *)
				platform_get_device_id(pdev)->driver_data;

	/* 此处为 imx21_i2c_hwdata */
static const struct imx_i2c_hwdata imx21_i2c_hwdata = {
	.devtype		= IMX21_I2C,	// 1
	.regshift		= IMX_I2C_REGSHIFT,		// 2
	.clk_div		= imx_i2c_clk_div,
	.ndivs			= ARRAY_SIZE(imx_i2c_clk_div),
	.i2sr_clr_opcode	= I2SR_CLR_OPCODE_W0C,		// 0x0
	.i2cr_ien_opcode	= I2CR_IEN_OPCODE_1,	// I2CR_IEN	0x80
};
	/* 设置i2c适配器的信息 */
	strlcpy(i2c_imx->adapter.name, pdev->name, sizeof(i2c_imx->adapter.name));
	i2c_imx->adapter.owner		= THIS_MODULE;
	i2c_imx->adapter.algo		= &i2c_imx_algo; /* 最主要的I2C数据处理结构体 */
	i2c_imx->adapter.dev.parent	= &pdev->dev;
	i2c_imx->adapter.nr		= pdev->id;
	i2c_imx->adapter.dev.of_node	= pdev->dev.of_node;
	i2c_imx->base			= base;
	/* 获得时钟 */
	i2c_imx->clk = devm_clk_get(&pdev->dev, NULL);
	/* 使能时钟 */	
	ret = clk_prepare_enable(i2c_imx->clk);
	/* 申请中断 */	
	ret = devm_request_irq(&pdev->dev, irq, i2c_imx_isr, IRQF_NO_SUSPEND, pdev->name, i2c_imx);
	/* 初始化队列 */
	init_waitqueue_head(&i2c_imx->queue);	
	/* 设置适配器数据 */
	i2c_set_adapdata(&i2c_imx->adapter, i2c_imx);   //	dev->driver_data = data;
	/* 设置平台驱动数据 */
	platform_set_drvdata(pdev, i2c_imx);		//	dev->driver_data = data;
	/* 设置时钟分频器 */
	i2c_imx->bitrate = IMX_I2C_BIT_RATE;
	ret = of_property_read_u32(pdev->dev.of_node,"clock-frequency", &i2c_imx->bitrate);
	/* 将芯片寄存器设置为默认值 */
	/* 此时val为0x00，设置的寄存器为 I2Cx_I2CR 效果清空寄存器 */
	imx_i2c_write_reg(i2c_imx->hwdata->i2cr_ien_opcode ^ I2CR_IEN, i2c_imx, IMX_I2C_I2CR);
	/* 此时val为0x00，设置的寄存器为 I2Cx_I2SR 效果为清空寄存器 */
	imx_i2c_write_reg(i2c_imx->hwdata->i2sr_clr_opcode, i2c_imx, IMX_I2C_I2SR);
	/* 初始化可选总线恢复函数 */
	ret = i2c_imx_init_recovery_info(i2c_imx, pdev);
	/* 添加i2c适配器 */
	ret = i2c_add_numbered_adapter(&i2c_imx->adapter);
	/* 初始化 DMA 配置 */
	i2c_imx_dma_request(i2c_imx, phy_addr);

```

###### i2c_imx_algo函数分析

```c
static struct i2c_algorithm i2c_imx_algo = {
	.master_xfer	= i2c_imx_xfer,		/* 传输函数 */
	.functionality	= i2c_imx_func,		/* 支持传输类型 */
};
static u32 i2c_imx_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL
		| I2C_FUNC_SMBUS_READ_BLOCK_DATA;
}

	/* i2c_imx_xfer函数分析 */
	unsigned int i, temp;
	int result;
	bool is_lastmsg = false;
	bool enable_runtime_pm = false;
	struct imx_i2c_struct *i2c_imx = i2c_get_adapdata(adapter);
	/* 开始 I2C 传输 */
	result = i2c_imx_start(i2c_imx);
	/* 开始读写操作 */
    if (msgs[i].flags & I2C_M_RD)
        result = i2c_imx_read(i2c_imx, &msgs[i], is_lastmsg);
    else {
        if (i2c_imx->dma && msgs[i].len >= DMA_THRESHOLD)
            result = i2c_imx_dma_write(i2c_imx, &msgs[i]);
        else
            result = i2c_imx_write(i2c_imx, &msgs[i]);
    }
	/* 停止 I2C 传输 */
	i2c_imx_stop(i2c_imx);

```

###### i2c_imx_start函数分析

```c
	/* 设置时钟 */	
	i2c_imx_set_clk(i2c_imx);   
	imx_i2c_write_reg(i2c_imx->ifdr, i2c_imx, IMX_I2C_IFDR);	/* 写分频寄存器 */
    /* 使能i2c控制器 */
    imx_i2c_write_reg(i2c_imx->hwdata->i2sr_clr_opcode, i2c_imx, IMX_I2C_I2SR);/* 清空I2SR寄存器 */
    imx_i2c_write_reg(i2c_imx->hwdata->i2cr_ien_opcode, i2c_imx, IMX_I2C_I2CR);/* 设置IEN位，使能I2C */

    /* 等待控制器稳定 */
    usleep_range(50, 150);

    /* Start I2C transaction */
    temp = imx_i2c_read_reg(i2c_imx, IMX_I2C_I2CR);	
    temp |= I2CR_MSTA;
    imx_i2c_write_reg(temp, i2c_imx, IMX_I2C_I2CR);		/* 切换为主机模式 */
    result = i2c_imx_bus_busy(i2c_imx, 1);		/* 判断总线状态 */
    if (result)
        return result;
    i2c_imx->stopped = 0;

    temp |= I2CR_IIEN | I2CR_MTX | I2CR_TXAK;	/* I2CR_IIEN 中断使能 */
    temp &= ~I2CR_DMAEN;
    imx_i2c_write_reg(temp, i2c_imx, IMX_I2C_I2CR);
```
###### i2c_imx_isr中断函数分析

```c
static irqreturn_t i2c_imx_isr(int irq, void *dev_id)
{
    struct imx_i2c_struct *i2c_imx = dev_id;
    unsigned int temp;
    temp = imx_i2c_read_reg(i2c_imx, IMX_I2C_I2SR);	/* 读取状态寄存器 */
    if (temp & I2SR_IIF) {
        /* save status register */
        i2c_imx->i2csr = temp;	/* 清I2SR_IIF位前先保存 */
        temp &= ~I2SR_IIF;
        temp |= (i2c_imx->hwdata->i2sr_clr_opcode & I2SR_IIF);
        imx_i2c_write_reg(temp, i2c_imx, IMX_I2C_I2SR);	/* 清I2SR_IIF位 */
        wake_up(&i2c_imx->queue); /* 唤醒队列 */
        return IRQ_HANDLED;
	}

	return IRQ_NONE;
}
```


###### i2c_imx_read函数分析

![image-20210220150954993](file://E:/QRS/doc_and_source_for_drivers/IMX6ULL/doc_pic/04_I2C/pic/04_I2C/008_i2c_read.png?lastModify=1660358179)

```c
int i, result;
unsigned int temp;
int block_data = msgs->flags & I2C_M_RECV_LEN;
	/* 写设备地址 */
	imx_i2c_write_reg((msgs->addr << 1) | 0x01, i2c_imx, IMX_I2C_I2DR);
	/* 休眠等待中断复位 */
	result = i2c_imx_trx_complete(i2c_imx);
		wait_event_timeout(i2c_imx->queue, i2c_imx->i2csr & I2SR_IIF, HZ / 10);
		i2c_imx->i2csr = 0;
	/* 判断是否收到确认信号 */
	result = i2c_imx_acked(i2c_imx);

	/* 启动总线读取数据 */
	temp = imx_i2c_read_reg(i2c_imx, IMX_I2C_I2CR);
	temp &= ~I2CR_MTX;	/* 设置为接收 */

	/*
	 * Reset the I2CR_TXAK flag initially for SMBus block read since the
	 * length is unknown
	 */
	if ((msgs->len - 1) || block_data)
		temp &= ~I2CR_TXAK;
	imx_i2c_write_reg(temp, i2c_imx, IMX_I2C_I2CR);
	imx_i2c_read_reg(i2c_imx, IMX_I2C_I2DR); /* 假读 */


	/* 读取数据 */
	for (i = 0; i < msgs->len; i++) {
		u8 len = 0;

		result = i2c_imx_trx_complete(i2c_imx);  /* 休眠等待中断复位 */
		/* 第一个字节是 SMBus 块数据读取中剩余数据包的长度。 将其添加到msgs->len。*/
		if ((!i) && block_data) {
			len = imx_i2c_read_reg(i2c_imx, IMX_I2C_I2DR);
			if ((len == 0) || (len > I2C_SMBUS_BLOCK_MAX))
				return -EPROTO;
			msgs->len += len;
		}
		if (i == (msgs->len - 1)) {
			if (is_lastmsg) {
				/* 如果为最后一个数据 */
				dev_dbg(&i2c_imx->adapter.dev,
					"<%s> clear MSTA\n", __func__);
				temp = imx_i2c_read_reg(i2c_imx, IMX_I2C_I2CR);
				temp &= ~(I2CR_MSTA | I2CR_MTX);/* 设置为接收和从机模式 */
				imx_i2c_write_reg(temp, i2c_imx, IMX_I2C_I2CR);
				i2c_imx_bus_busy(i2c_imx, 0);
				i2c_imx->stopped = 1;/* 读取DR寄存器之前停止，防止生成下一周期 */
			} else {
				/*
				 * 对于 i2c 主接收器重复重启操作，如：读取 -> 重复 MSTA -> 读/写控制器必须在读取
				 * 第一次读取操作中的最后一个字节之前设置 MTX，否则第一次读取成本一个额外的时钟周期。
				 */
				temp = readb(i2c_imx->base + IMX_I2C_I2CR);
				temp |= I2CR_MTX;
				writeb(temp, i2c_imx->base + IMX_I2C_I2CR);
			}
		} else if (i == (msgs->len - 2)) { /* 如果为倒数第二个msg */
			dev_dbg(&i2c_imx->adapter.dev,
				"<%s> set TXAK\n", __func__);
			temp = imx_i2c_read_reg(i2c_imx, IMX_I2C_I2CR);
			temp |= I2CR_TXAK;
			imx_i2c_write_reg(temp, i2c_imx, IMX_I2C_I2CR);
		}
		if ((!i) && block_data)
			msgs->buf[0] = len;
		else
			msgs->buf[i] = imx_i2c_read_reg(i2c_imx, IMX_I2C_I2DR); /* 从DR寄存器读取数据 */
		dev_dbg(&i2c_imx->adapter.dev,
			"<%s> read byte: B%d=0x%X\n",
			__func__, i, msgs->buf[i]);
	}

```
###### i2c_imx_write函数分析

![image-20210220150757825](file://E:/QRS/doc_and_source_for_drivers/IMX6ULL/doc_pic/04_I2C/pic/04_I2C/007_i2c_write.png?lastModify=1660372410)

```c
static int i2c_imx_write(struct imx_i2c_struct *i2c_imx, struct i2c_msg *msgs)
{
	int i, result;
    
	/* 写从设备地址 */
	imx_i2c_write_reg(msgs->addr << 1, i2c_imx, IMX_I2C_I2DR);
	/* 等待中断完成 */
    result = i2c_imx_trx_complete(i2c_imx);
	/* 接收回应 */
	result = i2c_imx_acked(i2c_imx);

	/* 写数据 */
	for (i = 0; i < msgs->len; i++) {
		imx_i2c_write_reg(msgs->buf[i], i2c_imx, IMX_I2C_I2DR);	/* 向I2DR寄存器写数据 */
		result = i2c_imx_trx_complete(i2c_imx); /* 等待中断完成 */
		result = i2c_imx_acked(i2c_imx);	/* 接收回应 */
	}
	return 0;
}
```

###### i2c_imx_stop函数分析

```c
static void i2c_imx_stop(struct imx_i2c_struct *i2c_imx)
{
	unsigned int temp = 0;

	if (!i2c_imx->stopped) { /* stopped为0时表示还未停止交易，为1是表示已经停止了交易 */
		/* Stop I2C transaction */
		temp = imx_i2c_read_reg(i2c_imx, IMX_I2C_I2CR);
		temp &= ~(I2CR_MSTA | I2CR_MTX);
		if (i2c_imx->dma)
			temp &= ~I2CR_DMAEN;
		imx_i2c_write_reg(temp, i2c_imx, IMX_I2C_I2CR);
	}
	if (!i2c_imx->stopped) {
		i2c_imx_bus_busy(i2c_imx, 0);
		i2c_imx->stopped = 1;
	}
	/* 禁用 I2C 控制器 */
	temp = i2c_imx->hwdata->i2cr_ien_opcode ^ I2CR_IEN,
	imx_i2c_write_reg(temp, i2c_imx, IMX_I2C_I2CR);
}
```

