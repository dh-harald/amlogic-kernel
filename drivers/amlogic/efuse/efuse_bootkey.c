#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/of.h>

#include <linux/ebsecure/ebsecure.h>
#include <linux/efuse.h>
#include "efuse_regs.h"

#define EFUSE_BOOTKEY_MODULE_NAME    "aml_keys-t"
#define EFUSE_BOOTKEY_DRIVER_NAME	"aml_keys-t"
#define EFUSE_BOOTKEY_DEVICE_NAME    "ebootkey"
#define EFUSE_BOOTKEY_CLASS_NAME     "ebootkey"


typedef struct 
{
    struct cdev cdev;
    unsigned int flags;
}ebootkey_dev_t;

static ebootkey_dev_t *ebootkey_devp=NULL;
static dev_t ebootkey_devno;
static struct device * ebootkey_device= NULL;


int efuse_read_item(char *buf, size_t count, loff_t *ppos);
int efuse_write_item(char *buf, size_t count, loff_t *ppos);
int efuse_getinfo_byTitle(char *title, efuseinfo_item_t *info);

static int ebootkey_open(struct inode *inode, struct file *file)
{
	ebootkey_dev_t *devp;
	devp = container_of(inode->i_cdev, ebootkey_dev_t, cdev);
	file->private_data = devp;
	return 0;
}
static int ebootkey_release(struct inode *inode, struct file *file)
{
	ebootkey_dev_t *devp;
	devp = file->private_data;
	return 0;
}

static loff_t ebootkey_llseek(struct file *filp, loff_t off, int whence)
{
	return 0;
}

static long ebootkey_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg )
{
	return 0;
}

static ssize_t ebootkey_read( struct file *file, char __user *buf, size_t count, loff_t *ppos )
{
	return 0;
}

static ssize_t ebootkey_write( struct file *file, const char __user *buf, size_t count, loff_t *ppos )
{
	int ret;
	char *local_buf;

	local_buf = kzalloc(count, GFP_KERNEL);
	if(!local_buf){
		printk(KERN_INFO "memory not enough,%s:%d\n",__func__,__LINE__);
		return -ENOMEM;
	}

	if (copy_from_user(local_buf, buf, count)){
		ret =  -EFAULT;
		goto exit;
	}
	
	ret = aml_ebootkey_put(local_buf,count);
	if(ret < 0){
		printk("write ebootkey error,%s:%d\n",__func__,__LINE__);
		goto exit;
	}

	ret = count;
exit:
	kfree(local_buf);
	return ret;
}

static ssize_t ebootkey_version_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	ssize_t n=0;
	n += sprintf(&buf[n], "version:1.0");
	buf[n] = 0;
	return n;
}

static const struct file_operations ebootkey_fops = {
	.owner      = THIS_MODULE,
	.llseek     = ebootkey_llseek,
	.open       = ebootkey_open,
	.release    = ebootkey_release,
	.read       = ebootkey_read,
	.write      = ebootkey_write,
	.unlocked_ioctl      = ebootkey_unlocked_ioctl,
};

static struct device_attribute ebootkey_class_attrs[] ={
	__ATTR_RO(ebootkey_version),
//	__ATTR_RO(installed_keys), 
	__ATTR_NULL 
};
static struct class ebootkey_class ={
	.name = EFUSE_BOOTKEY_CLASS_NAME,
	.dev_attrs = ebootkey_class_attrs, 
};

#ifdef CONFIG_OF
static const struct of_device_id meson6_ebootkey_dt_match[];
static char *get_ebootkey_drv_data(struct platform_device *pdev)
{
	char *key_dev = NULL;
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(meson6_ebootkey_dt_match, pdev->dev.of_node);
		if(match){
			key_dev = (char*)match->data;
		}
	}
	return key_dev;
}
#endif

