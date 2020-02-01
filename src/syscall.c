#include <syscall.h>

void sys_write_int(long long x)
{
	asm("addi a7, zero, 0");
	asm("ecall");
}

void sys_write_chr(char x)
{
	asm("addi a7, zero, 1");
	asm("ecall");
}

void sys_write_str(char *str)
{
	asm("addi a7, zero, 2");
	asm("ecall");
}

long long sys_read_int()
{
    long long result;
	asm("addi a7, zero, 3");
	asm("ecall");
    asm("addi %0, a0, 0" : "=r" (result));
}

char sys_read_chr()
{
    char result;
	asm("addi a7, zero, 4");
	asm("ecall");
    asm("addi %0, a0, 0" : "=r" (result));
}

void sys_exit(int x)
{
	asm("addi a7, zero, 93");
	asm("ecall");
}
