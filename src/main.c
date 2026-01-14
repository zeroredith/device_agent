#include <stdio.h>
#include "types.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#define Array(type) type*

f32 get_temperature() {
  FILE *fp;
  int temp;
  fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
}

int main(void) {

  return 0;
}