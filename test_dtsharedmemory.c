/**
 *
 *	darwintrace generally doesn't consume more than 10 MB. It generally only
 *	inserts 400-500 paths during a phase whose length generally varies from 3 - 150.
 *	This test inserts randomly generated strings, so most of them generally
 *	don't share same prefix and insertion consumes a good amount of memory.
 *	But in case of paths, lots of them have common prefixes which reduces memory usage.
 *
 *	While setting the macros below its better to be realistic with numbers
 *	and if testing with big numbers, remember to set `LARGE_MEMORY_NEEDED` to 1
 *	in `dtsharedmemory.h`.
 *
 *	As per the tests till now, the code works stably for both 64 bit and 32 bit machine.
 *	To test as a 32-bit machine, use -m32 flag in Makefile. Although for big numbers
 *	memory will run out soon in case of 32-bit machine.
 *	If testing on 64 bit machine, the code should work stably for large numbers too,
 *	provided that LARGE_MEMORY_NEEDED (in dtsharedmemory.h) is set to 1.
 *
 **/

#define NUMBER_OF_PROCESSES 4

#define NUMBER_OF_THREADS_PER_PROCESS 4 //excluding main
#define NUMBER_OF_STRINGS_TO_BE_INSERTED_BY_EACH_THREAD 2000

#define MAX_STRING_SIZE 3
#define MIN_STRING_SIZE 150



#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/param.h>

#include "dtsharedmemory.h"


//Store any errors that occured during the test in this file
//Simply redirect stderr to this file referenced by this fd
int errors_log_fd;

//Store all messages printed by test here
FILE *test_messages;


//If flag is true, a message on stdout gets printed to check errors.log because errors exist
bool flag = false;


//Argument to individual thread
struct PathData
{
	int number_of_strings;	//To be inserted by that particular thread
	char **path;			//Array of those strings
	uint8_t *flags;	//Array of permissions associated with those strings
};


void* pathInserter(void* arg);
void* pathSearcher(void* arg);

char *get_random_string(int minLength, int maxLength);
void prepareThreadArguments(struct PathData *argsToThreads, int number_of_strings);


