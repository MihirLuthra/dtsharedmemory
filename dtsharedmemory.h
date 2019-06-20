#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#ifndef __SHARED_MEMORY_H__
#define __SHARED_MEMORY_H__


//For debugging purposes
#define DISABLE_DUMPING_AND_RECYCLING 	(0)
#define DISABLE_MEMORY_EXPANSION 		(0)
//Instead of disabling DISABLE_MEMORY_EXPANSION, a better idea is to
//take INITIAL_FILE_SIZE big enough such that expansion isn't needed


//If printing location is stderr, it can cause conflicts with build.
#define	DEBUG_MESSAGES_ALLOWED 	(1)


//To debug any of them , just make them 1
//If DEBUG_MESSAGES_ALLOWED is not 1, none of these flags get debugged even if they are 1.
#define	DEBUG_PRINT_MESSAGES 	(1)
#define	DEBUG_FAIL_MESSAGES		(1)



//{{{{{{{{{{{{{{{{{{{{{{{{{{
#if (DEBUG_MESSAGES_ALLOWED && 1) && (DEBUG_PRINT_MESSAGES && 1)
		#define print_error(errorDescription) fprintf(stderr, "%s : func(%s) : %s : %s\n", __FILE__, __func__, errorDescription, strerror(errno));
#else
		#define print_error(errorDescription)
#endif
//}}}}}}}}}}}}}}}}}}}}}}}}}}



//{{{{{{{{{{{{{{{{{{{{{{{{{{
#if (DEBUG_MESSAGES_ALLOWED && 1) && (DEBUG_FAIL_MESSAGES && 1)

	#define FAIL_IF(condition, message, returnVal) \
	if((condition)){\
		fprintf(stderr, "%s : func(%s) : %s : %s\n", __FILE__, __func__, message, strerror(errno));\
		return returnVal;\
	}

#else

	#define FAIL_IF(condition, message, returnVal) \
	if((condition)){\
		return returnVal;\
	}

#endif
//}}}}}}}}}}}}}}}}}}}}}}}}}}



/**
 *
 *	If need more than 4 GB, set this as 1.
 *
 *	By setting this 1, memory usage doubles as well.
 *
 *	This macro simply makes program to switch to use `uint32_t` instead of `size_t`
 *	in struct CNode "only".
 *
 *	By doing that, array of `possibilities`, which generally would use (64bits)*array size
 *	because of size_t(unless a 32 bit machine) drops usage to half.
 **/
#define LARGE_MEMORY_NEEDED (1)



/**
 *	Set bitmap size according to size_t.
 *	size_t is being used as data type of bitmap because it gets
 *	largest possible unsigned integer (32 bit or 64 bit).
 **/

#ifdef __LP64__
#	define NO_OF_BITS 64
#else
#	define NO_OF_BITS 32
#endif



#define KB(x) (x * 1024)
#define MB(x) (x * 1024 * 1024)
#define GB(x) (x * (size_t)1024 * 1024 * 1024)



/*
 *	EXPANDING_SIZE should be big enough such that newSize is never made less than
 *	the current file size by truncate(2) and will also cause other problems.
 *
 *	After testing till now, at least 20 MB expanding size seems necessary for the library
 *	to function correctly. 
 *
 */
#define INITIAL_FILE_SIZE 	MB(20) //Keep it greater than sizeof(CNode)+sizeof(INode)

#define EXPANDING_SIZE 		(MB(10) * sysconf(_SC_NPROCESSORS_ONLN))
//EXPANDING_SIZE should vary accordingly with number of processors and processing speed.
//The bigger factor is number of processors, which has been considered above.


struct SharedMemoryStatus;


/*
 *	#Member1(sharedMemoryFile_mmap_base):
 *		This stores the base address obtained by mmap(2) call on shared memory file.
 *		Also gcc and clang allow void * arithemetic, so any offset can simply be accessed as
 *		*(sharedMemoryFile_mmap_base + offset).
 *
 *	#Member2(sharedMemoryFile_mapping_size):
 *		Size of the shared memory file mapping.
 *
 *	#Member3(sharedMemoryFile_name):
 *		Name of file that is to be used for storing paths in a ctrie data structure.
 *
 *	#Member4(sharedMemoryFile_fd):
 *		File descriptor of shared memory file.
 *
 *	#Member5(statusFile_mmap_base):
 *		This stores the base address obtained by mmap(2) call on shared memory status file.
 *
 *	#Member6(statusFile_fd):
 *		File descriptor of shared memory status file.
 *
 *	#Member7(statusFile_name):
 *		Name of shared memory status file.
 *
 */
