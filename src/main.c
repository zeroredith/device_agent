#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#include <unistd.h>
#include <curl/curl.h>

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

enum Gpu_Type {
	AMD,
	NVIDIA,
	INTEL,
	UKNOWN_OR_NOT_EXISTS,
};

struct Gpu_Info {
	bool exists;
	Gpu_Type gpu_type;
	s64 power;
	s64 temperature;
};

// TODO: Nvidia and Intel
Gpu_Info
get_gpu_info() {
	char dir_path[256];
	char file_path[256];
	String_Builder file_content = sb_init(1024);
	Gpu_Info info = {
		.exists = false,
		.gpu_type = UKNOWN_OR_NOT_EXISTS,
		.power       = -1,
		.temperature = -1,
	};
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
					info.gpu_type = AMD;
					break;
				}
				fclose(f);
			}
		}
		if (info.gpu_type == UKNOWN_OR_NOT_EXISTS) {
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
				fscanf(f, "%lu", &info.power);

				printf("info.power: %lu microwatts, %lu watts \n", info.power, info.power / 1000000);
				fclose(f);
			}
			if (strcmp(entry->d_name, "temp1_input") == 0) {
				snprintf(file_path, sizeof(file_path), "%s/temp1_input", dir_path);
				FILE* f = fopen(file_path, "r");
				fscanf(f, "%lu", &info.temperature);

				printf("info.temperature of gpu: %lu \n", info.temperature);
				fclose(f);

			}
		}



		index_path++;
		closedir(dir);
	}

	free(file_content.data);
	return info;
	// snprintf()
}



const u64 KB = 1024;
const u64 MB = 1024 * KB;
const u64 GB = 1024 * MB;
const u64 TB = 1024 * GB;

struct Device_Info {
	f32 cpu_use;
	u64 available_ram;
	u64 used_ram;
	u64 virtual_ram_used;
	Array(u64) cpu_frequencies;
	Gpu_Info gpu_info;
};

int main(void) {

	f64 loads[2048];
	u64 n_loads;
	f32 cpu_use = get_cpu_use();
	if (cpu_use < 0) return 0;

	sysinfo(&mem);
	Device_Info info = {0};
  info.available_ram = get_available_ram();
  info.used_ram = mem.totalram - mem.freeram - info.available_ram;
	info.virtual_ram_used = get_virtual_ram_used();
	u64 cpu_count = get_cpu_count();
	info.gpu_info = get_gpu_info();

	printf("gpu info: type = %d\n exists = %d \n power = %ld \n temperature = %ld\n", info.gpu_info.gpu_type, info.gpu_info.exists, info.gpu_info.power, info.gpu_info.temperature);


  printf("cpu use %f \n", cpu_use);
	printf("\ntemperature %f \n", get_temperature());
  printf("threads active %d \n", count_threads());
	printf("CPUs             : %lu\n", cpu_count);

	for (int i = 0; i < cpu_count; i++) {
		u64 cpu_frequency = get_cpu_frequency(i);
		arrpush(info.cpu_frequencies, cpu_frequency);
		printf("CPU frequency    : CPU %d %luKhz\n", i, cpu_frequency);
	}

	for (int i = 0; i < arrlen(info.cpu_frequencies); i++) printf("frequency: %lu\n", info.cpu_frequencies[i]);
  printf("virtual used ram : %luB, %luKB, %luMB, %luGB, %luTB\n", info.virtual_ram_used, info.virtual_ram_used/KB, info.virtual_ram_used/MB, info.virtual_ram_used/GB, info.virtual_ram_used/TB);
  printf("total ram        : %luB, %luKB, %luMB, %luGB\n", mem.totalram, mem.totalram/KB, mem.totalram/MB, mem.totalram/GB);
  printf("free ram         : %luB, %luKB, %luMB, %luGB\n", mem.freeram, mem.freeram/KB, mem.freeram/MB, mem.freeram/GB);
  printf("available ram    : %luB, %luKB, %luMB, %luGB\n", info.available_ram, info.available_ram/KB, info.available_ram/MB, info.available_ram/GB);
  printf("used ram         : %luB, %luKB, %luMB, %luGB\n", info.used_ram, info.used_ram/KB, info.used_ram/MB, info.used_ram/GB);
	printf("total swap       : %luB, %luKB, %luMB, %luGB\n", mem.totalswap, mem.totalswap/KB, mem.totalswap/MB, mem.totalswap/GB);
	printf("free swap        : %luB, %luKB, %luMB, %luGB\n", mem.freeswap, mem.freeswap/KB, mem.freeswap/MB, mem.freeswap/GB);
	// for (;;) {
	// 	f32 cpu_use = get_cpu_use();
	// 	if (cpu_use < 0) return 0;
	//   printf("cpu use %f \n", cpu_use);
	//   sleep(1);
	// }

	CURL *curl;
	CURLcode result;
	result = curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, "http://postit.example.com/moo.cgi");
  result = curl_easy_perform(curl);
  curl_easy_strerror(result);


	arrfree(info.cpu_frequencies);


  return 0;
}