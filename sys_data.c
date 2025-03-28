
/*
Author: Cameron Pinchin
Date: March 26th, 2025
Description: System logger for linux, meant to be run on a Raspberry Pi.
 - Will be deployable on other systems soon.
 - Mainly wanted to get more experience with multi-threadding. 

*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <math.h>

#define CPUIINFO_FILE "/sys/class/thermal/thermal_zone0/temp"
#define MEMINFO_FILE "/proc/meminfo"
#define NETINFO_FILE "/proc/net/dev"

#define CONVERSION_CONST 1024
#define MILLIDEGREE_TO_C 1000.0f

pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t data_ready = PTHREAD_COND_INITIALIZER;

int cpu_temp = -1, mem_usage = -1, transmit_rate = -1, received_rate = -1;
int ready_count = 0;
int should_exit = 0;

// Function for gathering the timestamp of a given log.
void get_timestamp(char *buffer, size_t size){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

// Function dedicated to logging data. 
void log_data(){
    pthread_mutex_lock(&data_mutex);
    FILE* fptr;

    fptr = fopen("log_file.txt", "a");
    if(fptr == NULL){
        printf("Error: Cannot open 'log_file.txt' \n");
        pthread_mutex_unlock(&data_mutex);
        exit(1);
    }

    char timestamp[20];
    get_timestamp(timestamp, 20);
    printf("Printing: %d, %d, %d, %d \n", cpu_temp, mem_usage, transmit_rate, received_rate);
    fprintf(fptr, "                           Timestamp: [%s]\n", timestamp);
    fprintf(fptr, "                     CPU Temperature: %d°C\n", cpu_temp);
    fprintf(fptr, "                        Memory Usage: %d mB\n", mem_usage);
    fprintf(fptr, "[Interface: lo]    Transmission Rate: %d KB/s\n", transmit_rate);
    fprintf(fptr, "[Interface: lo]        Received Rate: %d KB/s\n", received_rate);
    fclose(fptr);

    pthread_mutex_unlock(&data_mutex);
}

void signal_data_ready(){
    ready_count++;
    if(ready_count == 3){
        pthread_mutex_unlock(&data_mutex);
        log_data();
        ready_count = 0;
        
        pthread_cond_broadcast(&data_ready);
    } 
}

// Function to gather the temperature of the CPU.
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

        if(fscanf(temp_ptr, "%d", &sys_temp) != 1){
            printf("Error: Could not read temperature. \n");
            fclose(temp_ptr);
            exit(1);
        }

        fclose(temp_ptr);

        float temp_in_c = sys_temp / MILLIDEGREE_TO_C;
        cpu_temp = (int)round(temp_in_c);

        pthread_mutex_lock(&data_mutex);
        signal_data_ready();
        pthread_mutex_unlock(&data_mutex);

        sleep(2);
    }

    return NULL;
}

// Function for gathering memory usage, scaled to mB for Pi memory usage.
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
        
        mem_usage = (sys_mem_total - sys_mem_available) / CONVERSION_CONST;
        pthread_mutex_lock(&data_mutex);
        signal_data_ready();
        pthread_mutex_unlock(&data_mutex);
        
        sleep(2);
    }

    return NULL;
}

//  Function that gathers net usage (bytes transmitted, bytes received).
void* get_net_usage(void* arg){
    FILE *net_ptr;
    unsigned long long sys_received, sys_transmitted;
    
    while(!should_exit){
        printf("[NET] Reading net usage... \n");
        net_ptr = fopen(NETINFO_FILE, "r");

        if(net_ptr == NULL){
            printf("Error: Could not open: %s \n", NETINFO_FILE);
            exit(1);
        }

        char lines[256];

        while(fgets(lines, sizeof(lines), net_ptr)){
            unsigned long long tmp_received, tmp_transmitted;
            if(strstr(lines, "lo")){
                sscanf(lines, "%*s %lld %*d %*d %*d %*d %*d %*d %*d %lld", &tmp_received, &tmp_transmitted);
                sys_received = tmp_received;
                sys_transmitted = tmp_transmitted;
                printf("Received: %llu, Transmitted: %llu\n", sys_received, sys_transmitted);
                break;
            }
        }

        received_rate = (int)round((float)sys_received/ CONVERSION_CONST);  
        transmit_rate = (int)round((float)sys_transmitted/ CONVERSION_CONST); 

        pthread_mutex_lock(&data_mutex); 
        signal_data_ready();
        pthread_mutex_unlock(&data_mutex);

       sleep(2);
    }
    
    return NULL;
}

int main() {
    // Thread declaration, creation, then calling pthread_join().
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

    sleep(10);
    should_exit = 1; 

    // allow threads to finish execution then terminate.
    pthread_join(temp_thread, NULL);
    pthread_join(mem_thread, NULL);
    pthread_join(net_thread, NULL);

    return 0;
}