struct SharedMemoryManager{

	//shared memory file
	void *			sharedMemoryFile_mmap_base;
	size_t  		sharedMemoryFile_mapping_size;
	const char * 	sharedMemoryFile_name;
	int 			sharedMemoryFile_fd;
	
	//status file
	struct	SharedMemoryStatus *	statusFile_mmap_base;
	int								statusFile_fd;
	const char *					statusFile_name;
	
};



#define DUMP_YARD_SIZE 64

/*
 *	The bitmap may need to store more than NO_OF_BITS bits. A size_t can only store
 *	mapping for max system bits. So to make it possible to store more than NO_OF_BITS bit,
 *	an array of such size_t is created whose size is given by DUMP_YARD_BITMAP_ARRAY_SIZE.
 */
#define DUMP_YARD_BITMAP_ARRAY_SIZE \
((DUMP_YARD_SIZE % NO_OF_BITS == 0) ? DUMP_YARD_SIZE/NO_OF_BITS : ((DUMP_YARD_SIZE/NO_OF_BITS) + 1))

/**
 *	#Member1(writeFromOffset):
 *		The offset from which data should be written.
 *
 *	#Member2(sharedMemoryFileSize)
 *		Size of the file mapped into the process. New threads that call openSharedMemoryFile()
 *		use this file size for mmap(2).
 *
 *	The members after this are used in dumping and recycling of wasted memory.
 *
 *	#Member3(wastedMemoryDumpYard):
 *		To update a CNode to contain a new entry in bitmap,
 *		a new copy of the same CNode is created with updated bitmap entry
 *		and placed at a newly reserved offset. This new offset is assigned to
 *		the parent INode by CAS because of which the memory at which old CNode
 *		resides gets wasted. That old offset is dumped into this array so that
 *		when writing a new CNode, the wasted offsets can
 *		be reused to write CNode instead of using more memory.
 *
 *	#Member5(parentINodesOfDumper):
 *		This array stores the parent INode of the subsequent wasted CNode that was dumped.
 *		The reason to store this is the "Common parent race condition problem".
 *
 * #### Common parent race condition problem ####
 *
 *		This is a sick race condition among sibling nodes.
 *		Suppose 3 threads, each dealing with different child of same parent.
 *		(1st thread)1st child prepared its old offset, and is now preparing new offset to
 *		get ready for CAS. Meanwhile (2nd thread)2nd child CASd the old offset to
 *		point to a new offset and it dumped the old offset for recycling after the
 *		new offset contained updated bitmap. Meanwhile the (3rd thread)3rd child
 *		recycled the offset dumped by the 2nd node and CASd it to be child of current parent.
 *		Bitmap has got updations from 2nd and 3rd child node.
 *		Now when 1st node will attemp to CAS, it will find the oldValue
 *		and the actual value to be equal, because 3rd node reused old offset,
 *		but the new value 1st node replaces don't contain bitmap entries set by
 *		2nd and 3rd node leading to data loss.
 *
 *		""For this reason a node can not recycle offsets dumped by its sibling.""
 *		See body of __dtsharedmemory_insert() to get an idea of the situation.
 *
 *
 *	#Member5(bitmapForDumping):
 *		While dumping into array, to ensure thread safety, the thread which is
 *		adding the wasted offset selects any index in `wastedMemoryDumpYard[]` where
 *		bitmap value is 0 and sets it to 1 in order to reserve that index.
 *		Now no other thread can write at this index.
 *		When this thread is done setting the value at index,
 *		it sets `bitmapForRecycling` to indicate that `wastedOffset` at
 *		this index is ready to be used.
 *
 *	#Member6(bitmapForRecycling):
 *		The thread which wants an offset to store a CNode,
 *		chekcs this bitmap for wherever it is 1.
 *		It preserves value at that index and sets the bitmap at this index 0.
 *		It then sets `bitmapForDumping` at the same index as 0 too so that
 *		other threads can dump `wastedOffset` into that index.
 *
 *	size_t is being used as data type of bitmap because it gets
 *	largest possible unsigned integer (32 bit or 64 bit).
 *
 *	This feature of dumping and recycling drops more than half of memory usage.
 *	Without dumping and recyclying, if the memory usage was "12MB", it drops to almost
 *	"6MB" when using dumping and recycling.
 *
 **/
