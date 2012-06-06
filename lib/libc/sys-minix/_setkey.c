#include <lib.h>
#include <unistd.h>

PUBLIC int setkey (unsigned int k0, unsigned k1) {
	message m;
	m.m2_i1 = k0;
	m.m2_i2 = k1;
	return(_syscall(VFS_PROC_NR, 58, &m));
}