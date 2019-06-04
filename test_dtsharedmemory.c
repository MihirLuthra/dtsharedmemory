/**
 *
 *	darwintrace generally doesn't consume more than 100 MB.
 *	This test inserts randomly generated strings, so most of them generally
 *	don't share same prefix and insertion consumes a good amount of memory.
 *	But in case of paths, lots of them have common prefixes which reduces memory usage.
 *
 *	While setting the macros below its better to be realistic with numbers
 *	and if testing with big numbers, remember to set `LARGE_MEMORY_NEEDED` to 1
 *	in `dtsharedmemory.h`.
 *
 **/

#define NUMBER_OF_PROCESSES 8

#define NUMBER_OF_THREADS_PER_PROCESS 3 //excluding main
#define NUMBER_OF_STRINGS_TO_BE_INSERTED_BY_EACH_THREAD 1000

#define MAX_STRING_SIZE 25
#define MIN_STRING_SIZE 20



#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#include "dtsharedmemory.h"

#define SHARED_MEMORY_STATUS_FILE_NAME 	".shared_Memory_Status"
#define SHARED_MEMORY_FILE_NAME 		".shared_Memory"


//Store any errors that occured during the test in this file
FILE *errors_log;


//If flag is true, a message on stdout gets printed to check errors.log because errors exist
bool flag = false;


//Argument to individual thread
struct PathData
{
	int number_of_strings;	//To be inserted by that particular thread
	unsigned char **path;	//Array of those strings
	bool *pathPersmission;	//Array of permissions associated with those strings
};


void* pathInserter(void* arg);
void* pathSearcher(void* arg);

unsigned char *get_random_string(int minLength, int maxLength);
void prepareThreadArguments(struct PathData *argsToThreads, int number_of_strings);


int main()
{
	
	//Create status and shared memory file and set to initial size
	if(access(SHARED_MEMORY_STATUS_FILE_NAME, F_OK) == -1 || access(SHARED_MEMORY_FILE_NAME, F_OK) == -1)
	{
		int fd;
		
		fd = creat(SHARED_MEMORY_STATUS_FILE_NAME, 0600);
		ftruncate(fd, sizeof(struct SharedMemoryStatus));
		
		fd = creat(SHARED_MEMORY_FILE_NAME, 0600);
		ftruncate(fd, INITIAL_FILE_SIZE);
	}
	//Open file for logging errors if any
	errors_log = fopen("errors.log", "w");
	
	
	int i;
	int isParentProcess = 1;//only allow parent process to fork
	
	//There are 3 phases for forking
	//1.Starting of program
	//2.Before insert
	//3.After insert & before search
	int number_of_fork_phases = 3;
	int *processCountPerPhase = (int *)malloc(sizeof(int) * number_of_fork_phases);
	
	//NUMBER_OF_PROCESSES get forked in number_of_fork_phases parts
	//So dividing NUMBER_OF_PROCESSES into number_of_fork_phases parts
	for( i = 0 ; i < number_of_fork_phases ; ++i)
	{
		//-1 because parent process is also a part
		processCountPerPhase[i] = (NUMBER_OF_PROCESSES-1)/number_of_fork_phases + ( ((((NUMBER_OF_PROCESSES-1) % number_of_fork_phases) - i) > 0) ? 1 : 0);
	}
	
	int insertion_count_per_process = NUMBER_OF_STRINGS_TO_BE_INSERTED_BY_EACH_THREAD * NUMBER_OF_THREADS_PER_PROCESS;
	
	//subtracting num of processes forked in last phase because they don't insert
	printf("\nStrings to be inserted = %d\n\n", (insertion_count_per_process * (NUMBER_OF_PROCESSES - processCountPerPhase[number_of_fork_phases - 1])));
	
	
	pid_t childProcesses[NUMBER_OF_PROCESSES];//To wait for all processes
	int processCountInThisPhase;
	int processno = 0;
	int atPhase = 0;
	
	//Fork Phase 1
	processCountInThisPhase = processCountPerPhase[atPhase++];
	
	for(i = 0 ; i < processCountInThisPhase ; ++i)
	{
		if(isParentProcess)
		{
			childProcesses[processno++] = isParentProcess = fork();
		}
	}

	
	clock_t t;
	double time_taken;
	struct PathData argsToThreads[NUMBER_OF_THREADS_PER_PROCESS];
	
	
	srand(time(0));
	
	// Feed random data to be sent to threads
	for(i = 0 ; i < NUMBER_OF_THREADS_PER_PROCESS ; ++i)
	{
		
		prepareThreadArguments(
							   &argsToThreads[i],
							   NUMBER_OF_STRINGS_TO_BE_INSERTED_BY_EACH_THREAD
							   );
		
	}


	bool didAppointManager;
	
	//Monitor time taken to appointSharedMemoryManager()
	t = clock();
	
	didAppointManager = appointSharedMemoryManager(SHARED_MEMORY_STATUS_FILE_NAME, SHARED_MEMORY_FILE_NAME);
	
	if(!didAppointManager)
	{
		fprintf(errors_log, "\nappointSharedMemoryManager() failed. Test failed\n\n");
		flag = true;
	}
	
	t = clock() - t;
	time_taken = ((double)t)/CLOCKS_PER_SEC;
	
	printf("\nTime taken by appointSharedMemoryManager() = %f\n", time_taken);
	
	
	//Fork Phase 2
	processCountInThisPhase = processCountPerPhase[atPhase++];
	
	for(i = 0 ; i < processCountInThisPhase ; ++i)
	{
		if(isParentProcess)
		{
			childProcesses[processno++] = isParentProcess = fork();
		}
	}

	
	
	pthread_t tids[NUMBER_OF_THREADS_PER_PROCESS];

	
	t = clock();
	
	// Launch threads for insertion
	for (i = 0; i < NUMBER_OF_THREADS_PER_PROCESS ; ++i)
	{
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_create(&tids[i], &attr, pathInserter, &argsToThreads[i]);
	}
	
	// Wait until insertions are done
	for (i = 0; i < NUMBER_OF_THREADS_PER_PROCESS; ++i) {
		
		pthread_join(tids[i], NULL);
	}

	
	t = clock() - t;
	time_taken = ((double)t)/CLOCKS_PER_SEC;
	
	printf("\nIn process %d Time taken to insert %d strings by %d threads = %f\n", getpid(), insertion_count_per_process, NUMBER_OF_THREADS_PER_PROCESS, time_taken);
	
	
	//Fork Phase 3
	processCountInThisPhase = processCountPerPhase[atPhase++];
	
	for(i = 0 ; i < processCountInThisPhase ; ++i)
	{
		if(isParentProcess)
		{
			childProcesses[processno++] = isParentProcess = fork();
		}
	}
	
	
	t = clock();
	// Launch threads for search
	for (i = 0; i < NUMBER_OF_THREADS_PER_PROCESS; ++i)
	{
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_create(&tids[i], &attr, pathSearcher, &argsToThreads[i]);
	}
	
	
	
	// Wait until searches are done
	for (i = 0; i < NUMBER_OF_THREADS_PER_PROCESS; ++i) {
		
		pthread_join(tids[i], NULL);
	}
	
	t = clock() - t;
	time_taken = ((double)t)/CLOCKS_PER_SEC;
	
	printf("\nTime taken to search %d strings by %d threads = %f\n", insertion_count_per_process, NUMBER_OF_THREADS_PER_PROCESS, time_taken);
	
	
	if (flag)
	{
		printf("\nTest failed, check errors.log\n");
	}
	
	
	if(isParentProcess)
	{
		for (i = 0 ; i < NUMBER_OF_PROCESSES - 1 ; ++i)
		{
			waitpid(childProcesses[i], NULL , 0);
		}
		
		struct SharedMemoryManager managerForSize = getAppointedManager();
		
		printf("\nShared memory size used after all processes ended = %zu\n\n",  managerForSize.statusFile_mmap_base->writeFromOffset);
	}
	
}

