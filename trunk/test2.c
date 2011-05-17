#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
	int j=0;
	int i=0;
	for(j=0;j<10;j++) {
		int pid = fork();
		if(pid == 0) {
			for(i=0;i<50;i++) {
				printf(1,"process: %d, num: %d\n",getpid(),i);
				sleep(10);
			}
			exit();
		}
	}
	wait();
	exit();
}

