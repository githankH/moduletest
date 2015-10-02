#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>

#include <linux/kprobes.h>
#include <linux/ptrace.h>

static struct kprobe kp;
static char *probesym="do_fork";
module_param(probesym,charp,S_IRUGO|S_IWUSR);

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    int i=0;
    printk("pre_handler: p->addr=0x%p\n",p->addr);

    for(i=0;i<18;++i){
        printk("regs->uregs[%d]=0x%lx \n",i,regs->uregs[i]);
    }
    dump_stack();

return 0;
}

static void handler_post(struct kprobe *p, struct pt_regs *regs)
{
    int i=0;
    printk("pre_handler: p->addr=0x%p\n",p->addr);
    for(i=0;i<18;++i){
        printk("regs->uregs[%d]=0x%lx \n",i,regs->uregs[i]);
    }
    dump_stack();
}

static int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnum)
{
    printk("fault_handler: p->addr=0x%p, trap #%d\n",p->addr, trapnum);
return 0;
}


int __init kprobe_init_module(void)
{
    int ret;
    kp.pre_handler = handler_pre;
    kp.post_handler = handler_post;
    kp.fault_handler = handler_fault;

    kp.addr = (kprobe_opcode_t*)kallsyms_lookup_name(probesym);
    if(!kp.addr){
        printk("Could not find %s to plant kprobe \n", probesym);
        return -1;
    }

    if((ret = register_kprobe(&kp))<0){
        printk("register_kprobe failed, returned %d\n", ret);
        return -1;
    }

    printk("kprobe registered O.K.\n");
return 0;
}

void __exit kprobe_cleanup_module(void)
{
    unregister_kprobe(&kp);
    printk("kprobe unregistered \n");
}

module_init(kprobe_init_module);
module_exit(kprobe_cleanup_module);
MODULE_LICENSE("GPL");
