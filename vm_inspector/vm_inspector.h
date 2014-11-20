#ifndef __VM_INSPECTOR_H
#define __VM_INSPECTOR_H

#include <linux/const.h>

#define syscall_no_expose_page_table	378
#define MMAP_FILE_BASE	"/dev/zero"
#define MAX_FILE_SIZE	16
#define PAGE_SIZE	4096
#define MMAP_SIZE	(2048 * PAGE_SIZE)
#define MAX_PGD_ENTRIES	2048
#define MAX_PTE_ENTRIES	(PAGE_SIZE / sizeof(unsigned long))
#define VERBOSE_OPTION	"-v"

#define UL(x) _AC(x, UL)
#define PAGE_SHIFT              12
#define TASK_SIZE               UL(3204448256)

#define L_PTE_PRESENT           (_AT(unsigned long, 1) << 0)
#define L_PTE_YOUNG             (_AT(unsigned long, 1) << 1)
#define L_PTE_FILE              (_AT(unsigned long, 1) << 2)
#define L_PTE_DIRTY             (_AT(unsigned long, 1) << 6)
#define L_PTE_RDONLY            (_AT(unsigned long, 1) << 7)
#define L_PTE_USER              (_AT(unsigned long, 1) << 8)
#define L_PTE_XN                (_AT(unsigned long, 1) << 9)
#define L_PTE_SHARED            (_AT(unsigned long, 1) << 10)

#define pte_none(pte)           (!pte)
#define pte_present(pte)        (pte & L_PTE_PRESENT)
#define pte_write(pte)          (!(pte & L_PTE_RDONLY))
#define pte_dirty(pte)          (pte & L_PTE_DIRTY)
#define pte_young(pte)          (pte & L_PTE_YOUNG)
#define pte_exec(pte)           (!(pte & L_PTE_XN))
#define pte_file(pte)           ((pte & L_PTE_FILE))


#define PTRS_PER_PTE		512
#define PGDIR_SHIFT		21
#define pgd_index(addr)         ((addr) >> PGDIR_SHIFT)
#define pte_index(addr)         (((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

#endif /* __VM_INSPECTOR_H */
