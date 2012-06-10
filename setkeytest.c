/* 
 * Adam Poit (apoit@ucsc.edu)
 * Shang Hua Wu (swu14@ucsc.edu)
 * Felix Yeung (fyeung@ucsc.edu)
 */

#include <stdio.h>
#include <setkey.h>
#include <strings.h>
#include <stdlib.h>

int main(int argc, char** argv) {
	unsigned int k0;
	unsigned int k1;
	if (argc != 2) {
		printf("Usage: setkeytest <key>\n");
		exit(1);
	}
	if (strlen(argv[1]) > 16 || strlen(argv[1]) == 0) {
		printf("Key must be at most 16 characters long. Received %d.\n",
				strlen(argv[1]));
		exit(1);
	}
	char finalKey[17];
	char* strKey;
	if (strlen(argv[1]) < 16) {
		int ctr = 0;
		int ctr2 = 0;
		while (ctr < (16 - strlen(argv[1]))) {
			finalKey[ctr] = '0';
			ctr++;
		}
		while (ctr < 16) {
			finalKey[ctr] = argv[1][ctr2];
			ctr2++;
			ctr++;
		}
		finalKey[16] = '\0';
		strKey = finalKey;
	}
	else strKey = argv[1];
	
	printf("KEY: %s\n", strKey);
	
	char strK0[9];
	char strK1[9];
	
	strncpy(strK0, strKey, 8);
	strncpy(strK1, strKey+8, 8);
	
	char* p;
	k0 = strtoul(strK0, &p, 16);
	if (*p != 0) {
		printf("String cannot be converted into hex :(.\n");
		exit(1);
	}
	printf("KEY0: %x(%u)\n", k0, k0);
	
	k1 = strtoul(strK1, &p, 16);
	if (*p != 0) {
		printf("String cannot be converted into hex :(.\n");
		exit(1);
	}
	printf("KEY1: %x(%u)\n", k1, k1);
	
	setkey(k0, k1);
	return 0;
}
