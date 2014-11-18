#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm-generic/mman-common.h>
#include <linux/mm.h>

SYSCALL_DEFINE3(expose_page_table, pid_t, pid,
		unsigned long, fake_pgd,
		unsigned long, addr)
{
	struct task_struct *task;
	struct vm_area_struct *vma;
	pgd_t *pgd;
	pgd_t *pgd_base;
	pte_t pte;
	pteval_t pfn;
	int ret;

	printk("pid: %d\n", pid);
	printk("fake_pgd: %lu\n", fake_pgd);
	printk("addr: %lu\n", addr);

	if (pid < -1) {
		pr_err("expose_page_table: invalid pid: %d\n", pid);
		return -EINVAL;
	}

	//TODO access_ok for addr
	read_lock(&tasklist_lock);
	if (pid == -1)
		task = current;
	else
		task = pid_task(find_vpid(pid), PIDTYPE_PID);

	pgd_base = task->mm->pgd;
	vma = find_vma(task->mm, addr);
	vma->vm_flags |= VM_DONTEXPAND;
	if (vma == NULL) {
		pr_err("expose_page_table: vma is NULL\n");
		return -EFAULT;
	}

	pr_err("came here 1: task->mm->mmap_sem->activity: %d\n",
			task->mm->mmap_sem.activity);
	down_read(&task->mm->mmap_sem);
	pr_err("came here 1.1: task->mm->mmap_sem->activity: %d\n",
			task->mm->mmap_sem.activity);
	for(pgd = pgd_base; pgd < pgd_base + PTRS_PER_PGD; pgd++) {
	pr_err("came here 2\n");
		if (pgd_none_or_clear_bad(pgd))
			continue;
		pte = pgd_val(*pgd);
		pr_err("pte: %lu\n", pte);
		pfn = pte_val(pte);
		pr_err("pfn: %lu\n", pfn);
		ret = remap_pfn_range(vma, addr, pfn, 4096, PROT_READ);
		//for debugging
		pr_err("expose_page_table: ret val of remap: %d\n", ret);
		break;
	pr_err("came here 3\n");

	}
	pr_err("came here 4: task->mm->mmap_sem->activity: %d\n",
			task->mm->mmap_sem.activity);
	up_read(&task->mm->mmap_sem);
	pr_err("came here 5: task->mm->mmap_sem->activity: %d\n",
			task->mm->mmap_sem.activity);
	/*
	remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
			2263                     unsigned long pfn, unsigned
						 long size, pgprot_t prot)
	*/
	read_unlock(&tasklist_lock);

	return 378;
}
