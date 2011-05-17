#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define N  10

/* reverse:  reverse string s in place */
void reverse(char s[])
{
	int i, j;
	char c;

	for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}

/* itoa:  convert n to characters in s */
void itoa(int n, char s[])
{
	int i, sign;

	if ((sign = n) < 0)  /* record sign */
		n = -n;          /* make n positive */
	i = 0;
	do {       /* generate digits in reverse order */
		s[i++] = n % 10 + '0';   /* get next digit */
	} while ((n /= 10) > 0);     /* delete it */
	if (sign < 0)
		s[i++] = '-';
	s[i] = '\0';
	reverse(s);
}

char *
strcat(char *dest, const char *src)
{
	int i,j;
	for (i = 0; dest[i] != '\0'; i++)
		;
	for (j = 0; src[j] != '\0'; j++)
		dest[i+j] = src[j];
	dest[i+j] = '\0';
	return dest;
}

int
main(void)
{
	int n,pid,i,j,k;

	printf(1,"sanity test\n");

	for(n=1; n<N+1; n++) {
		pid = fork();
		if(pid == 0) {//children
			for(j=0;j<1;j++) {
				for(k=0;k<4096;k++) {
					for(i=0;i<n;i++) {
						memset(malloc(1),0,1);
					}
				}
			}
			char* filename = "proc";
			char str[11];
			itoa(getpid(),str);
			filename = strcat(filename,str);
			int fd = open(filename,O_CREATE | O_RDWR);
			if(fd<0) {
				printf(2,"ERROR");
			}
			else {
			for(j=1000;j>0;j--) {
				printf(fd,"process: %d, num: %d\n",getpid(),j);
			}
			close(fd);
			}
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
	char* filename = "proc";
	char str[11];
	itoa(getpid(),str);
	filename = strcat(filename,str);
	int fd = open(filename,O_CREATE | O_RDWR);
	for(j=1000;j>0;j--) {
		printf(fd,"process: %d, num: %d\n",getpid(),j);
	}
	close(fd);
	exit();
}