struct SharedMemoryStatus
{
	
#if !(DISABLE_DUMPING_AND_RECYCLING)
//{
	size_t wastedMemoryDumpYard [DUMP_YARD_SIZE];
	size_t parentINodesOfDumper [DUMP_YARD_SIZE];
	
#	ifdef HAVE_STDATOMIC_H
	//{
		_Atomic(size_t) writeFromOffset;
		_Atomic(size_t) sharedMemoryFileSize;
	
		_Atomic(size_t) bitmapForDumping	 [DUMP_YARD_BITMAP_ARRAY_SIZE];
		_Atomic(size_t) bitmapForRecycling	 [DUMP_YARD_BITMAP_ARRAY_SIZE];
	//}
#	else
	//{
		size_t 			writeFromOffset;
		size_t 			sharedMemoryFileSize;
	
		size_t 			bitmapForDumping	 [DUMP_YARD_BITMAP_ARRAY_SIZE];
		size_t 			bitmapForRecycling	 [DUMP_YARD_BITMAP_ARRAY_SIZE];
	//}
#	endif

//}

#else
//{

#	ifdef HAVE_STDATOMIC_H
	//{
		_Atomic(size_t) writeFromOffset;
		_Atomic(size_t)	sharedMemoryFileSize;
	//}
#	else
	//{
		size_t 			writeFromOffset;
		size_t 			sharedMemoryFileSize;
	//}
#	endif

//}
#endif
	
};



/**
 *
 *	This function allocates memory for a the Global(manager).
 *	It opens status file and shared memory file by calling
 *	openStatusFile() and openSharedMemoryFile().
 *	Those 2 functions set the manager's members to appropriate values.
 *	This function would return if Global(manager) is not NULL.
 *
 *	Arguments:
 *
 *	#Arg1(status_file_name):
 *		Name of the status file.
 *
 *	#Arg2(shared_memory_file_name):
 *		Name of the shared memory file
 *
 **/
bool __dtsharedmemory_set_manager(const char *status_file_name, const char *shared_memory_file_name);



/*
 *
 *	INode acts as an intermediary between parent and child CNodes.
 * 	It contains offset from the base to the main CNode.
 *
 *	To add a new child to any CNode, a copy of that node is created,
 *	and the changes are made to that copy and the parent node is made to
 *	point the updated copy.
 *
 *	Suppose there was no INode.
 *	Let the parent CNode be C1 and one of the child of C1 be C2.
 *	To add a new value to bitmap of a C1, a copy of it is created.
 * 	While the changes are being made to that copy, it is possible that
 *	some other thread had created a copy of C2 to update its bitmap.
 *	Parent C1 is made to reference updated C2. But the copy of C1 that
 *	was created before replaces the old one, the changes that were made
 *	in between won't get reflected to the update C1.
 *	This is why INode is necessary.
 *
 */
typedef struct INode{
	
#ifdef HAVE_STDATOMIC_H
	_Atomic(size_t) mainNode;
#else
	size_t mainNode;
#endif
	
}INode;


/*
 *	POSSIBLE_CHARACTERS specify the variety of characters a file name can have.
 *	Paths don't use ascii chars 0-31 and 128-256 generally, so no use of making such big nodes
 *	for rare cases. Bigger size of CNode means more time taken for insertion and also
 *	shared memory expands really fast.
 */
#define LOWER_LIMIT 32	//inclusive
#define UPPER_LIMIT 122	//inclusive
#define POSSIBLE_CHARACTERS (UPPER_LIMIT - LOWER_LIMIT + 1) //The array size


#if UPPER_LIMIT <= LOWER_LIMIT
#	error 	Invalid range of possible characters.\
	Reset UPPER_LIMIT and LOWER_LIMIT values.
#endif



