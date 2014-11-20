#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include "vm_inspector.h"

static int verbose = 0;
static int pid = -1;

static void set_verbose_or_pid(char *s)
{
	if (strcmp(s, VERBOSE_OPTION) == 0)
		verbose = 1;
	else
		pid = atoi(s);
}

static void validate_args(int argc, char **argv)
{
	if (argc > 3) {
		printf("Usage: ./vm_inspector [-v] [pid]\n");
		exit(-1);
	}

	if (argc == 2) {
		set_verbose_or_pid(argv[1]);
	} else if (argc == 3) {
		set_verbose_or_pid(argv[1]);
		set_verbose_or_pid(argv[2]);
	}
}

int main(int argc, char **argv)
{
	int ret;
	int i, j;
	int fd = -1;
	int should_print = 0;
	void *mmap_addr = NULL;
	unsigned long *pte_base = NULL;
	unsigned long page_phy_addr = NULL;
	unsigned long pte_entry;
	unsigned long *fake_pgd_base = NULL;
	char unique_file_name[MAX_FILE_SIZE] = "";

	validate_args(argc, argv);
	printf("Pid: %d, verbose: %d\n", pid, verbose);
	snprintf(unique_file_name, MAX_FILE_SIZE, "%s.%d", MMAP_FILE_BASE,
			getpid());

	fd = open(MMAP_FILE_BASE
			, O_RDONLY);

	if (fd == -1) {
		printf("Open failed with error: %s\n", strerror(errno));
		return -1;
	}

	mmap_addr = mmap(0, MMAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);

	if (mmap_addr == MAP_FAILED) {
		printf("mmap failed with error: %s\n", strerror(errno));
		return -1;
	}

	fake_pgd_base = (unsigned long *) malloc(MAX_PGD_ENTRIES*
			sizeof(unsigned long));

	if (fake_pgd_base == NULL) {
		printf("error in allocating memory\n");
		return -1;
	}

	ret = syscall(syscall_no_expose_page_table, pid,
			fake_pgd_base, mmap_addr);
	printf("Ret: %d mmap_addr = %u, fake_pgd_base = %u\n",
			ret, (unsigned int)mmap_addr,
			(unsigned int)fake_pgd_base);

	if (ret != 0) {
		printf("Syscall failed with error: %s\n", strerror(errno));
		return -1;
	}

	/*
	 * [index] [virt] [phys] [young bit] [file bit] [dirty bit] [read-only
	 * bit] [xn bit]
	 * e.g.
	 * 0x0 0x00000000 0x530000 1 1 0 1 1
	 * */
	printf("mmap start: %x\n", (unsigned int)mmap_addr);
	printf("mmap end: %x\n", (unsigned int)(mmap_addr + MMAP_SIZE));
	for (i = 0; i < MAX_PGD_ENTRIES; i++) {
		if (((void *)fake_pgd_base[i]) != NULL) {
			pte_base = (unsigned long *)
				(fake_pgd_base[i]);
			printf("======PTE Page %d==========\n", (i+1));
			for (j = 0; j < MAX_PTE_ENTRIES; j++) {
				pte_entry = pte_base[j];
				/* zero out the offset bits 
				 * to get the physical address
				 */
				should_print = (!pte_none(pte_entry)) || verbose;
				if (should_print) {
					page_phy_addr =
					(pte_entry >> PAGE_SHIFT) << PAGE_SHIFT;
					printf("0x%lx 0x%lx 0x%lx %d %d %d %d %d\n"
					,(unsigned long) (fake_pgd_base + i),
					(unsigned long) (pte_base + j),
					(unsigned long) page_phy_addr,
					pte_young(pte_entry)?1:0,
					pte_file(pte_entry)?1:0,
					pte_dirty(pte_entry)?1:0,
					pte_write(pte_entry)?0:1,
					pte_exec(pte_entry)?1:0);
				}
			}
		}
	}
	return 0;
}
