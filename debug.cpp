#include "debug.h"

std::recursive_mutex debug_mutex;

const int BUFFER_FOR_PRINT_SIZE = 10000;
char buffer_for_print[BUFFER_FOR_PRINT_SIZE];
int buffer_pos = 0;
std::atomic_int num_alive_writers;

void lok_print() {
}
