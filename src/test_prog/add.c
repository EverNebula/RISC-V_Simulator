#include<stdio.h>
#include <syscall.h>

int result[10]={1,2,3,4,5,6,7,8,9,10};

//result:11 12 13 14 15 1 2 3 4 5

int main()
{
	int i=0;
	for(i=0;i<5;i++)
		result[i]=result[i]+10;
	for(i=5;i<10;i++)
		result[i]=result[i]-5;

	// output
	for (int i = 0; i < 10; i++)
	{
		sys_write_int(result[i]);
		sys_write_chr(' ');
	}
	sys_write_chr('\n');
	
	return 0;
}
