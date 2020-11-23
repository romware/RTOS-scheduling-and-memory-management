/*******************************************************************************
 * 
 * Project: SRTF CPU Scheduling
 * 
 * Authors:
 * Jack Romanous
 * 
 * Compile instructions:
 * Ensure that gcc is installed and run the following command:
 * gcc -Wall -O2 SRTF-CPU-scheduling.c -o SRTF-CPU-scheduling -lpthread -lrt
 * 
 * Notes:
 * The input data of the cpu scheduling algorithm is:
 * --------------------------------------------------------
 *     Process ID           Arrive time          Burst time
 *              1                     8                  10
 *              2                    10                   3
 *              3                    14                   7
 *              4                     9                   5
 *              5                    16                   4
 *              6                    21                   6
 *              7                    26                   2
 * --------------------------------------------------------
 * 
*******************************************************************************/

#include <pthread.h>    /* pthread functions and data structures for pipe */
#include <unistd.h>     /* for POSIX API */
#include <stdlib.h>     /* for exit() function */
#include <stdio.h>      /* standard I/O routines */
#include <stdbool.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>

/*---------------------------------- Constants -------------------------------*/

#define NUM_OF_PROCESSES 7
#define NAME_OF_FIFO "fifo"
#define WRITE_INTERVAL 500*1000 // 500 milliseconds


/*----------------------------------- Structs --------------------------------*/

typedef struct SRTF_Params 
{
    int pid, arrive_t, wait_t, burst_t, turnaround_t, remain_t;
} Process_Params;

typedef struct Thread_Params
{
    sem_t readSem;
    sem_t writeSem;
    int readFIFO;
    int writeFIFO;
    char filename[64];
    Process_Params processes[NUM_OF_PROCESSES+1];
} Thread_Params;

/*---------------------------------- Prototypes ------------------------------*/

/* this function calculates CPU SRTF scheduling, writes waiting time and turn-around time to the FIFO */
void *worker1(void *params);

/* reads the waiting time and turn-around time through the FIFO and writes to text file */
void *worker2(void *params);

/* Outputs the welcome banner to the console */
void outputWelcome();

/* Outputs the header line to the console */
void outputHeader();

/* Outputs the dividing line to the console */
void outputDivider();

/* Outputs the footer line to the console */
void outputFooter();

/* Outputs a message line to the console */
void outputLine(char *message, const char color);

/* Outputs a message with variable line to the console */
void outputVar(char *message, const char color, char *value);

/* Changes the console color */
void changeColor(const char color);

/*---------------------------------- Functions -------------------------------*/

/* this main function creates named pipe and threads */
int main(int argc, char* argv[])
{
    /* creating a named pipe(FIFO) with read/write permission */
    int makefifo = mkfifo(NAME_OF_FIFO, 0666);
    if((makefifo == -1) && (errno != EEXIST))
    {
        perror("Error creating named pipe");
        return(-11);
    }

    /* initialize the parameters */
    int i;
    pthread_t thread1, thread2;
    Thread_Params params;
    
    // Get output file name from user
    if(argc > 1)
    {
        strcpy(params.filename, argv[1]);
    }
    else
    {
        printf("Missing filename in command line arguments\n");
        return(-12);
    }
    
    // Set input data for CPU scheduling
    params.processes[0].pid = 1; params.processes[0].arrive_t =  8; params.processes[0].burst_t = 10;
    params.processes[1].pid = 2; params.processes[1].arrive_t = 10; params.processes[1].burst_t =  3;
    params.processes[2].pid = 3; params.processes[2].arrive_t = 14; params.processes[2].burst_t =  7;
    params.processes[3].pid = 4; params.processes[3].arrive_t =  9; params.processes[3].burst_t =  5;
    params.processes[4].pid = 5; params.processes[4].arrive_t = 16; params.processes[4].burst_t =  4;
    params.processes[5].pid = 6; params.processes[5].arrive_t = 21; params.processes[5].burst_t =  6;
    params.processes[6].pid = 7; params.processes[6].arrive_t = 26; params.processes[6].burst_t =  2;
    
    //Initialise remaining time to be same as burst time
    for(i = 0; i < NUM_OF_PROCESSES; i++) 
    {
        params.processes[i].remain_t = params.processes[i].burst_t;
    }
    
    // Open FIFO for reading and writing
    params.readFIFO  = open(NAME_OF_FIFO, O_RDONLY | O_NONBLOCK);
    params.writeFIFO = open(NAME_OF_FIFO, O_WRONLY | O_NONBLOCK);
    
    // Output banner to the console
    outputWelcome();
    
    /* initialise semaphore */
    if(sem_init(&(params.readSem), 0, 0) != 0)
    {
        printf("Semaphore initialize error\n");
        return(-10);
    }
    if(sem_init(&(params.writeSem), 0, 0) != 0)
    {
        printf("Semaphore initialize error\n");
        return(-10);
    }
    
    /* create threads */
    if(pthread_create(&thread1, NULL, worker1, (void*)(&params)) != 0)
    {
        printf("Thread 1 created error\n");
        return -1;
    }
    if(pthread_create(&thread2, NULL, worker2, (void*)(&params)) != 0)
    {
        printf("Thread 2 created error\n");
        return -2;
    }
    
    /* wait for threads to exit */
    if(pthread_join(thread1, NULL) != 0)
    {
        printf("Join thread 1 error\n");
        return -3;
    }
    if(pthread_join(thread2, NULL) != 0)
    {
        printf("Join thread 2 error\n");
        return -4;
    }
    
    /* destroy semaphore */
    if(sem_destroy(&(params.readSem)) != 0)
    {
        printf("Semaphore destroy error\n");
        return -5;
    }
    if(sem_destroy(&(params.writeSem)) != 0)
    {
        printf("Semaphore destroy error\n");
        return -5;
    }
    
    /* close the FIFO */
    close(params.readFIFO);
    close(params.writeFIFO);
    
    /* delete the FIFO */
    if(unlink(NAME_OF_FIFO) < 0)
    {
        perror("Error deleting to FIFO");
        return -6;
    }
    
    return 0;
}


