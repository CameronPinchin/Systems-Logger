
/*
Author: Cameron Pinchin
Date: March 26th, 2025
Description: System logger for linux, meant to be run on a Raspberry Pi.
 - Will be deployable on other systems soon.
 

Notes: Allow for dynamic paths for system information, as opposed
        to statically assigning paths. Makes the system more deployable
        and resistant.

*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

#define CPUIINFO_FILE "/sys/class/thermal/thermal_zone0/temp"
#define MEMINFO_FILE "/proc/meminfo"
#define NETINFO_FILE "/proc/net/dev"

#define CONVERSION_CONST 1024
#define MILIDEGREE_TO_C 1000

/*
CPU Temp: Read from /sys/class/thermal/thermal_zone0/temp
Memory Usage: Parse /proc/meminfo
Network Stats: Parse /proc/net/dev
*/

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
int should_exit = 0;

void get_timestamp(char *buffer, size_t size){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

void log_data(int cpu_temp, int mem_usage, int transmit_rate, int received_rate){
    pthread_mutex_lock(&log_mutex);
    FILE* fptr;

    fptr = fopen("log_file.txt", "w");
    if(fptr == NULL){
        printf("Error: Cannot open 'log_file.txt' \n");
        pthread_mutex_unlock(&log_mutex);
        exit(1);
    }

    char timestamp[20];
    get_timestamp(timestamp, 20);

    fprintf(fptr, "                           Timestamp: [%s]\n", timestamp);
    fprintf(fptr, "                     CPU Temperature: %d°C\n", cpu_temp);
    fprintf(fptr, "                        Memory Usage: %d GB\n", mem_usage);
    // This needs to be changed, output will not be as simple as a singular float.
    // - depends on how I extract it, can extracted transmitted and received bytes over a given protocol (eth for ex).
    fprintf(fptr, "[Interface: lo]    Transmission Rate: %d KB/s\n", transmit_rate);
    fprintf(fptr, "[Interface: lo]        Received Rate: %d KB/s\n", received_rate);

    fclose(fptr);
    pthread_mutex_unlock(&log_mutex);
    
}

void* get_temp(void* arg){
    FILE *temp_ptr;
    int sys_temp;

    while(!should_exit){
        printf("[TEMP] Reading CPU Temperature... \n");
        temp_ptr = fopen(CPUIINFO_FILE, "r");

        if(temp_ptr == NULL){
            printf("Error! Cannot access: %s \n", CPUIINFO_FILE);
            exit(1);
        }

        fscanf(temp_ptr, "%d", &sys_temp);
        fclose(temp_ptr);
        sys_temp = sys_temp / MILIDEGREE_TO_C;
        log_data(sys_temp, 0, 0, 0);
        sleep(2);
        if(should_exit){
            break;
        }
    }
    return NULL;
}

void* get_mem_usage(void* arg){
    FILE *mem_ptr;
    int sys_mem_total, sys_mem_available;

    while(!should_exit){
        printf("[MEMORY] Reading memory usage... \n");
        mem_ptr = fopen(MEMINFO_FILE, "r");

        if(mem_ptr == NULL){
            printf("Error: Cannot open file: /proc/meminfo \n");
            exit(1);
        }

        char line[256];
        while(fgets(line, sizeof(line), mem_ptr)){
            if(sscanf(line, "MemTotal: %d kB", &sys_mem_total) == 1) continue;
            if(sscanf(line, "MemAvailable: %d kB", &sys_mem_available) == 1) break;
        }
        fclose(mem_ptr);

        int used_mem = (sys_mem_total - sys_mem_available) / CONVERSION_CONST;
        
        log_data(0, used_mem, 0, 0);
        sleep(3);
        if(should_exit){
            break;
        }
    }
    return NULL;
}

void* get_net_usage(void* arg){
    FILE *net_ptr;
    int sys_net;
    int sys_received, sys_transmitted;
    int last_received = 0, last_transmitted = 0;
    while(!should_exit){
        printf("[NET] Reading net usage... \n");
        net_ptr = fopen(NETINFO_FILE, "r");

        if(net_ptr == NULL){
            printf("Error: Could not open: %s \n", NETINFO_FILE);
            exit(1);
        }

        char lines[256];

        while(fgets(lines, sizeof(lines), net_ptr)){
            if(strstr(lines, "lo")){
                sscanf(lines, "%*s %d %*d %*d %*d %*d %*d %*d %*d %d", &sys_received, &sys_transmitted);
                break;
            }
        }

        int received_rate = (sys_received - last_received) / 1024;
        int transmitted_rate = (sys_transmitted - last_transmitted) / CONVERSION_CONST;

        last_received = sys_received;
        last_transmitted = sys_transmitted;

        log_data(0, 0, transmitted_rate, received_rate);
        sleep(4);
        if(should_exit){
            break;
        }

    }
    return NULL;
}

int main() {
    pthread_t temp_thread, mem_thread, net_thread;

    FILE *fptr;
    fptr = fopen("log_file.txt", "w");

    if(fptr){
        fprintf(fptr, "SYSTEM LOG STARTED \n");
        fprintf(fptr, "================== \n");
        fclose(fptr);
    }

    pthread_create(&temp_thread, NULL, get_temp, NULL);
    pthread_create(&mem_thread, NULL, get_mem_usage, NULL);
    pthread_create(&net_thread, NULL, get_net_usage, NULL);

    sleep(20);
    should_exit = 1;

    pthread_join(temp_thread, NULL);
    pthread_join(mem_thread, NULL);
    pthread_join(net_thread, NULL);

    return 0;

}