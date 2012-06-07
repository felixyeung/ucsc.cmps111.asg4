#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
//#include <setkey.h>

int main(int argc, char *argv[]) {
	char buffer[256];
	FILE* in;
	FILE* tmp;
	char option;
	struct stat inStat;
	int mode;
	int user;
	unsigned int k0;
	unsigned int k1;
	
	if (argc != 4) {
		printf("Usage: protectfile <file> <e|d> <key>\n");
		exit(1);
	}
	
	/*try to open our file*/
	in = fopen(argv[1], "r+");
	if (in == NULL) {
		printf("Cannot open target.\n");
		exit(1);
	}
	
	/* check for options */
	if (strcmp(argv[2], "e") == 0) {
		option = 'e';
	}
	else if (strcmp(argv[2], "d") == 0) {
		option = 'd';
	}
	else {
		printf("Option (%s) must be 'e' or 'd'.\n", argv[2]);
		exit(1);
	}
	
	/* check for consistency between options and sticky bit */
	if (fstat(fileno(in), &inStat)) {
		printf("Failed to fetch file stat (%s).\n", strerror(errno));
		exit(1);
	}
	
	printf("==>%o\n", inStat.st_mode);
	
	//this is the mask that checks for sticky ness
	if (inStat.st_mode & S_ISVTX) { //sticky = 1
		printf("Sticky\n");
		if (option == 'e') {
			printf("Cannot do encrypt operation on an encrypted file.\n");
			exit(1);
		}
	}
	else { //sticky = 0
		printf("Not Sticky\n");
		if (option == 'd') {
			printf("Cannot do dencrypt operation on a plain text file.\n");
			exit(1);
		}
	}

	/* check to see if our key is good */
	if (strlen(argv[3]) != 16) {
		printf("Key must be 16 characters long. Received %d.\n", strlen(argv[3]));
		exit(1);
	}
	
	/*----------------------------------
	 * This is the end of all our checks
	 ----------------------------------*/
	 
	/* setkey */
	user = getuid();
	printf("Uid: %d\n", user);
	
	char* strKey = argv[3];
	
	char strK0[9];
	char strK1[9];
	
	strncpy(strK0, strKey, 8);
	strncpy(strK1, strKey+8, 8);
	
	char* p;
	k0 = strtoul(strK0, &p, 16);
	if (*p != 0) {
		printf("String cannot be converted into hex :(.");
		exit(1);
	}
	printf("KEY0: %x(%d)\n", k0, k0);
	
	k1 = strtoul(strK1, &p, 16);
	if (*p != 0) {
		printf("String cannot be converted into hex :(.");
		exit(1);
	}
	printf("KEY1: %x(%d)\n", k1, k1);
	
	//setkey(user, k0, k1);
	
	/*open a temp file*/
	tmp = tmpfile();
	
	/* chmod the tmp file to 0777 */
	
	/* extract the file descriptor from the stream 
	 * then fchmod it
	 */
	if (fchmod(fileno(tmp), 01777) < 0) {
		fprintf(stderr, "%s: error in fchmod(%o, %s) - %d (%s)\n",
				argv[0], fileno(tmp), 01777, errno, strerror(errno));
		exit(1);
	}
	
	/* buffer out input file, in, and fputs it into
	   our temp file */
    while(!feof(in)){
		fgets(buffer, 256, in);
		if(fputs(buffer, tmp) != EOF)
			printf("-->Wrote from buffer to tmp:\n      %s\n", buffer);
    }
	
	/* reopen our file with w+ to destroy its content */
	in = fopen(argv[1], "w+");
	
	/* now, chmod our original file */
	if (option == 'd') 
		mode = 0777;
	else if (option == 'e') {
		mode = 01777;
	}
		
	if (fchmod(fileno(in), mode) < 0) {
		fprintf(stderr, "%s: error in fchmod(%o, %s) - %d (%s)\n",
				argv[0], fileno(in), mode, errno, strerror(errno));
		exit(1);
	}
	
	if (fstat(fileno(in), &inStat)) {
		printf("Failed to fetch file stat (%s).\n", strerror(errno));
		exit(1);
	}
	
	printf("END==>%o\n", inStat.st_mode);
	
	/* now, write back from tmp */
	rewind(tmp);
	while(!feof(tmp)){
		fgets(buffer, 256, tmp);
		if(fputs(buffer, in) != EOF)
			printf("-->Wrote from buffer to in:\n      %s\n", buffer);
    }
	
	fclose(in);
	fclose(tmp);
	return 0;
}