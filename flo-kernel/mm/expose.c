#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>

SYSCALL_DEFINE3(expose_page_table, pid_t, pid,
		unsigned long, fake_pgd,
		unsigned long, addr)
{
	printk("pid: %d\n", pid);
	printk("fake_pgd: %lu\n", fake_pgd);
	printk("addr: %lu\n", addr);

	return 378;
}
