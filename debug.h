#ifndef DEBUG_H
#define DEBUG_H

#define REMOVE_TEXT
// #define WRITE_TO_CONSOLE

#define WRITE(str) {\
	int res = write(1, str, strlen(str));\
	if (res != strlen(str)) {\
		exit(1234);\
	}\
}

#include <mutex>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <atomic>
#include <cstring>

extern std::recursive_mutex debug_mutex;
extern thread_local int num_alive_writers;
extern std::atomic_bool is_constructors_begin_to_executing;

template<typename ...Args>
void lok_print(std::ofstream &out, const Args &...args) {
}

template<typename First, typename ...Other>
void lok_print(std::ofstream &out, const First &first, const Other &...other) {
	out << first;
	lok_print(out, other...);
}

void print_to_console();

template<typename ...Other>
void print_to_console(const char* a, const Other ...other);

template<typename ...Other>
void print_to_console(long long c, const Other ...other) {
	char buff[20];
	bool is_neg = false;
	if (c < 0) {
		c *= -1;
		is_neg = true;
	}
	for (int w = 0; w < 20; w++) {
		buff[w] = c % 10;
		c /= 10;
	}
	if (is_neg) {
		while(write(1, "-", 1) == 0);
	}
	int w = 19;
	while ((w > 0) && (buff[w] == 0)) {
		w--;
	}
	while (w >= 0) {
		buff[w] += '0';
	 	while (write(1, &buff[w], 1) == 0);
		w--;
	}
	print_to_console(other...);
}

template<typename ...Other>
void print_to_console(const char* a, const Other ...other) {
	int size, pos = 0;
	for (size = 0; a[size]; size++);

	while (pos != size) {
		int res = write(1, &a[pos], size - pos);
		pos += res;
	}

	print_to_console(other...);
}

extern thread_local int no;

template<typename ...Args>
void print(const Args &...args) {
	#ifndef REMOVE_TEXT
		std::lock_guard<std::recursive_mutex> lg(debug_mutex);
		num_alive_writers++;

		#ifndef WRITE_TO_CONSOLE
			std::ofstream out("/home/vlad-kv/My_Creations/Allocator/log.txt", std::ios_base::app);
			lok_print(out, args...);
			out.close();
		#else
			print_to_console(args...);
		#endif
		num_alive_writers--;
	#endif
}

template<typename ...Args>
void my_assert(bool c, Args ...args) {
	if (!c) {
		WRITE("in my_assert\n");
		print("My assertion failed: ", args..., "\n");
		exit(1);
	}
}

template<typename ...Args>
void fatal_error(Args ...args) {
	WRITE("in fatal_error\n");
	print("Fatal error: ", args...);
	exit(1);
}

#endif
