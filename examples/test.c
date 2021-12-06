#include <sys/types.h>
#include <stdio.h>

#define ll long
#define int ll

int global_printf = 0;

int main() {
  setvbuf(stdout, (char *)NULL, _IONBF, 0);
  printf("P %lx o", &global_printf);

  int n = 10 * 3;
  int a[n];


  for (int i = 0; i < n; i++) {
    a[i] = (1ll<<28) + i;

    global_printf = 1;
    if (i % 3 == 0) {
      printf("GC o");
    } else if (i % 3 == 1) {
      printf("%lx t", (unsigned long)(&a[i]));
      printf("%lx t", a[i]);
      printf("%ld t", a[i]);
      printf("o");
    } else {
      printf("GCEND o");
      printf("\n");
    }
    global_printf = 0;
  }
};
