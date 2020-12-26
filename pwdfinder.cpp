/* Программное средство pwdfinder используется для дешифрования пароля Unix 
 * методом перебора всех строк из файла со списком всех возможных паролей. 
 * Каждая строка файла шифруется с помощью функции crypt(), результат сравнивается с исходным паролем. 
 * Если обнаруживается совпадение, пароль считается вскрытым. 
 * Если перебраны все строки из файла, и совпадение не обнаружено, то пароль считается невскрытым.
 *
 * Использование: mpirun -np <p> <pwdfinder> <input_dictionary> <password_hash>
 *
 * Программа принимает аргументы:
 * argv[1]				- Имя входного файла с паролями для перебора
 * argv[2]				- Хэш, который необходимо дешифровать
 */ 

#include <stdio.h>//		Для вывода информации (printf, fprintf, sprintf)
#include <mpi.h>//			Для работы функционала MPI
#include <ctype.h> //		Для функций isalpha, isdigit, isspace
#include <string.h> //		Для использования strlen
#include <string> //		Для использования std::string
#include <crypt.h> //		Для использования crypt()
#include <errno.h>//		Для использования глобальной переменной errno

/* Функция разбивает входной файл на части (chunks). 
 * Затем каждый процесс параллельно считывает свою порцию данных.
 * При этом к каждой строке входных данных применяется функция crypt.
 * 
 * Функция принимает параметры:
 * MPI_File *in			- Указатель на входной файл для параллельного чтения
 * const int rank		- Ранг для идентификации процесса
 * const int proc_cnt	- Общее количество процессов
 * const int delta	- Размер "пересечения" (для корректного разделения по строкам)
 * const char *pwd_hash	- Хэш от пароля, который необходимо вскрыть
 */ 
std::string process_in_chunks(MPI_File *in, const int rank, const int proc_cnt, const int delta, char *pwd_hash) {
	MPI_Request request;
	
	std::string word = "";
	
	MPI_Offset globalstart;
	MPI_Offset globalend;
	MPI_Offset filesize;
	MPI_Offset chunk_size;
	int flag_msg = 0;
	int flag_len_err = 0;
	int flag_sym_err = 0;	
	
	char *cmp_res = new char[13];
	char c;
	//Массив разрешенных символов
	const char allowed_sym[] = {'!','"','#','$','%',' ','&','\'','(',')','*','+','-','.','/',':',';', \
								'<','=','>','?','@','[','\\',']','^','_','`','{','|','}','~'};

	
	MPI_File_get_size(*in, &filesize);

	//Файл делится на части, основываясь на рангах
	chunk_size = filesize / proc_cnt;
	globalstart = rank * chunk_size;
	globalend   = globalstart + chunk_size - 1;
	
	if (rank == proc_cnt-1) globalend = filesize-1;
	
	//Добавить дополнительный интервал, чтобы не обрезать строку с паролем
	if (rank != proc_cnt-1) globalend += delta;

	chunk_size =  globalend - globalstart + 1;

	char *chunk = new char[chunk_size + 1];
	
	//Чтение данных в переменную chunk
	MPI_File_read_at_all(*in, globalstart, chunk, chunk_size, MPI_CHAR, MPI_STATUS_IGNORE);
	chunk[chunk_size] = '\0';

    int locstart=0, locend=chunk_size-1;
	
	//Обрезаем лишние данные, порция данных начинается с \n  и заканчивается \n (кроме начала и конца файла)
    if (rank != 0) {
        while(chunk[locstart] != '\n') locstart++;
        locstart++;
    }
    if (rank != proc_cnt-1) {
        locend-=delta;
        while(chunk[locend] != '\n') locend++;
    }
	
    chunk_size = locend-locstart+1;

	int i=locstart;
	
	//Посимвольный перебор порции данных
    while(i<=locend) {
		
		//Сообщение о необходимости завершения работы (пароль найден)
		MPI_Iprobe( MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &flag_msg, MPI_STATUS_IGNORE);
		if (flag_msg == 1) break;
		
		c = chunk[i];
		int iter = 0;
		
		//В цикле выделяются слова
		while(!isspace(c) || chunk[i] != '\n') {
			word += c;
			i++;
			iter++;
			
			//Проверка на превышение длины
			if (iter >= 40) flag_len_err = 1;
			
			//Проверка на наличие посторонних символов
			if (!isalpha(c) & !isdigit(c)) {
				if (!flag_sym_err)
					for (int k = 0; k < 31; k++) {
						if (allowed_sym[k] == c) {
							flag_sym_err = 0; 
							break;
						}
						else flag_sym_err = 1;
					}
			}
			
			//Проверка на конец файла
			if (i>locend) break;
			c = chunk[i];
		}
		
		iter = 0;
		
		if (flag_len_err) {
			fprintf(stderr, "\nInput string %s is missed (Length error)\n", word.c_str());
			flag_len_err = 0;
		}
		else if (flag_sym_err) {
			fprintf(stderr, "\nInput string %s is missed (Bad symbols error)\n", word.c_str());
			flag_sym_err = 0;
		}
		else {
			//Шифрование слова
			strcpy( cmp_res , crypt(word.c_str(),pwd_hash) );

			//Сравнение результатов
			if (strcmp(pwd_hash, cmp_res) == 0) {
				
				//Сигнал к завершению работы другими процессами
				for(int i = 0; i < proc_cnt; i++) 
					if (i != rank)
						MPI_Isend( &rank, 1, MPI_INT, i, 1, MPI_COMM_WORLD, &request);
				
				delete chunk;
				delete cmp_res;
				
				return word;
			}
		}
		word = "";
		i++;
    }
	
	delete chunk;
	delete cmp_res;

    return "";
}

