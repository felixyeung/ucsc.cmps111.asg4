#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
	char buffer[256];
	FILE* in;
	FILE* tmp;
	char dMode[] = "0777"; //plain text file permissions
	char eMode[] = "1777"; //encrypted file permissions
	int mode;
	char option;
	
	if (argc != 3) {
		printf("Usage: protectfile <file> [e|d]\n");
		exit(1);
	}
	
	/*try to open our file*/
	in = fopen(argv[1], "r+");
	if (in == NULL) {
		printf("Cannot open target.\n");
		exit(1);
	}
	
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
	
	/*open a temp file*/
	tmp = tmpfile();
	
	/* chmod the tmp file to 1777 */
    mode = strtol(dMode, 0, 8);
	
	/* extract the file descriptor from the stream 
	 * then fchmod it
	 */
	if (fchmod(fileno(tmp), mode) < 0) {
		fprintf(stderr, "%s: error in fchmod(%s, %s) - %d (%s)\n",
				argv[0], fileno(tmp), dMode, errno, strerror(errno));
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
	if (option = 'd')
		mode = strtol(dMode, 0, 8);
	else
		mode = strtol(eMode, 0, 8);
		
	if (fchmod(fileno(in), mode) < 0) {
		fprintf(stderr, "%s: error in fchmod(%s, %s) - %d (%s)\n",
				argv[0], fileno(in), dMode, errno, strerror(errno));
		exit(1);
	}
	
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