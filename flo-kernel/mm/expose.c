#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm-generic/mman-common.h>
#include <asm/pgtable.h>
#include <linux/mm.h>
#include <linux/atomic.h>


static struct task_struct *find_process_by_pid(pid_t pid)
{
	return pid ? find_task_by_vpid(pid) : current;
}


SYSCALL_DEFINE3(expose_page_table, pid_t, pid,
		unsigned long, fake_pgd,
		unsigned long, addr)
{
	struct task_struct *task;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	pgd_t *pgd = NULL;
	pud_t *pud = NULL;
	pmd_t *pmd = NULL;
	unsigned long *fake_pgd_kern = NULL;
	int i = 0, j = 0, k = 0, pte_count = 0;

	//pteval_t pfn;
	//pfn_t pfn;
	unsigned long pfn = 0;

	pgd_t *pgd_base;
	int ret;

	printk("pid: %d\n", pid);
	printk("fake_pgd: %lu\n", fake_pgd);
	printk("addr: %lu\n", addr);

	if (pid < -1) {
		pr_err("expose_page_table: invalid pid: %d\n", pid);
		return -EINVAL;
	}

	fake_pgd_kern = kcalloc(PTRS_PER_PGD,
			sizeof(unsigned long), GFP_KERNEL);
	if (fake_pgd_kern == NULL) {
		return -ENOMEM;
	}
	//TODO access_ok for addr
	rcu_read_lock();
	if (pid == -1)
		mm = current->mm;
	else {
		//task = pid_task(find_vpid(pid), PIDTYPE_PID);
		task = find_process_by_pid(pid);
		pr_err("task is: %x\n", task);
		if (task == NULL) {
			pr_err("expose_page_table: No task with pid: %d\n",
					pid);
			kfree(fake_pgd_kern);
			read_unlock(&tasklist_lock);
			return -EINVAL;
		}
		pr_err("Found task_struct with pid: %d\n", task->pid);
		mm = task->mm;
		if (mm == NULL) {
			pr_err("expose_page_table: mm is: %x\n",
					mm);
			kfree(fake_pgd_kern);
			read_unlock(&tasklist_lock);
			return -EFAULT;
		}
	}
	rcu_read_unlock();
	//pr_err("came here 2\n");

	down_read(&mm->mmap_sem);

	pgd_base = mm->pgd;
	//pr_err("came here 2.1\n");
	vma = find_vma(mm, addr);
	if (vma == NULL) {
		pr_err("expose_page_table: vma is NULL\n");
		kfree(fake_pgd_kern);
		up_read(&mm->mmap_sem);
		return -EFAULT;
	}
	vma->vm_flags |= VM_DONTEXPAND;
	//pr_err("came here 2.2\n");

	for(pgd = pgd_base; pgd < pgd_base + PTRS_PER_PGD; pgd++, k++) {
		if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd))) {
			continue;
		}
		//for debugging
		pr_err("came 2.1 count = %d i = %d j = %d k = %d pgd = 0x%x\n",
				pte_count, i, j, k, pgd);

		for (i = 0; i < PTRS_PER_PUD; i++) {
			pud = pud_offset(pgd, PUD_SIZE * i);

			//for debugging
			//pr_err("came 3.0.1 count = %d i = %d j = %d k = %d pud = 0x%x\n",
			//		pte_count, i, j, k, pud);
			if (pud_none(*pud) || pud_bad(*pud)) {
				//pr_err("came 2.2\n");
				continue;
			}
			//for debugging
			//pr_err("came 3.0 count = %d i = %d j = %d k = %d\n",
			//		pte_count, i, j, k);
			for (j = 0; j < PTRS_PER_PMD; j++) {
				pmd = pmd_offset(pud, PMD_SIZE * j);
				if (pmd_none(*pmd) || pmd_bad(*pmd)) {
					//pr_err("came 2.3\n");
					//for debugging
					//up_read(&mm->mmap_sem);
					//kfree(fake_pgd_kern);
					//return -1;
					continue;
				}

				if (pte_count >= PTRS_PER_PGD) {
					pr_err(
					"pte_count %d exceeds max limit\n",
					pte_count);
					up_read(&mm->mmap_sem);
					kfree(fake_pgd_kern);
					return -EFAULT;
				}
				/* pmd_val will give the base address
				* of the pte page
				*/
				pfn = __pfn_to_phys(__phys_to_pfn
						(pmd_val(*pmd)));
				//pfn = page_to_pfn(pmd_page(*pmd));

				ret = remap_pfn_range(vma,
						(addr + (pte_count*PAGE_SIZE)),
						pfn, PAGE_SIZE,
						vma->vm_page_prot);
				if (ret) {
					pr_err("remap_pfn_range failed: ret: %d\n",
							ret);
					pr_err(" pfn=%d, i=%d, j=%d, k=%d\n",
						pfn, i, j, k);
					up_read(&mm->mmap_sem);
					kfree(fake_pgd_kern);
					return -EFAULT;
				}
				//add_fake_pgd_entry(fake_pgd_kern, addr + (pte_count*PAGE_SIZE), pte_count);
				pr_err("Inserting number %d into fake pgd at index: %d\n",
						pte_count, k);
				fake_pgd_kern[k] =
					(addr + (pte_count*PAGE_SIZE));

				pte_count++;
			}
		}
		//for debugging
		//break;
	}
	pr_err("came 3.1 count = %d i = %d j = %d k = %d\n",
			pte_count, i, j, k);
	up_read(&mm->mmap_sem);

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