/**
 *
 *	#Member1(possibilities):
 *		This array stores offsets to next INodes for every possibility.
 *
 *	#Member2(endOfString):
 *		Is set true when a node needs to represent end of string.
 *
 *	#Member3(flags):
 *		These tells charactersistics associated with the path that has been inserted.
 *
 **/
typedef struct CNode{
	
#if (LARGE_MEMORY_NEEDED && 1)
	size_t		possibilities	[POSSIBLE_CHARACTERS];
#else
	uint32_t 	possibilities	[POSSIBLE_CHARACTERS];
#endif
	
	bool 		isEndOfString;
	uint8_t		flags;
	
}CNode;



/*
 *	This is the minimum size of the shared memory file which is for sure required by
 *	it to function correctly
 */
#define ROOT_SIZE (sizeof(INode) + sizeof(CNode))



/**
 *	ALLOW_PATH
 *		Path should be allowed access.
 *
 *	DENY_PATH
 *		Path should be denied access.
 *
 *	SANDBOX_VIOLATION
 *		This is a path to a file that belongs to a foreign port.
 *		For logging purposes in darwintrace.c
 *
 *	SANDBOX_UNKNOWN
 *		This is a path to a file which is not known to macports.
 *		For logging purposes in darwintrace.c
 *
 *	IS_PREFIX
 *		Path being inserted is a prefix, i.e., all paths with
 *		this prefix are treated same way.
 *		e.g., If "/bin" is inserted with this flag,
 *		and then a search is made for "/bin/ls", the search will succeed
 *		and path characteristics of "/bin" will be returned.
 *		Also these are specifically path prefixes and won't work as a
 *		general prefix, like search for "/binabc" will fail.
 **/
enum
{
	ALLOW_PATH			= (uint8_t) 1 << 0,
	DENY_PATH			= (uint8_t) 1 << 1,
	SANDBOX_VIOLATION	= (uint8_t) 1 << 2,
	SANDBOX_UNKNOWN		= (uint8_t) 1 << 3,
	IS_PREFIX			= (uint8_t) 1 << 4
};



/**
 *
 *	This function inserts a string `path` along with `flags` into the shared memory.
 *	If insertion is successful, it returns true else false.
 *	The shared memory follows a ctrie data structure. Although it doesn't implement
 *	tomb nodes due to lack of need to remove nodes.
 *
 *	Arguments:
 *
 *	#Arg1(path):
 *		Path to be inserted into shared memory.
 *
 *	#Arg2(flags):
 *		Tell the characteristics of the path getting inserted.
 *
 *
 **/
bool __dtsharedmemory_insert(const char *path, uint8_t flags);




/**
 *
 *	This function searches for a string `path` in the shared memory.
 *	If found it returns true and sets the value of `flags`. Otherwise,
 *	it returns false.
 *	The shared memory follows a ctrie data structure. Although it doesn't implement
 *	tomb nodes due to lack of need to remove nodes.
 *
 *	Arguments:
 *
 *	#Arg1(path):
 *		Path to be searched in shared memory.
 *
 *	#Arg2(flags):
 *		Tells the characteristics of the path. This argument needs to be passed
 *		by address to get characteristcis associated with the path.
 *
 **/
bool __dtsharedmemory_search(const char *path, uint8_t *flags);



/**
 *
 *	While installing a port there is a chance that it may try to use
 *	the file descriptor that is being used by either status file or
 *	shared memory file via dup2(2). Each process has a unique file descriptor
 *	for each of these files which is used throughout the process.
 *	If while installing the port, it calls dup2(2) and tries to use this fd,
 *	this function will make the process to reset the fd that we are using and
 *	let it take our old fd.
 *
 **/
bool __dtsharedmemory_reset_fd();



/**
 *
 *	__dtsharedmemory_getStatusFileFd() returns status file fd and __dtsharedmemory_getSharedMemoryFileFd()
 *	returns shared memory file fd, if manager is not NULL.
 *	This provides a more faster way to check if dup2(2) or close(2)
 *	are trying to close this fd and if they are the call __dtsharedmemory_reset_fd().
 *
 **/
int __dtsharedmemory_getStatusFileFd();
int __dtsharedmemory_getSharedMemoryFileFd();



/**
 *
 *	Returns the offset from which a new node would be written,
 *	while simply is the real file size used.
 *
 **/
size_t __dtsharedmemory_getUsedSharedMemorySize();


#endif
