#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/syscall.h>
#include <errno.h>


int main(int argc, char **argv)
{
	while(1)
		printf("While 1\n");
	return 0;
}
