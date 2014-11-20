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

static int verbose;
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
	int ret = -1;
	unsigned long virt_addr;
	int fd = -1;
	void *mmap_addr = NULL;
	unsigned long page_phy_addr = NULL;
	unsigned long *fake_pgd_base = NULL;
	char unique_file_name[MAX_FILE_SIZE] = "";



	unsigned long index;
	unsigned long *pte_base;
	unsigned long my_pte_index;
	unsigned long pte_entry;




	validate_args(argc, argv);
	printf("Pid: %d, verbose: %d\n", pid, verbose);
	snprintf(unique_file_name, MAX_FILE_SIZE, "%s.%d", MMAP_FILE_BASE,
			getpid());

	fd = open(MMAP_FILE_BASE
			, O_RDONLY);

	if (fd == -1) {
		printf("Open failed with error: %s\n", strerror(errno));
		return ret;
	}

	mmap_addr = mmap(0, MMAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);

	if (mmap_addr == MAP_FAILED) {
		printf("mmap failed with error: %s\n", strerror(errno));
		return ret;
	}

	fake_pgd_base = (unsigned long *) calloc(MAX_PGD_ENTRIES,
			sizeof(unsigned long));

	if (fake_pgd_base == NULL) {
		printf("error in allocating memory\n");
		return ret;
	}

	ret = syscall(syscall_no_expose_page_table, pid,
			fake_pgd_base, mmap_addr);
	printf("Ret: %d mmap_addr = %u, fake_pgd_base = %u\n",
			ret, (unsigned int)mmap_addr,
			(unsigned int)fake_pgd_base);

	if (ret != 0) {
		printf("Syscall failed with error: %s\n", strerror(errno));
		return ret;
	}

	/*
	* [index] [virt] [phys] [young bit] [file bit] [dirty bit] [read-only
	* bit] [xn bit]
	* e.g.
	* 0x0 0x00000000 0x530000 1 1 0 1 1
	* */
	printf("mmap start: %x\n", (unsigned int)mmap_addr);
	printf("mmap end: %x\n", (unsigned int)(mmap_addr + MMAP_SIZE));

	for (virt_addr = 0; virt_addr < TASK_SIZE; virt_addr += PAGE_SIZE) {
		index = pgd_index(virt_addr);
		pte_base = (unsigned long *) fake_pgd_base[index];
		if (pte_base != NULL) {
			my_pte_index = pte_index(virt_addr);
			pte_entry = pte_base[my_pte_index];
			if (pte_present(pte_entry)) {
				page_phy_addr =
					(pte_entry >> PAGE_SHIFT)
					<< PAGE_SHIFT;
				printf("0x%lx 0x%lx 0x%lx %d %d %d %d %d\n"
						, index,
						virt_addr,
						(unsigned long) page_phy_addr,
						pte_young(pte_entry) ? 1 : 0,
						pte_file(pte_entry) ? 1 : 0,
						pte_dirty(pte_entry) ? 1 : 0,
						pte_write(pte_entry) ? 0 : 1,
						pte_exec(pte_entry) ? 1 : 0);

			} else if (verbose == 1) {
				page_phy_addr =
					(pte_entry >> PAGE_SHIFT)
					<< PAGE_SHIFT;
				printf("0x%lx 0x%lx %d %d %d %d %d %d\n"
						, index,
						virt_addr,
						0, 0, 0, 0, 0, 0);
			}
		}
	}

	return 0;
}
