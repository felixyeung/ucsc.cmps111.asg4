#include <stdio.h>
#include <setkey.h>

int main(void) {
	unsigned int one = 3;
	unsigned int two = 6;
	unsigned int three = 4;
	unsigned int four = 99;
	unsigned int zip = 0;
	int i = setkey2(one, two);
	i = setkey2(three, four);
	//i = setkey2(zip, zip);
	printf("return %d\n", i);

}
