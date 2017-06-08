#ifndef DEBUG_H
#define DEBUG_H

template<typename ...Other>
void print(const int& first, Other ...other);

template<typename ...Other>
void print(const char* first, Other ...other);

void print() {
}

template<typename ...Other>
void print(const char* first, Other ...other) {
	write(1, first, strlen(first));
	print(other...);
}

template<typename ...Other>
void print(const int& first, Other ...other) {
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
	print(other...);
}

#endif