int main()
{
	//________________________________________________________________________________
	//SETUP ERROR LOGGING
	//SETUP FILE NAME FOR SHARED FILE AND ITS STATUS FILE
	//________________________________________________________________________________
	
	//Open file for logging errors if any
	test_messages = fopen("test_messages.log", "w");
	errors_log_fd = open("errors.log", O_CREAT | O_WRONLY, 0600);
	
	
	if ( errors_log_fd == -1 || dup2(errors_log_fd, STDERR_FILENO) == -1)
		fprintf(stderr, "Couldn't redirect output to errors.log, errors will be printed on stderr\n");
	
	char mktemp_dtsm_template[MAXPATHLEN] 		= "macports-dtsm-XXXXXX";
	char mktemp_dtsm_status_template[MAXPATHLEN] 	= "macports-dtsm-status-XXXXXX";
	
	char *dtsm_status_file 	= mktemp(mktemp_dtsm_status_template);
	char *dtsm_file		= mktemp(mktemp_dtsm_template);
	
	
	//________________________________________________________________________________
	
	
	//________________________________________________________________________________
	//(1) 	SETUP TO ALLOW ONLY PARENT PROCESS TO FORK
	//(2) 	SPLIT NUMBER_OF_PROCESSES EQUALLY 3 PHASES, SO THEY CAN GET FORKED AT
	//		START OF PROGRAM, BEFORE INSERT & AFTER INSERT(BEFORE SEARCH)
	//________________________________________________________________________________
	int i;
	int isParentProcess = 1;//only allow parent process to fork
	
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
	
	
	pid_t childProcesses[NUMBER_OF_PROCESSES];//To wait for all processes
	int processCountInThisPhase;
	int processno = 0;
	int atPhase = 0;
	
	//________________________________________________________________________________
	
	printf("\nEXPANDING_SIZE on this machine = %zu\n", EXPANDING_SIZE);
	
	//subtracting num of processes forked in last phase because they don't do insert
	printf("\nStrings to be inserted = %d\n", (insertion_count_per_process * (NUMBER_OF_PROCESSES - processCountPerPhase[number_of_fork_phases - 1])));
	
	//________________________________________________________________________________
	//FORK PHASE 1
	//________________________________________________________________________________
	processCountInThisPhase = processCountPerPhase[atPhase++];
	
	for(i = 0 ; i < processCountInThisPhase ; ++i)
	{
		if(isParentProcess)
		{
			childProcesses[processno++] = isParentProcess = fork();
		}
	}
	//________________________________________________________________________________
	
	
	
	//________________________________________________________________________________
	//PREPARE ARGUMENTS TO BE SENT TO THREADS.
	//JUST GENERATE `NUMBER_OF_STRINGS_TO_BE_INSERTED_BY_EACH_THREAD` RANDOM STRINGS
	//AND SEND THEM TO EACH THREAD
	//________________________________________________________________________________
	
	srand(time(0));
	
	struct PathData argsToThreads[NUMBER_OF_THREADS_PER_PROCESS];
	
	for(i = 0 ; i < NUMBER_OF_THREADS_PER_PROCESS ; ++i)
	{
		
		prepareThreadArguments(
				       &argsToThreads[i],
				       NUMBER_OF_STRINGS_TO_BE_INSERTED_BY_EACH_THREAD
				       );
		
	}
	//________________________________________________________________________________
	
	
	//________________________________________________________________________________
	//SET SHARED MEMORY MANAGER
	//________________________________________________________________________________
	clock_t t;
	long double time_taken;
	
	bool didSetManager;
	
	//Monitor time taken to __dtsharedmemory_set_manager()
	t = clock();
	
	didSetManager = __dtsharedmemory_set_manager(dtsm_status_file, dtsm_file);
	
	if(!didSetManager)
	{
		fprintf(test_messages, "\n__dtsharedmemory_set_manager() failed. Test failed\n\n");
		flag = true;
	}
	
	t = clock() - t;
	time_taken = ((long double)t)/CLOCKS_PER_SEC;
	
	printf("\nTime taken by __dtsharedmemory_set_manager() = %Lf\n", time_taken);
	
	
	//Monitor time taken to __dtsharedmemory_set_manager() one second call
	//This should be way less because Global(manager) is already set
	t = clock();
	
	didSetManager = __dtsharedmemory_set_manager(dtsm_status_file, dtsm_file);
	
	if(!didSetManager)
	{
		fprintf(test_messages, "\n__dtsharedmemory_set_manager() failed when called 2nd time. Test failed\n\n");
		flag = true;
	}
	
	t = clock() - t;
	time_taken = ((long double)t)/CLOCKS_PER_SEC;
	
	printf("\nTime taken by second call to __dtsharedmemory_set_manager() = %Lf\n", time_taken);
	//________________________________________________________________________________
	
	
	//________________________________________________________________________________
	//FORK PHASE 2
	//________________________________________________________________________________
	processCountInThisPhase = processCountPerPhase[atPhase++];
	
	for(i = 0 ; i < processCountInThisPhase ; ++i)
	{
		if(isParentProcess)
		{
			childProcesses[processno++] = isParentProcess = fork();
		}
	}
	//________________________________________________________________________________
	
	
	pthread_t tids[NUMBER_OF_THREADS_PER_PROCESS];
	
	//________________________________________________________________________________
	//INSERTION
	//________________________________________________________________________________
	
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
	time_taken = ((long double)t)/CLOCKS_PER_SEC;
	
	printf("\nIn process %d Time taken to insert %d strings by %d threads = %Lf\n", getpid(), insertion_count_per_process, NUMBER_OF_THREADS_PER_PROCESS, time_taken);
	//________________________________________________________________________________
	
	
	//________________________________________________________________________________
	//FORK PHASE 3
	//________________________________________________________________________________
	processCountInThisPhase = processCountPerPhase[atPhase++];
	
	for(i = 0 ; i < processCountInThisPhase ; ++i)
	{
		if(isParentProcess)
		{
			childProcesses[processno++] = isParentProcess = fork();
		}
	}
	//________________________________________________________________________________
	
	
	//________________________________________________________________________________
	//SEARCH
	//________________________________________________________________________________
	
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
	time_taken = ((long double)t)/CLOCKS_PER_SEC;
	
	printf("\nTime taken to search %d strings by %d threads = %Lf\n", insertion_count_per_process, NUMBER_OF_THREADS_PER_PROCESS, time_taken);
	//________________________________________________________________________________
	
	if (flag)
	{
		printf("\nTest failed, check errors.log and test_messages.log\n");
	}
	
	//________________________________________________________________________________
	//WAIT FOR CHILD PROCESSES
	//________________________________________________________________________________
	if(isParentProcess)
	{
		for (i = 0 ; i < NUMBER_OF_PROCESSES - 1 ; ++i)
		{
			waitpid(childProcesses[i], NULL, 0);
		}
		
		size_t realFileSizeUsed = __dtsharedmemory_getUsedSharedMemorySize();
		
		
		printf("\nShared memory size used after all processes ended = %zu\n\n", realFileSizeUsed);
	}
	//________________________________________________________________________________
	
}

