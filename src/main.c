#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#include <unistd.h>

#include "typedefgen.h"
#include "types.h"
#include "base.c"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#define Array(type) type*

f32
get_temperature() {
  FILE *f;
  s32 temp;
  f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");

  fscanf(f, "%d", &temp);
  return temp;
}

u32
count_threads() {
  DIR *dir;
  struct dirent *entry;
  int count = 0;

  if ((dir = opendir("/proc")) != null) {
    while ((entry = readdir(dir)) != null) {
      if (isdigit(entry->d_name[0])) { // PID has to be number
        char task_path[256];
        snprintf(task_path, sizeof(task_path), "/proc/%s/task", entry->d_name);
        DIR *task_dir;

        if ((task_dir = opendir(task_path)) != null) {
          struct dirent *task_ent;
          while ((task_ent = readdir(task_dir)) != null) {
            if (isdigit(task_ent->d_name[0])) {
              count++;
            }
          }
        	closedir(task_dir);
        }

      }
    }
    closedir(dir);
  }
  else {
    perror("Cannot open /proc");
  }

  return count;
}

u64 prev_cpu_idle = 0;
u64 prev_cpu_total = 0;

f32
get_cpu_use() {
  FILE *f;
  u64 user = 0, nice = 0, system = 0, idle, iowait = 0, irq = 0, softirq = 0, steal = 0;

  f = fopen("/proc/stat", "r");
  if (f == null) {
    perror("Error al abrir /proc/stat");
    return -1.0;
  }

	if (fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
			&user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) != 8) {
	  fprintf(stderr, "Error reading CPU stats\n");
	  fclose(f);
	  return -1.0;
	}

	u64 idle_time  = idle + iowait;
	u64 total_time = user + nice + system + idle_time + irq + softirq + steal;

	if (prev_cpu_total == 0) {
	  prev_cpu_total = total_time;
	  prev_cpu_idle  = idle_time;
	  return 0.0f;
	}

	u64 total_diff = total_time - prev_cpu_total;
	u64 idle_diff  = idle_time  - prev_cpu_idle;

	prev_cpu_total = total_time;
	prev_cpu_idle  = idle_time;

	return (f32)(total_diff - idle_diff) * 100.0f / total_diff;
}

global struct sysinfo mem;

// sysinfo doesn't have page all the information linux use for calculating available ram like page cache
u64
get_available_ram() {
	FILE* f = fopen("/proc/meminfo", "r");
	if (!f) return 0;

	char line[256];
	u64 available = 0;

	while (fgets(line, sizeof(line), f)) {
		if (sscanf(line, "MemAvailable: %lu kB", &available) == 1) {
			break;
		}
	}

	fclose(f);
	return available * 1024;
}

u64
get_virtual_ram_used() {
	FILE* f = fopen("/proc/meminfo", "r");
	char line[256];

	while(fgets(line, sizeof(line), f)) {
		if (strncmp(line, "Committed_AS:", 13) == 0) {
			char* c;
			c = line+13;
			for(;;) {
				if (isspace(c[0])) c++;
				else break;
			}
			int len = 0;
			for (;;) {
				if(isspace(c[len])) break;
				len++;
			}
			c[len] = '\0';
			u64 value = 0;
			sscanf(c, "%lu", &value);
			return value * 1024;
		}
	}
	return 0;
}

u64
get_cpu_frequency(int cpu_id) {
	char path[256];
	snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu_id);
	FILE *f = fopen(path, "r");
	if(!f) return 0.;
	u64 freq;
	fscanf(f, "%lu", &freq);
	fclose(f);
	return freq;
}

u64
get_cpu_count() {
	return sysconf(_SC_NPROCESSORS_ONLN); // POSIX specific
}

