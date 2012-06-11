/* Adam Poit (apoit@ucsc.edu)
 * Shang Hua Wu (swu14@ucsc.edu)
 * Felix Yeung (fyeung@ucsc.edu)
 */

#include <lib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>

PUBLIC int setkey(unsigned int k0, unsigned int k1) {
	printf("setkey.h k0: %u k1: %u\n", k0, k1);
	message m;
	m.m2_i1 = k0;
	m.m2_i2 = k1;
	return(_syscall(VFS_PROC_NR, 58, &m));
}