#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/mman.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/poll.h>

#define MODULE_TEST_DEV_MAJOR (249)
#define MODULE_TEST_DEV_MINOR (0)
#define MODULE_TEST_DEV_NUM   (1)
#define MODULE_TEST_NAME      "module_test"

static char *teststring="moduletest_str_param";
module_param(teststring,charp,S_IRUGO|S_IWUSR);

typedef struct module_test_total_s
{
    dev_t  module_test_dev_id;
    struct cdev module_test_cdev;
    struct mutex module_test_cdev_mutex;
    struct semaphore module_test_cdev_sema;
    wait_queue_head_t module_test_cdev_readqueue;
    wait_queue_head_t module_test_cdev_writequeue;
    unsigned int rp;
    unsigned int wp;
    unsigned int poll_status;
    unsigned int read_available;
    unsigned int write_available;
    unsigned int ref_count;
    char   buffer[1024];
} module_test_total_t;

static dev_t module_test_dev_id;
static module_test_total_t module_test_total_data;

static ssize_t module_test_cdev_read(struct file *filp, char *buffer, size_t len, loff_t *offset)
{
    int err_count=0;
    //mutex_lock(&module_test_total_data.module_test_cdev_mutex);
    module_test_total_t *p = (module_test_total_t *)(filp->private_data); 

    err_count = copy_to_user(buffer,p->buffer,strlen(p->buffer)); 
    if(err_count){
        printk(KERN_ERR "%s: FAIL copy to user\n",__func__);
    }
    len = strlen(p->buffer);

    //should check free space to decide
    p->write_available=true;
    //mutex_unlock(&module_test_total_data.module_test_cdev_mutex);
return err_count; 
}

static ssize_t module_test_cdev_write(struct file *filp, const char *buffer, size_t len, loff_t *offset)
{
    int err_count=0;
    //mutex_lock(&module_test_total_data.module_test_cdev_mutex);
    module_test_total_t *p = (module_test_total_t *)(filp->private_data); 

    err_count = copy_from_user(p->buffer,buffer,len); 
    if(err_count){
        printk(KERN_ERR "%s: FAIL copy from user\n",__func__);
    }

    p->read_available=true;
    //mutex_unlock(&module_test_total_data.module_test_cdev_mutex);
return len;
}

static long module_test_cdev_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
    module_test_total_t *p = (module_test_total_t *)(filp->private_data); 
    //mutex_lock(&module_test_total_data.module_test_cdev_mutex);
    //mutex_unlock(&module_test_total_data.module_test_cdev_mutex);
return 0;
}

static int module_test_cdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int err = 0;
    unsigned long vma_size = vma->vm_end - vma->vm_start;
    //mutex_lock(&module_test_total_data.module_test_cdev_mutex);
    //vma->vm_page_prot = pgprot_nocached(vma->vm_page_prot); 
    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot); 

    if(remap_pfn_range(vma, vma->vm_start, 
                       vma->vm_pgoff, vma_size, vma->vm_page_prot)){
        err = -EINVAL;
    }
    //mutex_unlock(&module_test_total_data.module_test_cdev_mutex);
return err;
}

static unsigned int module_test_cdev_poll(struct file *filp, poll_table *wait)
{
    module_test_total_t *p = (module_test_total_t *)(filp->private_data); 
    unsigned int mask=0;

    //down(&p->module_test_cdev_sema);

    poll_wait(filp,&p->module_test_cdev_readqueue,wait);
    poll_wait(filp,&p->module_test_cdev_writequeue,wait);

    if(p->read_available){
        mask |=POLLIN|POLLRDNORM;
    }
    if(p->write_available){
        mask |=POLLOUT|POLLWRNORM;
    }
    //mask = p->poll_status;
    //up(&p->module_test_cdev_sema);

return mask;
}

static int module_test_cdev_open(struct inode *inode, struct file *filp)
{
    ++module_test_total_data.ref_count;
    filp->private_data = (void *)(&module_test_total_data);
return 0;
}

static int module_test_cdev_release(struct inode *inode, struct file *filp)
{
    --module_test_total_data.ref_count;
return 0;
}

static struct file_operations module_test_fops = {
	.owner		= THIS_MODULE,
	.open		= module_test_cdev_open,
	.release	= module_test_cdev_release,
	.unlocked_ioctl	= module_test_cdev_ioctl,
	.mmap		= module_test_cdev_mmap,
        .poll           = module_test_cdev_poll,
        .read           = module_test_cdev_read,
        .write          = module_test_cdev_write,
};


int __init module_test_init(void)
{
    int err=0;

    module_test_dev_id =
       MKDEV(MODULE_TEST_DEV_MAJOR,MODULE_TEST_DEV_MINOR);
    err = register_chrdev_region(module_test_dev_id,
                   MODULE_TEST_DEV_NUM, MODULE_TEST_NAME);

    if(err){
        printk(KERN_ERR "module_test init fail\n");
        goto module_test_init_fail_1;
    }

    cdev_init(&module_test_total_data.module_test_cdev, &module_test_fops);

    module_test_total_data.module_test_cdev.owner = THIS_MODULE;
    module_test_total_data.module_test_dev_id=module_test_dev_id;

    err = cdev_add(&module_test_total_data.module_test_cdev, 
                    module_test_dev_id,MODULE_TEST_DEV_NUM);
    if(err){
        printk(KERN_ERR "module_test cdev_add fail\n");
        goto module_test_init_fail_1;
    }

    mutex_init(&module_test_total_data.module_test_cdev_mutex);
    sema_init(&module_test_total_data.module_test_cdev_sema,1);
    init_waitqueue_head(&module_test_total_data.module_test_cdev_readqueue);
    init_waitqueue_head(&module_test_total_data.module_test_cdev_writequeue);

    printk(KERN_INFO "module_test_init MAJ:%d, MIN%d \n",MAJOR(module_test_dev_id),MINOR(module_test_dev_id));

module_test_init_fail_1:
return err;
}


void __exit module_test_exit(void)
{
    cdev_del(&module_test_total_data.module_test_cdev);
    unregister_chrdev_region(module_test_dev_id, MODULE_TEST_DEV_NUM);
}


module_init(module_test_init);
module_exit(module_test_exit);
//MODULE_LICENSE("GPL");
