
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
#include <math.h>

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

pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t data_ready = PTHREAD_COND_INITIALIZER;

int cpu_temp = -1, mem_usage = -1, transmit_rate = -1, received_rate = -1;
int ready_count = 0;
int should_exit = 0;

void get_timestamp(char *buffer, size_t size){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

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

    fprintf(fptr, "                           Timestamp: [%s]\n", timestamp);
    fprintf(fptr, "                     CPU Temperature: %d°C\n", cpu_temp);
    fprintf(fptr, "                        Memory Usage: %d mB\n", mem_usage);
    fprintf(fptr, "[Interface: lo]    Transmission Rate: %d KB/s\n", transmit_rate);
    fprintf(fptr, "[Interface: lo]        Received Rate: %d KB/s\n", received_rate);
    fclose(fptr);

    pthread_mutex_unlock(&data_mutex);
    
}

void signal_data_ready(){
    pthread_mutex_lock(&data_mutex);
    ready_count++;

    if(ready_count == 3){
        log_data();
        ready_count = 0;
        printf("[DEBUG] Resetting ready_count \n");
        pthread_cond_broadcast(&data_ready);
    }
    printf("[DEBUG] After signal: ready_count = %d \n", ready_count);
    pthread_mutex_unlock(&data_mutex);
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


        if(fscanf(temp_ptr, "%d", &sys_temp) != 1){
            printf("Error: Could not read temperature. \n");
            fclose(temp_ptr);
            exit(1);
        }

        fclose(temp_ptr);

        pthread_mutex_lock(&data_mutex);
        float temp_in_c = sys_temp / 1000.0f;
        cpu_temp = (int)round(temp_in_c);
        pthread_mutex_unlock(&data_mutex);

        printf("[DEBUG] Temp calling signal_data_ready()\n");

        signal_data_ready();
        sleep(5);
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
        
        pthread_mutex_lock(&data_mutex);
        mem_usage = (sys_mem_total - sys_mem_available) / CONVERSION_CONST;
        pthread_mutex_unlock(&data_mutex);

        printf("[DEBUG] Memory calling signal_data_ready()\n");
        signal_data_ready();
        sleep(5);
    
    }
    return NULL;
}

void* get_net_usage(void* arg){
    FILE *net_ptr;
    int sys_net;
    unsigned long long sys_received, sys_transmitted;
    unsigned long long last_received = 0, last_transmitted = 0;
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


        unsigned long long received_diff = sys_received - last_received;
        unsigned long long transmitted_diff = sys_transmitted - last_transmitted;

        pthread_mutex_lock(&data_mutex);
        received_rate = (int)round((float)received_diff/ 1024.0);  
        transmit_rate = (int)round((float)transmitted_diff / 1024.0);  
        pthread_mutex_unlock(&data_mutex);



        last_received = sys_received;
        last_transmitted = sys_transmitted;

        printf("[DEBUG] Network calling signal_data_ready()\n");

        signal_data_ready();
        sleep(5);

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