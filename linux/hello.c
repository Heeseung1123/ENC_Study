#include <linux/init.h>
#include <linux/module.h>
MODULE_LICENSE("GPL");
static int __init h_init(void) { 
	printk("Hello world\n");
	return 0;
}
static void __exit h_exit(void) { }
module_init(h_init);
module_exit(h_exit);



