
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

/*
CPU Temp: Read from /sys/class/thermal/thermal_zone0/temp
Memory Usage: Parse /proc/meminfo
Network Stats: Parse /proc/net/dev
*/

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void get_timestamp(char *buffer, size_t size){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

void log_data(float cpu_temp, float mem_usage, float net_usage){
    pthread_mutex_lock(&log_mutex);
    FILE* fptr;

    while(1){
        fptr = fopen("/Users/cameronpinchin/Desktop/projects/systems-logger/log_file.txt", "w");
        
        if(fptr == NULL){
            printf("Error: Cannot open 'log_file.txt' \n");
            pthread_mutex_unlock(&log_mutex);
            exit(1);
        }

        char timestamp[20];
        get_timestamp(timestamp, 20);

        fprintf(fptr, "[%s]\n", timestamp);
        fprintf(fptr, "CPU Temperature: %.1f°C\n", cpu_temp);
        fprintf(fptr, "   Memory Usage: %.1f/GB\n", mem_usage);
        // This needs to be changed, output will not be as simple as a singular float.
        // - depends on how I extract it, can extracted transmitted and received bytes over a given protocol (eth for ex).
        fprintf(fptr, "   Net Actvitiy: %.1fKB/s\n", net_usage);

        fclose(fptr);
        pthread_mutex_unlock(&log_mutex);
    }
}

void* get_temp(void* arg){
    FILE *temp_ptr;
    float sys_temp;

    while(1){
        printf("[TEMP] Reading CPU Temperature... \n");
        temp_ptr = fopen("/sys/class/thermal/thermal_zone0/temp", "r");

        if(temp_ptr == NULL){
            printf("Error! Cannot access: /sys/class/thermal/thermal_zone0/temp \n");
            exit(1);
        }

        fscanf(temp_ptr, '%f', &sys_temp);
        log_data(sys_temp, 0, 0);
    }
    return NULL;
}

void* get_mem_usage(void* arg){
    FILE *mem_ptr;
    float sys_mem;

    while(1){
        printf("[MEMORY] Reading memory usage... \n");
        mem_ptr = fopen("/proc/meminfo", "r");

        if(mem_ptr == NULL){
            printf("Error: Cannot open file: /proc/meminfo \n");
            exit(1);
        }

        fscanf(mem_ptr, '%f', &sys_mem);
        log_data(0, sys_mem, 0);
        sleep(3);
    }
    return NULL;
}

void* get_net_usage(void* arg){
    FILE *net_ptr;
    float sys_net;

    while(1){
        printf("[NET] Reading net usage... \n");
        net_ptr = fopen("/proc/net/dev", "r");

        if(net_ptr == NULL){
            printf("Error: Could not open: /proc/net/dev \n");
            exit(1);
        }

        fscanf(net_ptr, '%f', sys_net);
        log_data(0, 0, sys_net);
        sleep(4);
    }
    return NULL;
}

int main() {
    pthread_t temp_thread, mem_thread, net_thread;

    FILE *fptr;
    fptr = fopen("/log_file.txt", "w");

    if(fptr){
        fprintf(fptr, "SYSTEM LOG STARTED \n");
        fprintf(fptr, "================== \n");
        fclose(fptr);
    } else {
        printf("Error: Could not open 'log_file.txt' \n");
        exit(1);
    }

    pthread_create(&temp_thread, NULL, get_temp, NULL);
    pthread_create(&mem_thread, NULL, get_mem_usage, NULL);
    pthread_create(&net_thread, NULL, get_net_usage, NULL);

    pthread_join(temp_thread, NULL);
    pthread_join(mem_thread, NULL);
    pthread_join(net_thread, NULL);

    return 0;

}