/* this function calculates CPU SRTF scheduling, writes waiting time and turn-around time to the FIFO */
void *worker1(void *params)
{
    Thread_Params *worker1_params = (Thread_Params *)(params);
    
    float avg_wait_t       = 0.0;
    float avg_turnaround_t = 0.0;
    
    int i, endTime, smallest, time, remain = 0;
    
    //Placeholder remaining time to be replaced
    worker1_params->processes[NUM_OF_PROCESSES].remain_t = 9999;
    
    //Run function until remain is equal to number of processes
    for(time = 0; remain != NUM_OF_PROCESSES; time++) 
    {
        //Assign placeholder remaining time as smallest
        smallest = 7;
        
        //Check all processes that have arrived for lowest remain time then set the lowest to be smallest
        for(i = 0; i < NUM_OF_PROCESSES; i++)
        {
            if(worker1_params->processes[i].arrive_t <= time 
            && worker1_params->processes[i].remain_t < worker1_params->processes[smallest].remain_t 
            && worker1_params->processes[i].remain_t > 0) 
            {
                smallest = i;
            }
        }
        
        //Decrease remaining time as time increases
        worker1_params->processes[smallest].remain_t--;
        
        //If process is finished, save time information, add to average totals and increase remain
        if(worker1_params->processes[smallest].remain_t == 0) 
        {
            remain++;
            
            endTime = time + 1;
            
            worker1_params->processes[smallest].turnaround_t = endTime - worker1_params->processes[smallest].arrive_t;
            
            worker1_params->processes[smallest].wait_t       = endTime - worker1_params->processes[smallest].burst_t - worker1_params->processes[smallest].arrive_t;
            
            avg_wait_t       += worker1_params->processes[smallest].wait_t;
            
            avg_turnaround_t += worker1_params->processes[smallest].turnaround_t;
        }
    }
    
    avg_wait_t       /= NUM_OF_PROCESSES;
    avg_turnaround_t /= NUM_OF_PROCESSES;
    
    // Write average wait time to FIFO
    if(write(worker1_params->writeFIFO, (float*)(&avg_wait_t), sizeof(avg_wait_t)) < 0)
    {
        perror("Error writing to FIFO");
        exit(-1);
    }
    usleep(WRITE_INTERVAL);
    sem_post(&(worker1_params->readSem));
    
    
    sem_wait(&(worker1_params->writeSem));
    
    
    // Write average turnaround time to FIFO
    if(write(worker1_params->writeFIFO, (float*)(&avg_turnaround_t), sizeof(avg_turnaround_t)) < 0)
    {
        perror("Error writing to FIFO");
        exit(-1);
    }
    usleep(WRITE_INTERVAL);
    sem_post(&(worker1_params->readSem));
    
    pthread_exit(NULL);
}

