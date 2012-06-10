#include <stdio.h>
#include <setkey.h>

int main (void) {
	unsigned int one = 18;
	unsigned int two = 878082202;
	setkey(one, two);
	return 0;
}