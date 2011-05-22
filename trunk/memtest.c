#include "types.h"
#include "stat.h"
#include "user.h"

#define N  10

int
main(void)
{
	int n,pid,i;

	printf(1,"memtest\n");
	for(n=1; n<N+1; n++) {
		pid = fork();
		if(pid == 0) {//children
			for(i=0;i<1000;i++) {
				if(n%2 == 0) {
					malloc(1);
				}
				sleep(1);
			}
			exit();
		}
		else {//parent
			continue;
		}
	}
	for(n=0; n<N; n++) {
		if(wait() == -1) { //error
			printf(1,"wait error");
			exit();
		}
	}
	exit();
}
