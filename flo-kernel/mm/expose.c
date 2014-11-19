#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm-generic/mman-common.h>
#include <asm/pgtable.h>
#include <linux/mm.h>
#include <linux/atomic.h>

/*adds a pte entry in the fake pgd table*/
static void add_fake_pgd_entry(unsigned long *fake_pgd_kern, u32 pte_base_ptr, int index) {
	*(fake_pgd_kern + index) = pte_base_ptr;
	
}

SYSCALL_DEFINE3(expose_page_table, pid_t, pid,
		unsigned long, fake_pgd,
		unsigned long, addr)
{
	int count=0;
	struct task_struct *task;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t pte, *ptep;
	unsigned long *fake_pgd_kern = NULL;
	int i = 0, j = 0, k = 0, pte_count = 0;

	//pteval_t pfn;
	//pfn_t pfn;
	unsigned long pfn = 0;

	pgd_t *pgd_base;
	spinlock_t *ptl;
	int ret;

	printk("pid: %d\n", pid);
	printk("fake_pgd: %lu\n", fake_pgd);
	printk("addr: %lu\n", addr);

	if (pid < -1) {
		pr_err("expose_page_table: invalid pid: %d\n", pid);
		return -EINVAL;
	}

	fake_pgd_kern = kcalloc(2048, sizeof(unsigned long), GFP_KERNEL);
	if (fake_pgd_kern == NULL) {
		return -ENOMEM;
	}
	//TODO access_ok for addr
	read_lock(&tasklist_lock);
	if (pid == -1)
		mm = current->mm;
	else {
		task = pid_task(find_vpid(pid), PIDTYPE_PID);
		mm = task->mm;
	}
	read_unlock(&tasklist_lock);

	pr_err("came here 1.1: task->mm->mmap_sem->activity: %d\n",
			mm->mmap_sem.activity);
	down_read(&mm->mmap_sem);

	pr_err("came here 1.1: task->mm->mmap_sem->activity: %d\n",
                        mm->mmap_sem.activity);
	pgd_base = mm->pgd;
	vma = find_vma(mm, addr);
	vma->vm_flags |= VM_DONTEXPAND;
	if (vma == NULL) {
		pr_err("expose_page_table: vma is NULL\n");
		kfree(fake_pgd_kern);
		return -EFAULT;
	}

	for(pgd = pgd_base; pgd < pgd_base + PTRS_PER_PGD; pgd++) {
		pr_err("came here 2 \n");
		k++;
		if (pgd_none_or_clear_bad(pgd)) {
			pr_err("came 2.1\n");
			continue;
		}
		for (i = 0; i < PAGE_SIZE / sizeof(*pud); i++) {
			pud = pud_offset(pgd, PUD_SIZE * i);

			if (pud_none(*pud) || pud_bad(*pud)) {
				pr_err("came 2.2\n");
				continue;
			}
			for (j = 512; j < PAGE_SIZE / sizeof(*pmd); j++) {
				pmd = pmd_offset(pud, PMD_SIZE * j);
				if (pmd_none(*pmd) || pmd_bad(*pmd)) {
					pr_err("came 2.3\n");
					continue;
				}
				/* pmd_val will give the base address
				* of the pte page
				*/
				pfn = (pmd_val(*pmd) >> PAGE_SHIFT) << PAGE_SHIFT;
				//pfn = page_to_pfn(pmd_page(*pmd));
				
				ret = remap_pfn_range(vma,
						(addr + (pte_count*PAGE_SIZE)),
						pfn, PAGE_SIZE,
						PROT_READ);
				if (ret) {
					pr_err("remap_pfn_range failed");
					pr_err(" pfn = %d, i = %d, j = %d\n",
						pfn, i, j);
					up_read(&mm->mmap_sem);
					kfree(fake_pgd_kern);
					return -EFAULT;
				}
				add_fake_pgd_entry(fake_pgd_kern, addr + (pte_count*PAGE_SIZE), pte_count);
				pte_count++;
				pr_err("came 3.1 count = %d i = %d j = %d k = %d\n",
						pte_count, i, j, k);
			}
			break;
		}
		break;
		/*pud = pud_offset(pgd, 0);
		if (pud_none(*pud)) {
			pr_err("came 2.2\n");
			continue;
		}

		pmd = pmd_offset(pud, 0);
		if (pmd_none(*pmd) || pmd_bad(*pmd)) {
			pr_err("came 2.3\n");
			continue;
		}
		ptep = pte_offset_map(pmd, 0);
		if (!ptep)
			continue;

		pte = *ptep;
		if (pte_none(pte)) {
			pr_err("came 2.4\n");
			continue;
		}
		pr_err("came 2.4.1\n");
		pfn = pte_pfn(pte);
		pr_err("came 2.5\n");
		ret = remap_pfn_range(vma, addr, pfn, 4096, vma->vm_flags);
		pr_err("came here 3\n");
		break;
		*/
	}
	pr_err("came here 4\n");
	up_read(&mm->mmap_sem);
	pr_err("came here 5\n");
	/*
	remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
			2263                     unsigned long pfn, unsigned
						 long size, pgprot_t prot)
	*/
	
	if (copy_to_user((void *)fake_pgd, fake_pgd_kern,
				PAGE_SIZE*4)) {
		pr_err("expose_page_table: copy_to_user");
		pr_err(" failed to copy fake_pgd\n");
		kfree(fake_pgd_kern);
		return -EFAULT;
	}
	pr_err("came here 6\n");

	return 0;
}
