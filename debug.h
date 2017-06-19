#ifndef DEBUG_H
#define DEBUG_H

// #define REMOVE_TEXT

#include <cstring>

namespace {
	void write_ll(long long first) {
		char temp[21];
		int c = first, sign = 0;
		if (c < 0) {
			c *= -1;
			sign = 1;
		}
		for (int w = 0; w < 21; w++) {
			temp[w] = c % 10;
			c /= 10;
		}
		if (sign) {
			write(1, "-", 1);
		}
		int w = 20;
		while ((w > 0) && (temp[w] == 0)) {
			w--;
		}
		while (w >= 0) {
			temp[w] += '0';
			write(1, &temp[w], 1);
			w--;
		}
	}
}

template<typename ...Other>
void print(const long long& first, Other ...other);

void print() {
}

template<typename ...Other>
void print(const char* first, Other ...other) {
	#ifndef REMOVE_TEXT
		write(1, first, strlen(first));
	#endif
	print(other...);
}

template<typename ...Other>
void print(const long long& first, Other ...other) {
	#ifndef REMOVE_TEXT
		write_ll(first);
	#endif
	print(other...);
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
