#include "types.h"
#include "stat.h"
#include "user.h"

#define N  10

int
main(void)
{
	int n,pid,i,j,k;

	printf(1,"sanity test\n");
	int l,m=0;
	for(n=1; n<N+1; n++) {
		pid = fork();
		if(pid == 0) {//children
			for(j=0;j<1;j++) {
				for(k=0;k<4096;k++) {
					for(i=0;i<n;i++) {
						char* mced;
						if((mced = sbrk(0)) >= 0){
							if(sbrk(1) >= 0){
								*mced = 0;
							}
						}
						for(l=0;l<100000;l++) {
							m++;
						}
						//printf(3,"stam");
						//printf(1,"lol");
						//;
					}
					//sleep(1);
				}
			}
			printf(1,"%d",m);
			//printf(1,"lol\n");
			//while(1);
			exit();
		}
		else if(pid > 0) {//parent
			continue;
		}
		else //fork error
			printf(1,"fork error");
		exit();
	}


	//sleep(1000);
	for(n=0; n<N; n++) {
		if(wait() == -1) { //error
			printf(1,"wait error");
			exit();
		}
	}
	exit();
}