void prepareThreadArguments(struct PathData *argsToThreads, int number_of_strings)
{
	int i;
	
	argsToThreads->number_of_strings = number_of_strings;
	argsToThreads->path = (char **)malloc( sizeof(char *) * number_of_strings);
	argsToThreads->flags = (uint8_t *)malloc( sizeof(uint8_t) * number_of_strings);
	
	if (argsToThreads->path == NULL) {
		print_error("null");
	}
	
	if (argsToThreads->flags == NULL) {
		print_error("null");
	}
	
	
	for (i = 0 ; i < number_of_strings ; ++i)
	{
		argsToThreads->path[i] = get_random_string(MIN_STRING_SIZE, MAX_STRING_SIZE);
		argsToThreads->flags[i] = (bool)rand() ? ALLOW_PATH : DENY_PATH;
	}
	
}

char *get_random_string(int minLength, int maxLength)
{
	
	int strSize = (rand() % maxLength) + minLength;
	
	char *random_string = (char *)malloc(sizeof(char) * (strSize + 2) );
	
	if (random_string == NULL) {
		print_error("null");
	}
	
	uint8_t random_char;
	
	int i;
	
	for (i = 0 ; i < strSize ; ++i)
	{
		
		random_char = (rand() % POSSIBLE_CHARACTERS) + LOWER_LIMIT;
		random_char = !random_char ? 1 : random_char; //shifting coz don't want 0, which is '\0'
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
		
		result = __dtsharedmemory_insert(pathToBeInserted->path[i], pathToBeInserted->flags[i]);
		
		if (!result)
		{
			fprintf(test_messages, "[%s] : \n\nInsertion failed for - %s\n", __FILE__, pathToBeInserted->path[i]);
			fprintf(test_messages, "\n------------------------------------------------------\n");
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
	
	uint8_t fetchedFlags;
	int size = pathToBeSearched->number_of_strings;
	
	for (i = 0 ; i < size ; ++i) {
		
		exists = __dtsharedmemory_search(pathToBeSearched->path[i], &(fetchedFlags) );
		
		
		if(!exists)
		{
			
			fprintf(test_messages, "[%s] : \n\nSearch failed for - %s\n", __FILE__, pathToBeSearched->path[i]);
			fprintf(test_messages, "\n------------------------------------------------------\n");
			flag = true;
		}
		else
		{
			if (fetchedFlags != pathToBeSearched->flags[i])
			{
				fprintf(test_messages, "\n %s permission not correct. Test failed\n\n", pathToBeSearched->path[i]);
				flag = true;
			}
		}
		
	}
	
	pthread_exit(0);
}


