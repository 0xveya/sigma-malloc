#include "./include/sigma_malloc.h"
#include "./include/utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define var auto

int main(void) {
  var lol = (int *)balls(sizeof(int) * 10);
  if (!lol) {
    exit(67);
  }
  // defer { cock(lol); };
  for (int i = 0; i < 10; i++) {
    lol[i] = i * 67;
    printf("%d\n", lol[i]);
  }
  var test = (char *)balls(1024 * 1024);
  memset(test, 'f', 1024 * 1024);
  printf("test: %.10s\n", test);
  defer { cock(test); };
  // cock(lol);
  return 0;
}