int main(int argc, char **argv) {
    MPI_File in, out;
	
    int rank, proc_cnt;
    int ierr;
	char *pwd_hash = new char[13];
    const int delta = 100;
	double starttime, endtime;
	std::string res;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &proc_cnt);

	//Проверка количества входных аргументов	
    if (argc != 3) {
        if (rank == 0)  fprintf(stderr, "Usage: %s infilename password_hash\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

	//Открытие входного файла
    ierr = MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_RDONLY, MPI_INFO_NULL, &in);
    if (ierr) {
        if (rank == 0)  fprintf(stderr, "%s: Couldn't open file %s  (%s)\n", argv[0], argv[1], strerror(errno));
        MPI_Finalize();
        return 2;
    }

	//Удалить выходной файл, если уже существует
    MPI_File_open(MPI_COMM_WORLD, "result", MPI_MODE_CREATE|MPI_MODE_DELETE_ON_CLOSE|MPI_MODE_WRONLY, MPI_INFO_NULL, &out);
    MPI_File_close(&out);
	
	//Открытие выходного файла
	ierr = MPI_File_open(MPI_COMM_WORLD, "result", MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &out);
    if (ierr) {
        if (rank == 0)  fprintf(stderr, "%s: Couldn't open file 'result'  (%s)\n", argv[0], strerror(errno));
        MPI_Finalize();
        return 3;
    }

	//В алгоритме DES функциии crypt выходной хэш имеет фиксированную длину - 13 символов
	if ((strlen(argv[2]) != 13)) {
        if (rank == 0)  fprintf(stderr, "Incorrect hash %s Size should be 13 symbols (%ld given)\n", argv[2], strlen(argv[2]));
        MPI_Finalize();
        return 4;
    }

	//Обработка данных по частям
	starttime = MPI_Wtime();
    res = process_in_chunks(&in, rank, proc_cnt, delta, argv[2]);
	endtime = MPI_Wtime();
	MPI_File_close(&in);
	
	//Пишет процесс, который нашел пароль
	if (res != "") {
		char out_buffer[70];
		int n = sprintf(out_buffer,"Password is: %s\nFound on rank: %d\n", res.c_str(), rank);
        MPI_File_write(out,out_buffer,n, MPI_CHAR,MPI_STATUS_IGNORE);
	}
	
	//Дождаться, пока данные будут записаны
	MPI_Barrier(MPI_COMM_WORLD);
	
	//Дописать время выполнения программы
	if (rank == 0) {
		char out_buffer[70];
		MPI_File_seek( out, 0, MPI_SEEK_END ); 
		int n = sprintf(out_buffer,"Searching time is: %.2f\n", endtime-starttime);
        MPI_File_write(out,out_buffer,n, MPI_CHAR,MPI_STATUS_IGNORE);
	}
    
	delete pwd_hash;
	MPI_File_close(&out);
	MPI_Finalize();

    return 0;
}