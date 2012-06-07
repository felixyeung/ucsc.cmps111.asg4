#include <lib.h>
#include <unistd.h>
#include <stdio.h>

PUBLIC int setkey(unsigned int k0, unsigned int k1) {
	printf("in _setkey.c\n");
	message m;
	m.m2_i1 = k0;
	m.m2_i2 = k1;
	return(_syscall(VFS_PROC_NR, 58, &m));
}