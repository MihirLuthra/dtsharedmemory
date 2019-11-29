NOTE: This implementation doesn't do hashing. Its more like just a trie which works concurrently between multiple threads and 
processes and with no node deletion feature and is lock free.

A new implementation is being developed noting the shortcomings of this one.

https://github.com/MihirLuthra/dtsm_alloc + https://github.com/neverpanic/cctrie

# dtsharedmemory

This repository contains standalone shared memory cache data sturcture implementation with a test file which are made as per 
macports, [speed up trace mode project](https://trac.macports.org/wiki/SummerOfCode#tracemode).

The main implementation of this code exists in the [dtsm-darwintrace](https://github.com/MihirLuthra/macports-base/tree/dtsm-darwintrace) 
branch of my forked macports repositoy.

Speed comparisons for installation of ports have been recorded in a [spreedsheet](https://docs.google.com/spreadsheets/d/1ksj3Fex-AnTEU4f4IRzwUkTpN4XfUye-HqSdZwXOsKs/edit#gid=0).
Probably more testing is needed for speed comparisons.

<p>
The basic motive of the code is to construct a shared memory cache for the processes into which darwintrace library gets 
injected. If a process into which darwintrace library has been injected tries to access a path, it asks the server which 
queries registry to check if the path should be allowed or denied for access. It does not do caching of the checked data. 
That's why it will ask for same paths again and again if asked. Also, if some other process or thread has checked same path, 
there shouldn't be a need to make same check in the registry again. To remove these two shortcomings, a shared memory cache is 
constructed in between these processes to allow them to store checked path data in a shared space.

Adding to that, caching of data would reduce traffic at the server side of trace mode.

The readme is divided into two parts:
	
(A)[QUICK START (How to use)](https://github.com/MihirLuthra/dtsharedmemory#aquick-start):

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;1.[INITIALISATION](https://github.com/MihirLuthra/dtsharedmemory#initialisation)

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;2.[FLAGS](https://github.com/MihirLuthra/dtsharedmemory#flags)

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;3.[INSERTION](https://github.com/MihirLuthra/dtsharedmemory#insertion)
   
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;4.[SEARCH](https://github.com/MihirLuthra/dtsharedmemory#search)
   
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;5.[IMPORTANT POINTS](https://github.com/MihirLuthra/dtsharedmemory#important-points)
	
(B)[CODE EXPLANATION](https://github.com/MihirLuthra/dtsharedmemory#bcode-explanation)

(C)[SHORTCOMINGS](https://github.com/MihirLuthra/dtsharedmemory#cshortcomings)

The code takes help from [this](https://arxiv.org/abs/1709.06056?context=cs) research paper which explains algorithm involved 
similar to ctrie. Don't worry if you didn't go through the research paper. Even if you read the complete readme, you will understand
the working of code easily. Although basic knowledge of how a [trie](https://www.youtube.com/watch?v=-urNrIAQnNo) works is 
needed.

**_As we proceed the terms used may get confusing. Just give a read to the line even if not understood completely and proceed.
Going in this sequence, you will eventually land accross every term being explained in detail_**

Note: <br>
It is a general question that why do function names start with an underscore (as they are reserver for special use).
Functions in the code use `__dtsharedmemory_*()` prefix generally to give it a similar look as is done is 
`darwintrace.c` by using `__darwintrace_*()`. Also, it actually provides a nice clarity to which module of the code provides 
the function.

# (A)QUICK START:
 
# INITIALISATION
 
 For "every process" that wants to insert or search a path string from shared memory,
 a successful call to `__dtsharedmemory_set_manager()` is required. 
    
    char *status_file_name = "dtsm-status", *shared_memory_file_name = "dtsm";
    bool did_set_manager = __dtsharedmemory_set_manager(status_file_name, shared_memory_file_name);
    
    if(!did_set_manager)
    {
        //Couldn't set shared memory manager
        //abort
    }
    
# FLAGS

There are 5 flags that can be passed to `__dtsharedmemory_insert()` function which are meant to denote characteristics of 
the path getting inserted.
These are:


**ALLOW_PATH**

  Insert paths that should be allowed with this flag and check it while searching.

**DENY_PATH**

  Insert paths that should be denied with this flag and check it while searching.

**IS_PREFIX**

  This tells to treat the path getting inserted as a prefix. All paths getting searched which have this prefix
  will be treated the as if the search was made for the prefix. Note that prefix here means common parent directory.
  Like inserting `/tmp` means all subdirectories or files within `/tmp` should be treated same way as `/tmp`.

**SANDBOX_VIOLATION** and **SANDBOX_UNKNOWN**

  Insert paths that couldn't be logged because `DT_REPORT` in `darwintrace.c` wasn't specified in darwintrace flags.
  For example, `_dt_getdirentries()` doesn't pass `DT_REPORT` to `__darwintrace_is_in_sandbox()`. So it couldn't be logged
  but it got inserted in shared memory. Now if some other system call, like `dt_mkdir()`, finds this unreported the path in
  shared memory cache, it won't do logging except of the fact that `dt_mkdir()` does specify `DT_REPORT`. So to solve this,
  unreported paths are supplied with `SANDBOX_VIOLATION` or `SANDBOX_UNKNOWN` flag depending on the requirement.
    
# INSERTION
 
    const char *path = "/usr/local";
    bool did_insert;
    
    did_insert = __dtsharedmemory_insert(path, DENY_PATH | IS_PREFIX);
    
    if(!did_insert)
    {
        //Insertion failed
    }
    else
    {
        //Successfully inserted path
    }
    
# SEARCH:
 
    const char *path = "/usr/local";
    uint8_t dtsm_flags;
    bool found_in_dtsm;
    
    found_in_dtsm = __dtsharedmemory_search(path, &dtsm_flags);
    
    if(!found_in_dtsm)
    {
        //Path doesn't exist in shared memory
    }
    else
    {
        //Path exists in shared memory
        
        if(dtsm_flags & ALLOW_PATH)
        {
            //path access allowed
        }
        else
        {
            //path access denied
        }
    }
    
# IMPORTANT POINTS:

  1)If the prgram using the library is gonna use more than 4 GB memory, `LARGE_MEMORY_NEEDED` should be set to `1` in
  	`dtsharedmemory.h`. This will allow more memory usage but will consume double the amount of memory it did before.
	Also setting it to `1` would certainly make insertion and searches take more time. Until not really needed, avoid 
	setting this.
	
  2)To print debug or failure messages, set `DEBUG_MESSAGES_ALLOWED` to `1` in `dtsharedmemory.h`.

	
    
# (B)CODE EXPLANATION

The basic motive of the code is to construct a shared memory cache for the processes into which darwintrace library gets 
injected. If a process into which darwintrace lib has been injected tries to access a path, it queries registry to check if 
the path should be allowed or denied for access. It does not do caching of the checked data. That's why it will query same 
paths again and again if asked. Also, if some other process or thread has checked same path, it shouldn't need to make same 
check in the registry again. To remove these two shortcomings, a shared memory cache is constructed in between these processes 
to allow them to store checked path data in a shared space.

This shared memory is simply a file.
Note that its best to construct the name for the shared memory file and its status file by `mktemp(3)`.
In macports-base, it is done in `porttrace.tcl` by `mktemp` and in `test_dtsharedmemory.c` it is done by `mktemp(3)`.

When a process or thread has checked a path, it will insert it in this file along with its permission(and other data) in the 
shared memory. Everytime before a process is going to check registry, it will first check shared memory cache.
The data structure used to store the paths in dtsm is [ctrie](https://en.wikipedia.org/wiki/Ctrie) with some modifications without hashing.

In order to make the access to shared memory file fast, they are `mmap(2)`'d in each process that wants to access it.
But to `mmap(2)` the file, it needs to have a size beforehand, which is defined in `dtsharedmemory.h` as `INITIAL_FILE_SIZE`.
At first when <br>
`__dtsharedmemory_set_manager(const char *status_file_name, const char *shared_memory_file_name)` 
is called, it
creates and adjusts file size. Truncation of file size works on the principle that the `INITIAL_FILE_SIZE`
should be big enough that even if 2 or more processes or threads end up calling `truncate(2)`, the file doesn't shrink and no 
data loss occurs. 

As we are reading and writing to a memory mapping, instead of pointers, we need to access nodes as <br>
`*(base + offset)` 
where `base` is returned from `mmap(2)` call and instead of using pointer to next nodes we add `offset` from the `base`. <br>
`bool __dtsharedmemory_set_manager(const char *status_file_name, const char *shared_memory_file_name)` sets the global 
variable named `manager` which is of type `struct SharedMemoryManager` which is defined in `dtsharedmemory.h` as:

	struct SharedMemoryManager{

		//shared memory file
		void *		sharedMemoryFile_mmap_base;
		size_t  	sharedMemoryFile_mapping_size;
		const char * 	sharedMemoryFile_name;
		int 		sharedMemoryFile_fd;
	
		//status file
		struct	SharedMemoryStatus *	statusFile_mmap_base;
		int				statusFile_fd;
		const char *			statusFile_name;
	
	};

`__dtsharedmemory_set_manager()` would allocate memory for a `struct SharedMemoryManager` data type and pass it to two
functions, in order, which are <br>
`bool openStatusFile(struct SharedMemoryManager *new_manager, const char *status_file_name)` 
and <br>
`bool openSharedMemoryFile(struct SharedMemoryManager *new_manager, const char *shared_memory_file_name)`.

These two functions will initialise all members of the new `struct SharedMemoryManager` variable with valid values.

`openStatusFile()` sets initial values in status file upon which `openSharedMemoryFile()` depends.
Status file contains meta data about shared memory file like file size, from where to write new data & wasted 
memory which is available for reuse.

Status file stores a single `struct SharedMemoryStatus` object which is defined as:

	struct SharedMemoryStatus
	{	
		_Atomic(size_t) writeFromOffset;
		_Atomic(size_t) sharedMemoryFileSize;
	
		size_t 		wastedMemoryDumpYard 	[DUMP_YARD_SIZE];
		size_t		parentINodesOfDumper 	[DUMP_YARD_SIZE];
		_Atomic(size_t) bitmapForDumping	[DUMP_YARD_BITMAP_ARRAY_SIZE];
		_Atomic(size_t) bitmapForRecycling	[DUMP_YARD_BITMAP_ARRAY_SIZE];
	};

`writeFromOffset` is the peak offset. If more space is needed, this variable is shifted ahead depending upon the amount of
space needed.

`sharedMemoryFileSize` holds file size of the shared memory file.

Other 4 are for the purpose of recycling wasted memory. That we will discuss later.

After new `struct SharedMemoryManager` variable is completely initialised, atomic `CAS` is performed on a global variable 
defined as:

	static 	_Atomic(struct SharedMemoryManager *) 	manager = NULL;
	
Iff `manager` is found `NULL`, `CAS` is performed, otherwise it is assumed that some other thread or parent process already did 
setting up of global manager.

Now as we are done with `__dtsharedmemory_set_manager()`, we can call <br>
`bool __dtsharedmemory_insert(const char *path, uint8_t flags)` or <br>
`bool __dtsharedmemory_search(const char *path, uint8_t *flags)`.

Lets say first we insert a string, `"/usr/local"`

	const char *path = "/usr/local";
	bool did_insert;
	
	did_insert = __dtsharedmemory_insert(path, DENY_PATH | IS_PREFIX);
	
	if(!did_insert)
	{
		//insertion successful
	}
	else
	{
		//insertion unsuccessful
	}
	
The code in `__dtsharedmemory_insert()` would start from root node which is already setup in `openSharedMemoryFile()`.
We iterate through each character of input string and for each character we check the array entries of that node. If the
array contains entry for that node (i.e., its not `0`), we proceed to next node. Otherwise we create entry for that index and 
proceed to next node.

The memory which we have to access is in range <br>
`manager->sharedMemoryFile_mmap_base` <br>
to <br>
`manager->sharedMemoryFile_mmap_base + manager->sharedMemoryFile_mapping_size`. <br>
If need for more space arises,
it can be expanded by a call to `expandSharedMemory(size_t offset)`.
For purpose of accessing offsets in the shared memory, a macro `GOTO_OFFSET(offset)` is used which evaluates to <br>
`manager->sharedMemoryFile_mmap_base + offset`. 
Before this evaluation, it makes checks if memory expansion is needed and if it is needed, it replaces global `manager` 
with a new one that contains a new `mmap(2)` base satisfying the space needed.

If seeing code along with reading, ignore `GUARD_CNODE_ACCESS()`. It mainly plays role in dumping and recycling.
For now just consider it as a code that evaluates <br>
`currentCNode = GOTO_OFFSET(currentINode->mainNode)` and
then makes the `CNode` member access.

We begin from offset `0`, which is the root `INode`. From root `INode` we proceed to its child `CNode` by <br>
`root->mainNode`.
We then fetch the first character from the input string and check the array of current `CNode` to see if
the entry for the character exists or not. If it exists, we simply move to the child `INode` of current `CNode` as <br>
`currentCNode->possibilities[character]`. <br>
If it doesn't exist, we create a new copy of current `CNode` so that we can make 
entry of this character in the array. In order to do so, we either recycle wasted memory(discussed later) or reserve space in 
shared memory for writing a new `CNode`. To get space in the shared memory, we call a function <br>
`bool reserveSpaceInSharedMemory(size_t bytesToBeReserverd, size_t *reservedOffset)` which shifts the `writeFromOffset` in 
status file ahead by `bytesToBeReserverd` and the old value of `writeFromOffset` is `reservedOffset` now. This portion of
shared memory belongs to the caller thread now and is not a critical section. We then call <br>
`bool createUpdatedCNodeCopy(CNode *copy, CNode cNodeToBeCopied, int index, bool updated_isEndOfString, uint8_t updated_flags)`
which creates the `CNode` copy with updated array entries and creates a child (a new `INode` + `CNode`) for new array entry.
Now on the parent `INode`, atomic `CAS` is performed to change it to the newly created copy. The old `CNode` is now 
wasted and this offset is dumped for reuse by calling 
`dumpWastedMemory()`. We will discuss about dumping and recycling in the end.

This process will continue until the end of the string.

Now to go deeper into this, firstly lets see the nodes we are using for the data structure.

Just like in ctrie, here also we have an `INode` and a `CNode`. We don't need to implement `SNode` as we don't need deletion 
of nodes in our case. As mentioned before, here as we are dealing with a memory mapping of a file, so we use 
offsets instead on pointer to next nodes. Data type we use for offsets is `size_t`.


	typedef struct INode{
		_Atomic(size_t) mainNode;
	}INode;

<!-- -->

	typedef struct CNode{
	
		size_t	possibilities	[POSSIBLE_CHARACTERS];
		bool 	isEndOfString;
		uint8_t	flags;
	
	}CNode;

where `POSSIBLE_CHARACTERS` can be set to any number between 0-255.
`flags` have been explained completely in the `(A)QUICK START` section of this readme.

`flags` are defined in `dtsharedmemory.h` as:

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
		ALLOW_PATH        = (uint8_t) 1 << 0,
		DENY_PATH         = (uint8_t) 1 << 1,
		SANDBOX_VIOLATION = (uint8_t) 1 << 2,
		SANDBOX_UNKNOWN	  = (uint8_t) 1 << 3,
		IS_PREFIX         = (uint8_t) 1 << 4
	};


Like in a basic linked list when you want to tell nothing exists ahead, you set next node to `NULL`. In a trie, 
the index which doesn't have a subtree, it is set to `NULL`. 
Here we set `possibilities[x]` at that particular index as `0` (which _is_ unique because root `INode` exists at `0`).
That should have increased the iterations as we need to set every index to 0 for each node. But that's not the case. 
When we expanded size of file by `truncate(2)`, it already filled the file that as it expanded with `'\0'`. This removes the
need to set anything to `0`. Everything is already set to `0`.

Ctrie being an extension of trie, has almost the same attributes. If our implementation was a normal trie, it would just
have `CNode`'s. In a ctrie, `INode` are also needed.

`INode` acts as an intermediary between parent and child `CNode`. It contains offset to the main `CNode`.
To add a new child to any `CNode`, a copy of that `CNode` is created, and the changes are made to that copy 
and the parent node is made to point the updated copy.

To understand this, suppose there was no `INode`.

Let the parent `CNode` be `C1` and one of the child of `C1` be `C2`. To add a new value to array of a `C1`, 
a copy of it is created. While the changes are being made to that copy, it is possible that some other thread had 
created a copy of `C2` to update its array. Parent `C1` is made to reference updated `C2`. But the copy of `C1` that
was created before replaces the old one, the changes that were made in between won't get reflected to the update `C1`.
This is why `INode` is necessary.

So now as concept of `INode` is clear, further we iterate through loop in the similar fashion as we do in [trie](https://www.geeksforgeeks.org/trie-insert-and-search/).

Go through the research paper about [ctrie](https://arxiv.org/abs/1709.06056?context=cs) to get a deeper understanding about
this.

To search a string we inserted, we simply need to call 

`bool __dtsharedmemory_search(const char *path, uint8_t *flags)`.
<!-- -->

    const char *path = "/usr/local";
    uint8_t dtsm_flags;
    bool found_in_dtsm;
    
    found_in_dtsm = __dtsharedmemory_search(path, &dtsm_flags);
    
    if(!found_in_dtsm)
    {
        //Path doesn't exist in shared memory
    }
    else
    {
        //Path exists in shared memory
        
        if(dtsm_flags & ALLOW_PATH)
        {
            //path access allowed
        }
        else
        {
            //path access denied
        }
    }
    
The procedure in `__dtsharedmemory_search()` goes same way as `__dtsharedmemory_insert()`.
We go down the tree for every iteration through the input string and if some character is not found in the node's
array, `false` is retuned indicating non-existence of the string. If the iteration reaches end of string,
it checks if that character marks end of the string in the tree and if it does, should it be allowed or denied.
After this it returns accordingly.

This completes the basic functionality of our code.

Further we need to discuss about `bool expandSharedMemory(size_t offset)`, <br>
`bool dumpWastedMemory(size_t wastedOffset, size_t parentINode)` & <br>
`bool recycleWastedMemory(size_t *reusableOffset, size_t parentINode)`.

`expandSharedMemory()` is always called in an abstract way. We only call this function through `GOTO_OFFSET(offset)`.

Basic job of `GOTO_OFFSET(offset)` is just to give the location, `mmap(2)` base + the offset. It also makes a check if
offset is greater than size of mapping and if it is, it calls `expandSharedMemory()`.

`expandSharedMemory()` would expand the size of the shared memory file by `truncate(2)` and then call `mmap(2)` again on
the same file with new size. Then it makes a new `struct SharedMemoryManager` variable. It then performs `CAS` on the global
`manager` with new variable.

`expandSharedMemory()` can be called from multiple threads or processes. To avoid shrinking of file when `truncate(2)` is
called, we keep the `EXPANDING_SIZE`, i.e. the size that is added to current size, big enough so that even if multiple
threads or processes make a call, the file never shrinks.	

Next we discuss about dumping and recycling of wasted memory.

As seen in insertion, to add a new entry to a bitmap, we create copy of the current `CNode`, make changes in that copy and
`CAS` the existing `CNode` by the updated copy. In this process, we end up wasting memory we reserved for the old `CNode`.
Due to this, more than half of the memory in our file is full with wasted nodes.

In order to remove this wastage, we reuse this memory. To achieve this we store offsets to the wasted memory
blocks in the status file in the array `wastedMemoryDumpYard[DUMP_YARD_SIZE]`.

The simple logic is that whenever we abandon a `CNode`, we store offset to it in this array. Whenever some other insertion
is being made, before calling `reserveSpaceInSharedMemory()`, the thread calls <br>
`bool recycleWastedMemory(size_t *reusableOffset, size_t parentINode)`. If it finds any available offset in the array it uses 
it instead of reserving more.

As this array will be accessed by multiple processes and threads, we simply just can't access the array normally.
To make access to this array thread safe, we add 2 new members to the status file, which are:

	_Atomic(size_t) bitmapForDumping	 [DUMP_YARD_BITMAP_ARRAY_SIZE];
	_Atomic(size_t) bitmapForRecycling	 [DUMP_YARD_BITMAP_ARRAY_SIZE];

This is a bitmap to keep a record about the entries in the array.

When putting an offset in the array, the thread first searches if any bit in `bitmapForDumping` is `0`. If it finds one,
it makes a copy of the existsing bitmap and updates it to set the index it found as `0` to `1`. Then it performs `CAS` on
`bitmapForDumping`. This tells other threads that this index is already reserved and some other thread is already
dumping the wasted offset at that index. After this it assigns the wasted offset as <br>
`wastedMemoryDumpYard[index] = wastedOffset`.
Then it updates the entry for the particular index in `bitmapForRecycling`. 
When some thread calls `recycleWastedMemory()`, it checks `bitmapForRecycling` and if it finds any entry to be `1`,
it tries to make it `0` in similar way as for `bitmapForDumping`. It then takes the value at that index from the array.
Now it sets the `bitmapForDumping` for that index back to `0` so that particular index becomes free for dumping again.

This way memory usage drops to half.
Also the speed of insertions and searches also increases 
(which i am not sure why but probably it is because less pages are being used so page caching becomes better).

Dumping and recycling gives rise to two new issues. To solve those we introduce three new concepts to the program.

1) **Common parent problem**

This is a sick race condition among sibling nodes. See `__dtsharedmemory_insert()` to get an idea of the situation.
Assume 3 threads, each dealing with a different `INode` with same parent `CNode`. 
(1st thread)1st one prepared its old offset to child `CNode`, and is now preparing new offset to get ready for `CAS`. 
Meanwhile (2nd thread)2nd `INode` `CAS`'d the old offset to point to a new
child `CNode` and it dumped the offset to old child `CNode` so that it can be recycled and the offset to new child `CNode`
contains updated array entries. Meanwhile the (3rd thread)3rd node recycled the offset dumped by the 2nd `INode` and `CAS`'d 
it to be child of current parent.
Array entries has got updations from 2nd and 3rd child `INode`. Now when 1st node will attempt to `CAS`, it will find the 
oldValue and the actual value to be equal, because 3rd node reused old offset, but the new value 1st node replaces don't 
contain array entries set by 2nd and 3rd node leading to data loss.
For this reason an `INode` can _not_ use a recycled offsets dumped by its sibling to make upgradations.

To solve this, we also pass offset to the parent `CNode` as an argument to `dumpWastedMemory()` and `recycleWastedMemory()` so that if parent are equal, deny recycling. 

Although this doesn't solve the complete problem. There can be a case when an offset was dumped, it was recycled and then
again dumped. The second time offset is dumped, it has a new parent. This causes it to lose data about its previous parent.
Due to this, common parent problem may occur again.
To solve this, we deny dumping of same offset twice. To achieve that, whenever `reserveSpaceInSharedMemory()` is called
it always reserves 2(defined as `PADDING_BYTES`) bytes extra. Now whenever an offset is dumped, `+1` is added to make it odd.
So once an offset has been dumped it shifts one byte ahead and becomes odd. So if the same offset is being dumped the
second time, +1 would be added to it and it will become even. When this happens, we reject that offset from being dumped.

2. **GUARD_CNODE_ACCESS(statement)**

The time when an offset to `CNode` is being accessed, its possible that it has already
been replaced with a new `CNode`. When dumping and recycling is disabled, this wasted offset won't be dumped for reuse and 
hence can be accessed even after being replaced. But when it has been dumped, its possible that it gets recycled and
used up to contain differnt bitmap entries before this access was made. This is why after the access we need to ensure
in a loop that parent still points to same child. Doing it with `GUARD_CNODE_ACCESS()` macro makes the code more readable
and simple.


# (C)SHORTCOMINGS:

Shortcomings that surely exist are specified with VERIFIED and the ones that need more testing are specified as NOT VERIFIED.<br>
A tick(✔️) is placed if the problem is solved and a cross(❌) is placed if the problem still exists.


 ❌1.Time taking `__dtsharedmemory_set_manager()` calls (NOT VERIFIED)

   `__dtsharedmemory_set_manager()` is a time taking function. This highly effects the installation time of port.
   The best solution to reduce time taken by this function should have been to not use a separate file for status file,
   rather move data of status file in the beginning of the shared memory file, hence reducing calls to an extra `open(2)`,       
   `stat(2)` and `mmap(2)`. This has been attempted by me but it didn't make any difference. 

 ✔️2.Unecessary calls to `__darwintrace_setup()` (VERIFIED)

   It would be better if `__darwintrace_setup()` only gets done when path is not found in shared memory.
   SOLVED! and this saves a lot of time.

 ❌3.Unecessary calls to `__darwintrace_setup_tls()` (VERIFIED)

   Similarly, as in point 2, thread local storage should only get setup when needed.

 ❌4. Omitting shared memory usage in phases that don't show improvement (**NOT VERIFIED**)
 
   The shared memory may not be much useful in phases where a lot of processes(that don't have parent child relation) 
   are involve and registry calls are really less. This would just increase calls to time taking 
   `__dtsharedmemory_set_manager()`. `__darwintrace_setup()` takes much less time than `__dtsharedmemory_set_manager()` but 
   gets called in every thread instead.
   So, it would be better to omit shared memory usage from phases which satisfy these conditions.
      
</p>
