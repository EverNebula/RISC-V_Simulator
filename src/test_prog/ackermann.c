#include <syscall.h>

char err_str[50] = "Error.";
char str1[50] = "To solve Ackermann(a, b), please input a:";
char str2[50] = "please input b:";

long long Ackermann (int m, int n)
{
	if(m == 0)
		return (n + 1);
	if( m > 0 && n == 0 )
		return Ackermann(m - 1, 1);
	if( m > 0 && n > 0 )
		return Ackermann( m - 1, Ackermann(m, n - 1) );
	sys_write_str(err_str);
	return 0;
}
 
int main ()
{
	int a, b;
	sys_write_str(str1);
	a = sys_read_int();
	sys_write_str(str2);
	b = sys_read_int();
	sys_write_int(Ackermann(a, b));
	sys_write_chr('\n');
	return 0;
}
