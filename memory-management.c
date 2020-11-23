/*******************************************************************************
 * 
 * Project: Memory Management
 * 
 * Authors:
 * Jack Romanous
 * 
 * Compile instructions:
 * Ensure that gcc is installed and run the following command:
 * gcc -Wall -O2 memory-management.c -o memory-management -lpthread -lrt
 * 
*******************************************************************************/

#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

//Number of pagefaults in the program
int pageFaults = 0;

//Function declaration
void SignalHandler(int signal);

/**
 Main routine for the program. In charge of setting up threads and the FIFO.

 @param argc Number of arguments passed to the program.
 @param argv array of values passed to the program.
 @return returns 0 upon completion.
 */
int main(int argc, char* argv[])
{
    //Register Ctrl+c(SIGINT) signal and call the signal handler for the function.
    signal(SIGINT, SignalHandler);
    
    int i;
    // reference number
    int REFERENCESTRINGLENGTH=24;
    //Argument from the user on the frame size, such as 4 frames in the document
    int frameSize = 4;
    if(argc > 1)
    {
        frameSize = atoi(argv[1]);
    }
    else
    {
        printf("Missing framesize in command line arguments\n");
        return(-12);
    }
    //Frame where we will be storing the references. -1 is equivalent to an empty value
    uint frame[REFERENCESTRINGLENGTH];
    //Reference string from the assignment outline
    int referenceString[24] = {7,0,1,2,0,3,0,4,2,3,0,3,0,3,2,1,2,0,1,7,0,1,7,5};
    //Next position to write a new value to.
    int nextWritePosition = 0;
    //Boolean value for whether there is a match or not.
    bool match = false;
    //Current value of the reference string.
    int currentValue;

    //Initialise the empty frame with -1 to simulate empty values.
    for(i = 0; i < frameSize; i++)
    {
        frame[i] = -1;
    }

    //Loop through the reference string values.
    for(i = 0; i < REFERENCESTRINGLENGTH; i++)
    {
        currentValue = referenceString[i];
        match = false;
        
        printf("Index %d: ",currentValue);
        
        // Check if current value is in frame
        for(int f = 0; f < frameSize; f++)
        {
            if(frame[f] == currentValue)
            {
                match = true;
                break;
            }
        }
        
        // If it is not, then increment page faults and the frame index position
        if(match == false)
        {
            pageFaults++;
            
            // Update frame
            frame[nextWritePosition] = currentValue;
            
            // Move frame position and reset if needed
            nextWritePosition++;
            if(nextWritePosition == frameSize)
            {
                nextWritePosition = 0;
            }
        }
        
        // Outpput the frame state in the concole
        for(int f = 0; f < frameSize; f++)
        {
            // Dont include blank frame values
            if(frame[f] != -1)
            {
                printf("%d ", frame[f]);
            }
            else
            {
                printf("  ");
            }
        }
        
        // Output the fault number if there is one
        if(match == false)
        {
            printf("FAULT %d", pageFaults);
        }
        
        printf("\n");
    }

    //Sit here until the ctrl+c signal is given by the user.
    while(1)
    {
        sleep(1);
    }

    return 0;
}

/**
 Performs the final print when the signal is received by the program.

 @param signal An integer values for the signal passed to the function.
 */
void SignalHandler(int signal)
{
    printf("\nTotal page faults: %d\n", pageFaults);
    exit(0);
}