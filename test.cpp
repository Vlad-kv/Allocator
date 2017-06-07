#include <iostream>
#include <cstdlib>
#include <cstdio>
using namespace std;
int main() {
	for (int w = 1; w <= 10; w++) {
		int *a = new int[w];
		delete []a;
	}
}
