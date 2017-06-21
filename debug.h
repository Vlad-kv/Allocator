#ifndef DEBUG_H
#define DEBUG_H

// #define REMOVE_TEXT


#include <mutex>
#include <cstring>
#include <fstream>
#include <unistd.h>

extern std::recursive_mutex debug_mutex;

extern const int BUFFER_FOR_PRINT_SIZE;
extern char buffer_for_print[];
extern int buffer_pos;
extern int num_alive_writers;

template<typename ...Args>
void lok_print(std::ofstream &out, const Args &...args) {
}

template<typename First, typename ...Other>
void lok_print(std::ofstream &out, const First &first, const Other &...other) {
	#ifndef REMOVE_TEXT
		out << first;
	#endif
	lok_print(out, other...);
}


template<typename ...Args>
void print(const Args &...args) {
	std::lock_guard<std::recursive_mutex> lg(debug_mutex);

	num_alive_writers++;
	std::ofstream out("log.txt", std::ios_base::app);

	lok_print(out, args...);

	out.close();

	buffer_pos = 0;
	num_alive_writers--;
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
