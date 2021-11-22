#include <sys/types.h>
#include <stdio.h>

#define ll long int
#define int ll

int main() {
  int n = 1000;
  int a[n];
  for (int i = 0; i < n; i++) {
    a[i] = (1<<28) + i;
  }
  int b;
  for (int i = 0; i < n; i++) {
    b = a[i]+1;
    // printf("%d\n", i);
    getpid();
  }
};
