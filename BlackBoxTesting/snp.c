#include<stdio.h>
#include<unistd.h>
int mainChild()
{
	printf("Hello");
}
int main(int argc,char* argv[])
{
	pid_t pid;
	int i;
	for(i=0;i<3;i++)
	{
		pid=fork();
		if(pid==0)
		{
			mainChild();
			exit(0);
		}
	}
	wait();
	printf("Done");
	return 1;
}