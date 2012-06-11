/* Adam Poit (apoit@ucsc.edu)
 * Shang Hua Wu (swu14@ucsc.edu)
 * Felix Yeung (fyeung@ucsc.edu)
 */

#ifndef _PROTECT_H_
#define _PROTECT_H_

struct keyValuePair {
	uid_t user;
	unsigned int k0;
	unsigned int k1;
};

extern struct keyValuePair pairs[8];

#endif