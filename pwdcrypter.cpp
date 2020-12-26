/* Программа шифрует пароль функцией crypt(). 
 *
 * Использование: ./pwdcrypter <password>
 *
 * Программа принимает аргументы:
 * password		- 	Пароль, который необходимо зашифровать.
 */ 
 
#include <crypt.h>
#include <string>
#include <stdio.h>

int main(int argc, char **argv) {
	//Вывод ошибки и выход при неверном количестве входных аргументов	
    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s password\n\n", argv[0]);
        return 1;
    }
	
	std::string password = argv[1];
	//Вывод ошибки и выход при неверной длине пароля
	if ( password.length() > 40) {
        fprintf(stderr, "\nWrong password. Max length is 40.\nCharacters allowed: A-Za-z0-9space!\"#$%%&'()*+,-./:;<=>?@[\\]^_`{|}~\n\n");
        return 2;
    }
	//Вывод результата работы программы
	fprintf( stdout, "\nResult:[%s]\n\n", crypt( argv[1], "any./Sa1t/") );
    
    return 0;
}