static int aml_ebootkey_probe(struct platform_device *pdev)
{
	int ret=-1;
	struct device *devp;

	ret = alloc_chrdev_region(&ebootkey_devno, 0, 1, EFUSE_BOOTKEY_DEVICE_NAME);
	if(ret<0){
		printk(KERN_ERR "ebootkey: failed to allocate major number\n ");
		ret = -ENODEV;
		goto out;
	}
	printk("%s:%d=============ebootkey_devno:%x\n",__func__,__LINE__,ebootkey_devno);
    ret = class_register(&ebootkey_class);
    if (ret){
        goto error1;
	}
	ebootkey_devp = kzalloc(sizeof(ebootkey_dev_t), GFP_KERNEL);
	if(!ebootkey_devp){
		printk(KERN_ERR "unifykey: failed to allocate memory\n ");
		ret = -ENOMEM;
		goto error2;
	}
	/* connect the file operations with cdev */
	cdev_init(&ebootkey_devp->cdev, &ebootkey_fops);
	ebootkey_devp->cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&ebootkey_devp->cdev, ebootkey_devno, 1);
	if(ret){
		printk(KERN_ERR "ebootkey: failed to add device\n");
		goto error3;
	}
	devp = device_create(&ebootkey_class, NULL, ebootkey_devno, NULL, EFUSE_BOOTKEY_DEVICE_NAME);
	if (IS_ERR(devp))
	{
		printk(KERN_ERR "ebootkey: failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}
#ifdef CONFIG_OF
	devp->platform_data = get_ebootkey_drv_data(pdev);
#else
    if (pdev->dev.platform_data)
        devp->platform_data = pdev->dev.platform_data;
    else
        devp->platform_data = NULL;
#endif	
	ebootkey_device = devp;
	printk(KERN_INFO "ebootkey: device %s created ok\n", EFUSE_BOOTKEY_DEVICE_NAME);
	return 0;

error4:
	cdev_del(&ebootkey_devp->cdev);
error3:
	kfree(ebootkey_devp);
error2:
	class_unregister(&ebootkey_class);
error1:
	unregister_chrdev_region(ebootkey_devno, 1);
out:
	return ret;
}

static int aml_ebootkey_remove(struct platform_device *pdev)
{
	//if (pdev->dev.of_node) {
	//	unifykey_dt_release(pdev);
	//}
	unregister_chrdev_region(ebootkey_devno, 1);
	device_destroy(&ebootkey_class, ebootkey_devno);
	//device_destroy(&efuse_class, unifykey_devno);
	cdev_del(&ebootkey_devp->cdev);
	kfree(ebootkey_devp);
	class_unregister(&ebootkey_class);
	return 0;
}

#ifdef CONFIG_OF
//static char * secure_device[3]={"nand_key","emmc_key",NULL};

static const struct of_device_id meson6_ebootkey_dt_match[]={
	{	.compatible = "amlogic,efuse_bootkey",
		.data		= NULL,
		//.data		= (void *)&secure_device[0],
	},
	{},
};
//MODULE_DEVICE_TABLE(of,meson6_rtc_dt_match);
#else
#define meson6_ebootkey_dt_match NULL
#endif

static struct platform_driver aml_efuse_bootkey_driver =
{ 
	.probe = aml_ebootkey_probe, 
	.remove = aml_ebootkey_remove, 
	.driver =
     {
     	.name = EFUSE_BOOTKEY_DEVICE_NAME, 
        .owner = THIS_MODULE, 
        .of_match_table = meson6_ebootkey_dt_match,
     }, 
};


static int __init efuse_bootkey_init(void)
{
	int ret = -1;
	ret = platform_driver_register(&aml_efuse_bootkey_driver);
	 if (ret != 0)
    {
        printk(KERN_ERR "failed to register efuse bootkey driver, error %d\n", ret);
        return -ENODEV;
    }
    printk(KERN_INFO "platform_driver_register--efuse bootkey  driver--------------------\n");

    return ret;
}
static void __exit efuse_bootkey_exit(void)
{
	platform_driver_unregister(&aml_efuse_bootkey_driver);
}

module_init(efuse_bootkey_init);
module_exit(efuse_bootkey_exit);

MODULE_DESCRIPTION("AMLOGIC eFuse bootkey driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhou benlong <benlong.zhou@amlogic.com>");