void prepareThreadArguments(struct PathData *argsToThreads, int number_of_strings)
{
	int i;
	
	argsToThreads->number_of_strings = number_of_strings;
	argsToThreads->path = (unsigned char **)malloc( sizeof(unsigned char *) * number_of_strings);
	argsToThreads->pathPersmission = (bool *)malloc( sizeof(bool) * number_of_strings);
	
	if (argsToThreads->path == NULL) {
		print_error("null");
	}
	
	if (argsToThreads->pathPersmission == NULL) {
		print_error("null");
	}

	
	for (i = 0 ; i < number_of_strings ; ++i)
	{
		argsToThreads->path[i] = get_random_string(MIN_STRING_SIZE, MAX_STRING_SIZE);
		argsToThreads->pathPersmission[i] = (bool)rand();
 	}

}

unsigned char *get_random_string(int minLength, int maxLength)
{

	int strSize = (rand() % maxLength) + minLength;
	
	unsigned char *random_string = (unsigned char *)malloc(sizeof(unsigned char) * (strSize + 2) );
	
	if (random_string == NULL) {
		print_error("null");
	}
	
	uint8_t random_char;
	
	int i;
	
	for (i = 0 ; i < strSize ; ++i)
	{
		do{
			random_char = (rand() % 96) + 32;
		}while(!random_char);
		random_string[i] = random_char;
	}
	
	random_string[i] = '\0';

	return random_string;
	
}


void* pathInserter(void* arg)
{
	struct PathData *pathToBeInserted = (struct PathData*)arg;
	
	bool result;
	
	int i;
	int size = pathToBeInserted->number_of_strings;

	
	for (i = 0 ; i < size; ++i) {
		
		result = __dtsharedmemory_insert(pathToBeInserted->path[i], pathToBeInserted->pathPersmission[i]);
		
		if (!result)
		{
			fprintf(errors_log, "\n %s : Insertion failed\n\n", pathToBeInserted->path[i]);
			flag = true;
		}
	}

	pthread_exit(0);
}


void* pathSearcher(void* arg)
{
	struct PathData *pathToBeSearched = (struct PathData*)arg;
	
	bool exists;
	
	int i;

	bool fetchedPathPersmission;
	int size = pathToBeSearched->number_of_strings;
	
	for (i = 0 ; i < size ; ++i) {
		
		exists = __dtsharedmemory_search(pathToBeSearched->path[i], &(fetchedPathPersmission) );
		
		
		if(!exists)
		{
			
			fprintf(errors_log, "\n %s not found in shared memory. Test failed\n\n", pathToBeSearched->path[i]);
			flag = true;
		}
		else
		{
			if (fetchedPathPersmission != pathToBeSearched->pathPersmission[i])
			{
				fprintf(errors_log, "\n %s permission not correct. Test failed\n\n", pathToBeSearched->path[i]);
				flag = true;
			}
		}
		
	}
	
	pthread_exit(0);
}


