#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#include<unistd.h>



#include "types.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#define Array(type) type*

f32
get_temperature() {
  FILE *fp;
  s32 temp;
  fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");

  fscanf(fp, "%d", &temp);
  return temp;
}

u32 count_threads() {
  DIR *dir;
  struct dirent *ent;
  int count = 0;

  if ((dir = opendir("/proc")) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      // Verifica si el nombre de la entrada es un número (indica un PID)
      if (isdigit(ent->d_name[0])) {
        char task_path[256];
        snprintf(task_path, sizeof(task_path), "/proc/%s/task", ent->d_name);
        DIR *task_dir;

        // Abre el directorio de tareas para cada PID
        if ((task_dir = opendir(task_path)) != NULL) {
          struct dirent *task_ent;
          // Cuenta cada hilo en el directorio de tareas
          while ((task_ent = readdir(task_dir)) != NULL) {
            if (isdigit(task_ent->d_name[0])) {
              count++; // Cada entrada numérica es un hilo
            }
          }
        	closedir(task_dir);
        }
      }
    }
    closedir(dir);
  }
  else {
    perror("No se pudo abrir el directorio /proc");
  }

  return count;
}

u64 prev_cpu_idle = 0;
u64 prev_cpu_total = 0;

f32
get_cpu_use() {
  FILE *fp;
  u64 user = 0, nice = 0, system = 0, idle, iowait = 0, irq = 0, softirq = 0, steal = 0;

  fp = fopen("/proc/stat", "r");
  if (fp == NULL) {
    perror("Error al abrir /proc/stat");
    return -1.0;
  }

	if (fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
			&user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) != 8) {
	  fprintf(stderr, "Error reading CPU stats\n");
	  fclose(fp);
	  return -1.0;
	}

	u64 idle_time  = idle + iowait;
	u64 total_time = user + nice + system + idle_time +
	               irq + softirq + steal;

	if (prev_cpu_total == 0) {
	  prev_cpu_total = total_time;
	  prev_cpu_idle  = idle_time;
	  return 0.0f;
	}

	u64 total_diff = total_time - prev_cpu_total;
	u64 idle_diff  = idle_time - prev_cpu_idle;

	prev_cpu_total = total_time;
	prev_cpu_idle  = idle_time;

	return (f32)(total_diff - idle_diff) * 100.0f / total_diff;
}

int main(void) {
	f64 loads[2048];
	u64 n_loads;
	printf("\ntemperature %f \n", get_temperature());
  printf("threads active %d \n", count_threads());
  for (;;) {
		f32 cpu_use = get_cpu_use();
		if (cpu_use < 0) return 0;
	  printf("cpu use %f \n", cpu_use);
	  sleep(1);
  }

  return 0;
}