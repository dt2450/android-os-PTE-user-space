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
	struct mm_struct *curr_mm;
	struct vm_area_struct *vma;
	pgd_t *pgd = NULL;
	pud_t *pud = NULL;
	pmd_t *pmd = NULL;
	unsigned long *fake_pgd_kern = NULL;
	int i = 0, j = 0, k = 0, pte_count = 0;
	int my_pid = 0;
	unsigned long pfn = 0;
	struct page *current_page = NULL;

	pgd_t *pgd_base;
	int ret;

	printk("pid: %d\n", pid);
	printk("fake_pgd: %lu\n", fake_pgd);
	printk("addr: %lu\n", addr);

	if (pid < -1) {
		pr_err("expose_page_table: invalid pid: %d\n", pid);
		return -EINVAL;
	}

	//TODO access_ok for addr
	rcu_read_lock();
	if (pid == -1) {
		pid = current->pid;
		my_pid = 1;
	}
	task = find_process_by_pid(pid);
	pr_err("task is: %x\n", task);
	if (task == NULL) {
		pr_err("expose_page_table: No task with pid: %d\n",
				pid);
		rcu_read_unlock();
		return -EINVAL;
	}
	rcu_read_unlock();

	curr_mm = get_task_mm(current);
	if (curr_mm == NULL) {
		pr_err("expose_page_table: curr_mm is: %x\n",
				curr_mm);
		return -EFAULT;
	}
	if (my_pid)
		mm = curr_mm;
	else {
		pr_err("Found task_struct with pid: %d\n", task->pid);
		mm = get_task_mm(task);
		if (mm == NULL) {
			pr_err("expose_page_table: mm is: %x\n",
					mm);
			mmput(curr_mm);
			return -EFAULT;
		}
	}

	down_write(&curr_mm->mmap_sem);
	if (!my_pid) {
		down_read(&mm->mmap_sem);
	}

	pgd_base = mm->pgd;
	vma = find_vma(curr_mm, addr);
	if (vma == NULL) {
		pr_err("expose_page_table: vma is NULL\n");
		up_write(&curr_mm->mmap_sem);
		mmput(curr_mm);
		if (!my_pid) {
			up_read(&mm->mmap_sem);
			mmput(mm);
		}
		return -EFAULT;
	}

	vma->vm_flags |= VM_DONTEXPAND;

	fake_pgd_kern = kcalloc(PTRS_PER_PGD,
			sizeof(unsigned long), GFP_KERNEL);
	if (fake_pgd_kern == NULL) {
		up_write(&curr_mm->mmap_sem);
		mmput(curr_mm);
		if (!my_pid) {
			up_read(&mm->mmap_sem);
			mmput(mm);
		}
		return -ENOMEM;
	}

	for(pgd = pgd_base; pgd < pgd_base + PTRS_PER_PGD; pgd++, k++) {
		if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd))) {
			continue;
		}
		//for debugging
		//pr_err("came 2.1 count = %d i = %d j = %d k = %d pgd = 0x%x\n",
		//		pte_count, i, j, k, pgd);

		for (i = 0; i < PTRS_PER_PUD; i++) {
			pud = pud_offset(pgd, PUD_SIZE * i);

			if (pud_none(*pud) || pud_bad(*pud)) {
				//pr_err("came 2.2\n");
				continue;
			}
			for (j = 0; j < PTRS_PER_PMD; j++) {
				pmd = pmd_offset(pud, PMD_SIZE * j);
				if (pmd_none(*pmd) || pmd_bad(*pmd)) {
					continue;
				}

				if (pte_count >= PTRS_PER_PGD) {
					pr_err(
					"pte_count %d exceeds max limit\n",
					pte_count);
					up_write(&curr_mm->mmap_sem);
					mmput(curr_mm);
					if (!my_pid) {
						up_read(&mm->mmap_sem);
						mmput(mm);
					}
					kfree(fake_pgd_kern);
					return -EFAULT;
				}

				//==============================
				unsigned int *v, diff;
				v = (unsigned int *)pgd;
				diff = v[1] - v[0];
				if (diff != 1024)
					pr_err("Diff: %d\n", diff);
				//==============================
				//pfn = __pfn_to_phys(__phys_to_pfn
				//		(pmd_val(*pmd)));
				current_page = pmd_page(*pmd);
				atomic_inc(&current_page->_count);
				pfn = page_to_pfn(current_page);

				ret = remap_pfn_range(vma,
						(addr + (pte_count*PAGE_SIZE)),
						pfn, PAGE_SIZE,
						vma->vm_page_prot);

				atomic_dec(&current_page->_count);

				if (ret) {
					pr_err("remap_pfn_range failed: ret: %d\n",
							ret);
					pr_err(" pfn=%d, i=%d, j=%d, k=%d\n",
						pfn, i, j, k);
					up_write(&curr_mm->mmap_sem);
					mmput(curr_mm);
					if (!my_pid) {
						up_read(&mm->mmap_sem);
						mmput(mm);
					}
					kfree(fake_pgd_kern);
					return -EFAULT;
				}
				pr_err("Inserting number %d into fake pgd at index: %d\n",
						pte_count, k);
				fake_pgd_kern[k] =
					(addr + (pte_count*PAGE_SIZE));

				pte_count++;
			}
		}
	}
	pr_err("came 3.1 count = %d i = %d j = %d k = %d\n",
			pte_count, i, j, k);
	up_write(&curr_mm->mmap_sem);
	mmput(curr_mm);
	if (!my_pid) {
		up_read(&mm->mmap_sem);
		mmput(mm);
	}

	pr_err("came here 8\n");
	if (copy_to_user((unsigned long *)fake_pgd, fake_pgd_kern,
				PTRS_PER_PGD*sizeof(unsigned long)))
	{
		pr_err("expose_page_table: copy_to_user");
		pr_err(" failed to copy fake_pgd\n");
		kfree(fake_pgd_kern);
		return -EFAULT;
	}

	pr_err("came here 9\n");
	kfree(fake_pgd_kern);
	return 0;
}
