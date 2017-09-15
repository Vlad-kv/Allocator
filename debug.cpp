#include "debug.h"

std::recursive_mutex debug_mutex;
thread_local int num_alive_writers = 0;

void print_to_console() {
}
