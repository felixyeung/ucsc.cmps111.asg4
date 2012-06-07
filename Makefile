all :
	gcc -o protectfile protectfile.c
	
testencrypt :
	gcc -o protectfile protectfile.c
	./protectfile test.txt e 123456789ABCDEF0
	
testdecrypt :
	gcc -o protectfile protectfile.c
	./protectfile test.txt d 123456789ABCDEF0