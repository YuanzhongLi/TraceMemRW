#include <stdio.h>

int main() {
  setvbuf(stdout, (char *)NULL, _IONBF, 0);
  int n = 100;
  int a[n];
  for (int i = 0; i < n; i++) {
    a[i] = i;
    // printf("i: %d", i);
  }
};
