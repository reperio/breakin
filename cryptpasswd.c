#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <crypt.h>
     
int main(int argc, char **argv) {
	char salt[2];
	char *password;

	int i;

	snprintf(salt, 3, "%s", argv[1]);

	/* printf("Salt %s Passwd %s\n", salt, argv[1]); */
     
     	
	/* Read in the user's password and encrypt it. */
	password = crypt(argv[1], salt);
     
	/* Print the results. */
	puts(password);
	return 0;
}
