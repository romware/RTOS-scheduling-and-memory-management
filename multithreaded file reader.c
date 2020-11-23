/*******************************************************************************
 * 
 * Project: The Super File Reader
 * 
 * Authors:
 * Jack Romanous
 * Braden Payne
 * 
 * Compile instructions:
 * Ensure gcc is installed then run the following command:
 * gcc prog_2.c -o prog_2 -lpthread -lrt
 * 
 * Notes:
 * Toggle DEBUG mode on (1) or off (0) for more information on line 38
 * 
 ******************************************************************************/

/* --- Included Libraries --- */

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/time.h>

/* --- Constant Definitions --- */

#define MAX_LINE_SIZE 255
#define END_HEADER "end_header"
#define DEBUG 1

/* --- Structs --- */

typedef struct ThreadParams
{
    int pipeFile[2];
    sem_t sem_A_to_B, sem_B_to_C, sem_C_to_A;
    char message[MAX_LINE_SIZE];
    char readFilename[MAX_LINE_SIZE];
    char writeFilename[MAX_LINE_SIZE];
    int reachedEnd;
} ThreadParams;

/* --- Prototypes --- */

/* Initializes data and utilities used in thread params */
void initializeData(ThreadParams *params);

/* This thread reads data from a file and writes each line to a pipe */
void *ThreadA(void *params);

/* This thread reads data from pipe used in ThreadA and writes it to a shared variable */
void *ThreadB(void *params);

/* This thread reads from shared variable and outputs non-header to a file */
void *ThreadC(void *params);

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

/* Inputs a filename from the user */
void inputFilename(FILE **file, char *filename, char *permission, char *purpose);

/* Changes the console color */
void changeColor(const char color);

/* --- Main Code --- */
int main(int argc, char const *argv[])
{
    // Initialise variables
    int result;
    pthread_t tid1, tid2, tid3;
    pthread_attr_t attr;
    ThreadParams params;
    
    // Output banner to the console
    outputWelcome();
    
    // Initialise semaphores
    initializeData(&params);
    pthread_attr_init(&attr);
    
    // Create pipe
    result = pipe(params.pipeFile);
    if(result < 0)
    {
        perror("Error creating pipe");
        exit(1);
    }
    
    // Create threads
    if(pthread_create(&tid1, &attr, ThreadA, (void*)(&params))!=0)
    {
        perror("Error creating threads: ");
        exit(-1);
    }
    
    if(pthread_create(&tid2, &attr, ThreadB, (void*)(&params))!=0)
    {
        perror("Error creating threads: ");
        exit(-1);
    }
    
    if(pthread_create(&tid3, &attr, ThreadC, (void*)(&params))!=0)
    {
        perror("Error creating threads: ");
        exit(-1);
    }
    
    // Wait on threads to finish
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    
    return 0;
}

/* Initializes data and utilities used in thread params */
void initializeData(ThreadParams *params) 
{
    if(sem_init(&(params->sem_A_to_B), 0, 1))
    {
        perror("Error initializing sem_A_to_B semaphore");
        exit(0);
    }
    
    if(sem_init(&(params->sem_B_to_C), 0, 0))
    {
        perror("Error initializing sem_B_to_C semaphore");
        exit(0);
    }
    
    if(sem_init(&(params->sem_C_to_A), 0, 0))
    {
        perror("Error initializing sem_C_to_A semaphore");
        exit(0);
    }
    
    return;
}

/* This thread reads data from a file and writes each line to a pipe */
void *ThreadA(void *params) 
{
    // Initialise variables
    ThreadParams *A_thread_params = (ThreadParams *)(params);
    int result;
    char txtLine[MAX_LINE_SIZE];
    FILE* readTxt;
    
    // Get name of input file from user
    sem_wait(&(A_thread_params->sem_A_to_B));
    inputFilename(&readTxt,A_thread_params->readFilename,"r","import");
    sem_post(&(A_thread_params->sem_B_to_C));
    
    // Read all lines from input file
    while(fgets(txtLine, sizeof(txtLine), readTxt) != NULL)
    {
        // Wait for access to thread
        sem_wait(&(A_thread_params->sem_A_to_B));
        
        // Write line to pipe
        result = write(A_thread_params->pipeFile[1], txtLine, MAX_LINE_SIZE);
        if(result < 0)
        { 
            perror("Error writing to pipe");  
            exit(2);
        }
        
        if(DEBUG)
        {
            outputVar("THREADA: Output line to pipe: ", 'M', txtLine);
        }
        
        // Give access to thread
        sem_post(&(A_thread_params->sem_B_to_C));
    }
    
    // Wait for access to thread
    sem_wait(&(A_thread_params->sem_A_to_B));
    
    // Close file and stop reading from it
    A_thread_params->reachedEnd = 1;
    fclose(readTxt);
    
    if(DEBUG)
    {
        outputLine("SUCCESS: Closed input file", 'Y');
    }
    
    // Give access to thread
    sem_post(&(A_thread_params->sem_B_to_C));
}

