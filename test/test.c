#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/syscall.h>
#include <errno.h>

#define __NR_expose_page_table	378

int main(int argc, char **argv)
{
	int ret;

	ret = syscall(__NR_expose_page_table, 111, 222, 333);
	printf("Ret: %d\n", ret);

	return 0;
}
