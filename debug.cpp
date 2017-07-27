#include "debug.h"

std::recursive_mutex debug_mutex;

const int BUFFER_FOR_PRINT_SIZE = 10000;
char buffer_for_print[BUFFER_FOR_PRINT_SIZE];
int buffer_pos = 0;
int num_alive_writers = 0;

void lok_print() {
}