/* This thread reads data from pipe used in ThreadA and writes it to a shared variable */
void *ThreadB(void *params) 
{
    // Initialise variables
    ThreadParams *B_thread_params = (ThreadParams *)(params);
    int result;
    
    // Thread B has no file to initialise so move to C
    sem_wait(&(B_thread_params->sem_B_to_C));
    sem_post(&(B_thread_params->sem_C_to_A));
    
    // Wait for access to thread
    while(!sem_wait(&(B_thread_params->sem_B_to_C)))
    {
        // Check if file has finished reading
        if(B_thread_params->reachedEnd == 1)
        {
            break;
        }
        
        // Read from pipe
        result = read(B_thread_params->pipeFile[0],B_thread_params->message,MAX_LINE_SIZE);
        if(result <= 0)
        {
            perror("Error reading from pipe");
            exit(4);
        }
        
        if(DEBUG)
        {
            outputVar("THREADB: Read line from pipe: ", 'B', B_thread_params->message);
        }
        
        // Give access to thread
        sem_post(&(B_thread_params->sem_C_to_A));
    }
    
    // Give access to thread
    sem_post(&(B_thread_params->sem_C_to_A));
}

/* This thread reads from shared variable and outputs non-header to a file */
void *ThreadC(void *params) 
{
    // Initialise variables
    ThreadParams *C_thread_params = (ThreadParams *)(params);
    int headerflag = 0;
    FILE* writeTxt;
    
    // Get name of output file from user
    sem_wait(&(C_thread_params->sem_C_to_A));
    inputFilename(&writeTxt,C_thread_params->writeFilename,"w","output");
    outputHeader();
    outputLine("SUCCESS: Program started transferring data", 'Y');
    outputDivider();
    sem_post(&(C_thread_params->sem_A_to_B));
    
    // Wait for access to thread
    while(!sem_wait(&(C_thread_params->sem_C_to_A)))
    {
        // Check if file has finished reading
        if(C_thread_params->reachedEnd == 1)
        {
            break;
        }
        // Check if end of header has been reached
        else if(headerflag == 1)
        {
            fputs(C_thread_params->message, writeTxt);
            
            if(DEBUG)
            {
                outputVar("THREADC: Output line to file: ", 'G', C_thread_params->message);
            }
        }
        // Check if current line to the end header flag
        else if(strstr(C_thread_params->message, END_HEADER) != NULL)
        {
            headerflag = 1;
            
            if(DEBUG)
            {
                outputVar("THREADC: Reached header flag: ", 'C', C_thread_params->message);
            }
        }
        // Otherwise ignore header line
        else
        {
            if(DEBUG)
            {
                outputVar("THREADC: Skipped header line: ", 'R', C_thread_params->message);
            }
        }
        
        if(DEBUG)
        {
            outputDivider();
        }
        
        // Give access to thread
        sem_post(&(C_thread_params->sem_A_to_B));
    }
    
    // Close file
    fclose(writeTxt);
    
    if(DEBUG)
    {
        outputLine("SUCCESS: Closed output file", 'Y');
    }
    
    // Output completion message
    outputVar("SUCCESS: Read all data from ", 'Y', C_thread_params->readFilename);
    outputVar("SUCCESS: Output all data to ", 'Y', C_thread_params->writeFilename);
    outputFooter();
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
    printf(" |                       THE SUPER FILE READER                      |\n");
    printf(" |                                                                  |\n");
    printf("  |                         Read from files,                         |\n");
    printf("  |                          Write to files,                         |\n");
    printf("  |                          Multithreaded,                          |\n");
    printf("  |                  And so many more great features                  |\n");
    printf("   |                                                                  |\n");
    printf("   |                 By Braden Payne and Jack Romanous                |\n");
    printf("   |                     (12947697)        (12551519)                |\n");
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
    int i;
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

/* Inputs a filename from the user */
void inputFilename(FILE **file, char *filename, char *permission, char *purpose)
{
    int spacesTop = 20 - strlen(purpose);
    int spacesBot = 30 - strlen(purpose);
    
    // Prompt user for the path to the file they want to select
    printf(
        "\n\n"
        "╔════════════════════════════════════════╗\n"
        "║ Please enter the filename of the file  ║\n"
        "║ you would like to %s%*s ║\n"
        "║                                        ║\n"
        "║ eg. %s.txt%*s ║\n"
        "╠════════════════════════════════════════╝\n"
        "╚ ► ",purpose,spacesTop,"",purpose,spacesBot,"");
    
    scanf("%s", filename);
    *file = fopen(filename, permission);
    
    // Check if the file is unreadable
    while(*file == NULL)
    {
        // Print error message and re-ask for the file path
        printf(
            "╔════════════════════════════════════════╗\n"
            "║ \x1B[31mError: File not found                 \x1B[0m ║\n"
            "╠════════════════════════════════════════╣\n"
            "║ Please select a valid filename         ║\n"
            "╠════════════════════════════════════════╝\n"
            "╚ ► ");
        scanf("%s", filename);
        *file = fopen(filename, permission);
    }
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