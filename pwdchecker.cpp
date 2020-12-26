#include <crypt.h>
#include <string>
#include <stdio.h>


int main(int argc, char **argv) {
	
    if (argc != 3) {
        fprintf(stderr, "\nUsage: %s password hash\n\n", argv[0]);
        return 1;
    }

	
	fprintf( stdout, "\nResult:[%s]\n\n", crypt( argv[1], argv[2]) );
    
    return 0;
}