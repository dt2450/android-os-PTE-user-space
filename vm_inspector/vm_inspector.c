#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#define syscall_no_expose_page_table	378
#define MMAP_FILE_BASE	"/dev/zero"
#define MAX_FILE_SIZE	16
#define PAGE_SIZE	4096
#define MMAP_SIZE	2048*PAGE_SIZE
#define MAX_PGD_ENTRIES	2048

static int verbose;
static int pid = -1;
#define VERBOSE_OPTION	"-v"

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
	int i;
	int fd = -1;
	void *mmap_addr = NULL;
	unsigned long *pgd_addr = NULL;
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

	pgd_addr = (unsigned long *) malloc(PAGE_SIZE*4);

	if (pgd_addr == NULL) {
		printf("error in allocating memory\n");
		return -1;
	}

	ret = syscall(syscall_no_expose_page_table, pid, pgd_addr, mmap_addr);
	printf("Ret: %d mmap_addr = %u, pgd_addr = %u\n",
			ret, (unsigned int)mmap_addr, (unsigned int)pgd_addr);

	if (ret != 0) {
		printf("Syscall failed with error: %s\n", strerror(errno));
		return -1;
	}

	printf("mmap start: %x\n", (unsigned int)mmap_addr);
	printf("mmap end: %x\n", (unsigned int)(mmap_addr + MMAP_SIZE));
	for (i=0; i<MAX_PGD_ENTRIES; i++) {
		if (((void *)pgd_addr[i]) != NULL) {
			printf("fake_pgd[%d]: %x\n", i,
					(unsigned int)(pgd_addr[i]));
		}
	}
/*
	ret = close(fd);
	if (ret != 0) {
		printf("Error in closing fd\n");
		return -1;
	}
	ret = remove(unique_file_name);
	if (ret != 0) {
		printf("Error in removing file\n");
		return -1;
	}
*/
	return 0;
}
