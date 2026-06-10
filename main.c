#include "./include/sigma_malloc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
  return 0;
}