/* reads the waiting time and turn-around time through the FIFO and writes to text file */
void *worker2(void *params)
{
    Thread_Params *worker2_params = (Thread_Params *)(params);
    
    FILE* writeTxt;
    
    float avg_wait_t       = 0.0;
    float avg_turnaround_t = 0.0;
    
    // Open file to output data
    writeTxt = fopen(worker2_params->filename, "w");
    if(writeTxt == NULL)
    {
        perror("Error opening file");
        exit(-1);
    }
    
    outputHeader();
    outputLine("Calculated Times", 'Y');
    outputFooter();
    
    sem_wait(&(worker2_params->readSem));
    
    // Read average wait time to FIFO
    if(read(worker2_params->readFIFO, &avg_wait_t, sizeof(avg_wait_t)) < 0)
    {
        perror("Error reading from FIFO");
        exit(-1);
    }
    printf("Average wait time: %fs\n", avg_wait_t);
    sem_post(&(worker2_params->writeSem));
    
    sem_wait(&(worker2_params->readSem));
    
    // Read average turnaround time to FIFO
    if(read(worker2_params->readFIFO, &avg_turnaround_t, sizeof(avg_turnaround_t)) < 0)
    {
        perror("Error reading from FIFO");
        exit(-1);
    }
    printf("Average turnaround time: %fs\n", avg_turnaround_t);
    
    // Output CPU scheduling results table
    outputHeader();
    outputLine("Process Schedule Table", 'Y');
    
    printf("╠═════════╦═══════════════╦═══════════════╦═══════════════╦═══════════════════╣\n");
    printf("║ ID      ║ Arrival Time  ║ Burst Time    ║ Wait Time     ║ Turnaround Time   ║\n");
    printf("╠═════════╬═══════════════╬═══════════════╬═══════════════╬═══════════════════╣\n");
    
    int i;
    
    for(i = 0; i < NUM_OF_PROCESSES; i++) 
    {
        printf(
            "║ %d\t  ║ %d\t\t  ║ %d\t\t  ║ %d\t\t  ║ %d\t\t      ║\n", 
            (worker2_params->processes[i].pid),
            (worker2_params->processes[i].arrive_t), 
            (worker2_params->processes[i].burst_t), 
            (worker2_params->processes[i].wait_t), 
            (worker2_params->processes[i].turnaround_t)
        );
    }
    
    printf("╚═════════╩═══════════════╩═══════════════╩═══════════════╩═══════════════════╝\n");
    
    
    // Write data to file then close it
    fprintf(writeTxt, "%s %s %s %f", "Average", "wait", "time:", avg_wait_t);
    fprintf(writeTxt, "%s %s %s %f", "\nAverage", "turnaround", "time:", avg_turnaround_t);
    fclose(writeTxt);
    
    outputHeader();
    outputLine("Program transferred data to file", 'Y');
    outputFooter();
    
    pthread_exit(NULL);
}

/* Outputs the welcome banner to the console */
void outputWelcome()
{
    printf("\x1B[33m");
    printf("                                             _______________________\n");
    printf("   _______________________-------------------                       `\\\n");
    printf(" /:--__                                                              |\n");
    printf("||< > |                                   ___________________________/\n");
    printf("| \\__/_________________-------------------                         |\n");
    printf("|                                                                  |\n");
    printf(" |                                                                  |\n");
    printf(" |                      THE SUPER CPU SCHEDULER                     |\n");
    printf(" |                                                                  |\n");
    printf("  |                         CPU Scheduling,                          |\n");
    printf("  |                              FIFOs,                              |\n");
    printf("  |                        Memory Managment,                         |\n");
    printf("  |                           And Signals                             |\n");
    printf("   |                                                                  |\n");
    printf("   |                       By Jack Romanous                           |\n");
    printf("   |                          (12551519)                             |\n");
    printf("  |                                              ____________________|_\n");
    printf("  |  ___________________-------------------------                      `\\\n");
    printf("  |/`--_                                                                 |\n");
    printf("  ||[ ]||                                            ___________________/\n");
    printf("   \\===/___________________--------------------------\n\n");
    printf("\x1B[0m");
}

/* Outputs the header line to the console */
void outputHeader()
{
    printf("╔═════════════════════════════════════════════════════════════════════════════╗\n");
}

/* Outputs the dividing line to the console */
void outputDivider()
{
    printf("╠═════════════════════════════════════════════════════════════════════════════╣\n");
}

/* Outputs the footer line to the console */
void outputFooter()
{
    printf("╚═════════════════════════════════════════════════════════════════════════════╝\n");
}

/* Outputs a message line to the console */
void outputLine(char *message, const char color)
{
    printf("║ ");
    changeColor(color);
    printf("%s", message);
    
    printf("\x1B[0m");
    
    for(int i = 0; i < (75 - strlen(message)); i++)
    {
        printf(" ");
    }
    printf(" ║\n");
}

/* Outputs a message with variable line to the console */
void outputVar(char *message, const char color, char *value)
{
    int valLength = 0;
    
    printf("║ ");
    changeColor(color);
    printf("%s", message);
    
    for(int i = 0; i < strlen(value); i++)
    {
        if(value[i] != '\n' && value[i] != '\r' )
        {
            printf("%c", value[i]);
            valLength++;
        }
    }
    
    printf("\x1B[0m");
    
    for(int i = 0; i < (75 - strlen(message) - valLength); i++)
    {
        printf(" ");
    }
    printf(" ║\n");
}

/* Changes the console color */
void changeColor(const char color)
{
    switch(color)
    {
        case 'R':
            printf("\x1B[31m");
            break;
        case 'G':
            printf("\x1B[32m");
            break;
        case 'Y':
            printf("\x1B[33m");
            break;
        case 'B':
            printf("\x1B[34m");
            break;
        case 'M':
            printf("\x1b[35m");
            break;
        case 'C':
            printf("\x1b[36m");
            break;
    }
}