#ifndef DEBUG_H
#define DEBUG_H

// #define REMOVE_TEXT
// #define WRITE_TO_CONSOLE

#include <mutex>
#include <cstring>
#include <fstream>
#include <unistd.h>

extern std::recursive_mutex debug_mutex;

extern const int BUFFER_FOR_PRINT_SIZE;
extern char buffer_for_print[];
extern int buffer_pos;
extern int num_alive_writers;


#ifndef WRITE_TO_CONSOLE
	template<typename ...Args>
	void lok_print(std::ofstream &out, const Args &...args) {
	}

	template<typename First, typename ...Other>
	void lok_print(std::ofstream &out, const First &first, const Other &...other) {
		out << first;
		lok_print(out, other...);
	}
#else
	void lok_print();

	template<typename ...Other>
	void lok_print(const char* a, const Other ...other);

	template<typename ...Other>
	void lok_print(long long c, const Other ...other) {
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
		lok_print(other...);
	}

	template<typename ...Other>
	void lok_print(const char* a, const Other ...other) {
		int size, pos = 0;
		for (size = 0; a[size]; size++);

		while (pos != size) {
			int res = write(1, &a[pos], size - pos);
			pos += res;
		}

		lok_print(other...);
	}
#endif

template<typename ...Args>
void print(const Args &...args) {
	#ifndef REMOVE_TEXT
		std::lock_guard<std::recursive_mutex> lg(debug_mutex);

		num_alive_writers++;
		#ifndef WRITE_TO_CONSOLE
			std::ofstream out("log.txt", std::ios_base::app);
			lok_print(out, args...);
			out.close();

			buffer_pos = 0;
		#else
			lok_print(args...);
		#endif

		num_alive_writers--;
	#endif
}

template<typename ...Args>
void my_assert(bool c, Args ...args) {
	if (!c) {
		print("My assertion failed: ", args..., "\n");
		exit(1);
	}
}

template<typename ...Args>
void fatal_error(Args ...args) {
	print("Fatal error: ", args...);
	exit(1);
}

#endif