s64
get_gpu_info() {
	char dir_path[256];
	char file_path[256];
	String_Builder file_content = sb_init(1024);
	bool is_amd_gpu = false;
	// TODO: Nvidia and Intel
	s64 power       = -1;
	s64 temperature = -1;
	int index_path = 0;

	for(;;) {
		snprintf(dir_path, sizeof(dir_path), "/sys/class/hwmon/hwmon%d", index_path);
		DIR* dir = opendir(dir_path);
		if(!dir) break;

		struct dirent* entry;
		while((entry = readdir(dir)) != null) {
			if (strcmp(entry->d_name, "name") == 0) {
				snprintf(file_path, sizeof(file_path), "%s/name", dir_path);
				FILE* f = fopen(file_path, "r");
				fscanf(f, "%s", file_content.data);
				file_content.len = strlen(file_content.data);
				trimp(&file_content);
				if (cstring_cmp(sb_to_string(file_content), "amdgpu")) {
					printf("device name: %s \n", file_content.data);
					is_amd_gpu = true;
					break;
				}
				fclose(f);
			}
		}
		if (!is_amd_gpu) {
			index_path++;
			closedir(dir);
			continue;
		}

		rewinddir(dir); // readdir needs acts like a cursor
		entry = null; // maybe this is not needed?

		while ((entry = readdir(dir)) != null) {
			if (strcmp(entry->d_name, "power1_average") == 0) {
				snprintf(file_path, sizeof(file_path), "%s/power1_average", dir_path);
				FILE* f = fopen(file_path, "r");
				fscanf(f, "%lu", &power);

				printf("power: %lu microwatts, %lu watts \n", power, power / 1000000);
				fclose(f);
			}
			if (strcmp(entry->d_name, "temp1_input") == 0) {
				snprintf(file_path, sizeof(file_path), "%s/temp1_input", dir_path);
				FILE* f = fopen(file_path, "r");
				fscanf(f, "%lu", &temperature);

				printf("temperature of gpu: %lu \n", temperature);
				fclose(f);

			}
		}



		index_path++;
		closedir(dir);
	}

	free(file_content.data);
	// snprintf()
}



const u64 KB = 1024;
const u64 MB = 1024 * KB;
const u64 GB = 1024 * MB;
const u64 TB = 1024 * GB;

int main(void) {

	f64 loads[2048];
	u64 n_loads;
	f32 cpu_use = get_cpu_use();
	if (cpu_use < 0) return 0;

	sysinfo(&mem);
  u64 available_ram = get_available_ram();
  u64 used_ram = mem.totalram - mem.freeram - available_ram;
	u64 virtual_ram_used = get_virtual_ram_used();
	u64 cpu_count = get_cpu_count();
	u64 gpu = get_gpu_info();


  printf("cpu use %f \n", cpu_use);
	printf("\ntemperature %f \n", get_temperature());
  printf("threads active %d \n", count_threads());
	printf("CPUs             : %lu\n", cpu_count);
	for (int i = 0; i < cpu_count; i++) {
		u64 cpu_frequency = get_cpu_frequency(i);
		printf("CPU frequency    : CPU %d %luKhz\n", i, cpu_frequency);
	}
  printf("virtual used ram : %luB, %luKB, %luMB, %luGB, %luTB\n", virtual_ram_used, virtual_ram_used/KB, virtual_ram_used/MB, virtual_ram_used/GB, virtual_ram_used/TB);
  printf("total ram        : %luB, %luKB, %luMB, %luGB\n", mem.totalram, mem.totalram/KB, mem.totalram/MB, mem.totalram/GB);
  printf("free ram         : %luB, %luKB, %luMB, %luGB\n", mem.freeram, mem.freeram/KB, mem.freeram/MB, mem.freeram/GB);
  printf("available ram    : %luB, %luKB, %luMB, %luGB\n", available_ram, available_ram/KB, available_ram/MB, available_ram/GB);
  printf("used ram         : %luB, %luKB, %luMB, %luGB\n", used_ram, used_ram/KB, used_ram/MB, used_ram/GB);
	printf("total swap       : %luB, %luKB, %luMB, %luGB\n", mem.totalswap, mem.totalswap/KB, mem.totalswap/MB, mem.totalswap/GB);
	printf("free swap        : %luB, %luKB, %luMB, %luGB\n", mem.freeswap, mem.freeswap/KB, mem.freeswap/MB, mem.freeswap/GB);
	// for (;;) {
	// 	f32 cpu_use = get_cpu_use();
	// 	if (cpu_use < 0) return 0;
	//   printf("cpu use %f \n", cpu_use);
	//   sleep(1);
	// }


  return 0;
}