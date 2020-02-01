#ifndef SYSCALL_HEADER
#define SYSCALL_HEADER

// syscall num saved in register a7(x17)
// write syscall
void sys_write_int(long long x); 	// 0
void sys_write_chr(char x);		// 1
void sys_write_str(char *x);	// 2

// read syscall
long long sys_read_int();			// 3
char sys_read_chr();			// 4

// exit syscall
void sys_exit(int x);			// 93

#endif
