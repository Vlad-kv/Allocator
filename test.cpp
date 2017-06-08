#include <iostream>
#include <cstdlib>
#include <cstdio>
using namespace std;
const int N = 10;;
char *a[N];

int main() {
	for (int w = 1; w < N; w++) {
		a[w] = new char[w * 10];
	}
	for (int w = 1; w < N; w++) {
		delete []a[w];
	}
}
