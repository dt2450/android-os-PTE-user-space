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
#define MMAP_FILE_BASE	"./_mmap"
#define MAX_FILE_SIZE	16
#define MMAP_SIZE	4096

int main(int argc, char **argv)
{
	int ret;
	int fd = -1;
	void *mmap_addr = NULL;
	void *pgd_addr = NULL;
	char unique_file_name[MAX_FILE_SIZE] = "";

	snprintf(unique_file_name, MAX_FILE_SIZE, "%s.%d", MMAP_FILE_BASE,
			getpid());

	fd = open(unique_file_name, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);

	if (fd == -1) {
		printf("Open failed with error: %s\n", strerror(errno));
		return -1;
	}

	mmap_addr = mmap(0, MMAP_SIZE*2, PROT_READ, MAP_PRIVATE, fd, 0);

	if (mmap_addr == MAP_FAILED) {
		printf("mmap failed with error: %s\n", strerror(errno));
		return -1;
	}

	pgd_addr = malloc(MMAP_SIZE*4);

	if (pgd_addr == NULL) {
		printf("error in allocating memory\n");
		return -1;
	}

	ret = syscall(syscall_no_expose_page_table, -1, pgd_addr, mmap_addr);
	printf("Ret: %d mmap_addr = %u, pgd_addr = %u\n",
			ret, (unsigned int)mmap_addr, (unsigned int)pgd_addr);

	return 0;
}
