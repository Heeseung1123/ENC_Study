#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/types.h>

static struct spi_device *my_spi;
static u8 spi_transfer_buf[1518];

static u8 spi_readReg(struct spi_device *spi, u8 address)
{
	u8 tx_buf[2];
	u8 rx_buf[4]; 
	u8 val = 0;
	int slen = 1; 
	int ret;
	
	if(address & 0x80) //set 8th bit fora MAC/MIIM register address
		slen++;

	tx_buf[0] = (0x00 | (address & 0x1f)); //ADDR_MASK 
	
	ret = spi_write_then_read(spi, tx_buf, 1, rx_buf, slen);	
	if(ret) {
		printk(KERN_INFO "readReg failed\n");
	}
	else {
		val = rx_buf[0];
	}

	return val;
}

static int spi_writeReg(struct spi_device *spi, u8 address, u8 data)
{
	u8 tx_buf[2];
	int ret = 0;

	tx_buf[0] = (0x40 | (address & 0x1f));
	tx_buf[1] = data; 
	
	ret = spi_write(spi, tx_buf, 2);
	if(ret) {
		printk(KERN_INFO "spi-write failed\n");
	}
	return ret;
}

static u8 get_revision(void) 
{
	u8 rev = 0;

	spi_writeReg(my_spi, 0x1f, 0x03);	

	rev = spi_readReg(my_spi, 0x12);

	dump_stack();


	return rev; 
}

static int spi_read_buf(struct spi_device *spi,
		u16 addr, int len, u8 *rx_buf) 
{
	int ret = 0; 
	u8 tx_buf[1]; 
	tx_buf[0] = (0x7A);

	spi_writeReg(spi, 0x1f, 0x00);

	spi_writeReg(spi, 0x00, (addr & 0xff));
	spi_writeReg(spi, 0x01, (addr >> 8)); 

	ret = spi_write_then_read(spi, tx_buf, 1, rx_buf, len);	

	if(ret) {
		printk(KERN_INFO "spi_read_buf failed\n");
	}
	return ret;

}

static int spi_write_buf(struct spi_device *spi,
		u16 addr, int len, u8 *tx_buf) 
{
	int ret = 0; 

	spi_writeReg(spi, 0x1f, 0x00);

	spi_writeReg(spi, 0x02, (addr & 0xff));
	spi_writeReg(spi, 0x03, (addr >> 8)); 

	spi_transfer_buf[0]  = 0x7A;
	memcpy(&spi_transfer_buf[1], spi_transfer_buf, len);
	ret = spi_write(spi, tx_buf, len+1);

	if(ret) {
		printk(KERN_INFO "spi_write_buf failed\n");
	}

	return ret; 

}


/**
 * test_enc28j60_sram - Test the ENC28J60 internal SRAM read/write operations.
 * @spi: Pointer to the SPI device structure.
 */
static void test_enc28j60_sram(struct spi_device *spi)
{
	u8 test_data_out[10] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
	u8 test_data_in[10] = {0};
	u16 test_addr = 0x1000; /* Arbitrary address within SRAM (0x0000-0x1FFF) */
	int i;
	bool success = true;

	printk(KERN_INFO "ENC28J60: Starting SRAM test at address 0x%04x\n", test_addr);

	/* Step 1: Write test pattern to SRAM */
	spi_write_buf(spi, test_addr, 10, test_data_out);

	/* Step 2: Read back the data from SRAM */
	spi_read_buf(spi, test_addr, 10, test_data_in);

	/* Step 3: Compare and verify the data */
	printk(KERN_INFO "ENC28J60: Data read back: ");
	for (i = 0; i < 10; i++) {
		printk(KERN_CONT "%02x ", test_data_in[i]);
		if (test_data_out[i] != test_data_in[i])
			success = false;
	}
	printk(KERN_CONT "\n");

	if (success)
		printk(KERN_INFO "ENC28J60: SRAM R/W test passed\n");
	else
		printk(KERN_ERR "ENC28J60: SRAM R/W test failed! Data mismatch detected.\n");
}

/**
 * my_enc28j60_probe - Bind the driver to an SPI device
 */



static int my_enc28j60_probe(struct spi_device *spi)
{
	my_spi = spi; 
	u8 rev; 

	printk(KERN_INFO "======================================\n");
	printk(KERN_INFO "My Driver: Probe function called!\n");
	printk(KERN_INFO "My Driver: Matching with compatible 'my,enc28j60' SUCCESS!\n");
	printk(KERN_INFO "======================================\n");

	rev = get_revision();
	printk(KERN_INFO "Revision Number: %x\n", rev);

	test_enc28j60_sram(my_spi);
	return 0;
}

/**
 * my_enc28j60_remove - Unbind the driver from the SPI device
 */
static int my_enc28j60_remove(struct spi_device *spi)
{
	printk(KERN_INFO "My Driver: Remove function called. Goodbye!\n");
	return 0;
}

/* Device Tree matching table */
static const struct of_device_id my_enc28j60_dt_ids[] = {
	{ .compatible = "my,enc28j60", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_enc28j60_dt_ids);

static struct spi_driver my_enc28j60_driver = {
	.driver = {
		.name = "my_enc28j60",
		.of_match_table = my_enc28j60_dt_ids,
	},
	.probe = my_enc28j60_probe,
	.remove = my_enc28j60_remove,
};

static int __init my_enc28j60_init(void)
{
	printk(KERN_INFO "My Driver: Initializing SPI driver\n");
	return spi_register_driver(&my_enc28j60_driver);
}

static void __exit my_enc28j60_exit(void)
{
	spi_unregister_driver(&my_enc28j60_driver);
	printk(KERN_INFO "My Driver: Driver unregistered\n");
}

module_init(my_enc28j60_init);
module_exit(my_enc28j60_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("SPI probe skeleton driver");


