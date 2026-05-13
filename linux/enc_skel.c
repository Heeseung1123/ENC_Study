#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>

/**
 * my_enc28j60_probe - Bind the driver to an SPI device
 */
static int my_enc28j60_probe(struct spi_device *spi)
{
	printk(KERN_INFO "======================================\n");
	printk(KERN_INFO "My Driver: Probe function called!\n");
	printk(KERN_INFO "My Driver: Matching with compatible 'my,enc28j60' SUCCESS!\n");
	printk(KERN_INFO "======================================\n");
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


