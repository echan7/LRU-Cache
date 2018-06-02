/* Name: Eric Chan
 * CS login: echan
 * Section: Lecture 2
 *
 *
 *
 * csim.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions.  The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *  1. Each load/store can cause at most one cache miss. (I examined the trace,
 *  the largest request I saw was for 8 bytes).
 *  2. Instruction loads (I) are ignored, since we are interested in evaluating
 *  trans.c in terms of its data cache performance.
 *  3. data modify (M) is treated as a load followed by a store to the same
 *  address. Hence, an M operation can result in two cache hits, or a miss and a
 *  hit plus an possible eviction.
 *
 * The function printSummary() is given to print output.
 * Please use this function to print the number of hits, misses and evictions.
 * This is crucial for the driver to evaluate your work. 
 */
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include<stdbool.h>

#include "cachelab.h"

// #define DEBUG_ON 
#define ADDRESS_LENGTH 64

/****************************************************************************/
/***** DO NOT MODIFY THESE VARIABLE NAMES ***********************************/

/* Globals set by command line args */
int verbosity = 0; /* print trace if set */
int s = 0; /* set index bits */
int b = 0; /* block offset bits */
int E = 0; /* associativity */
char* trace_file = NULL;

/* Derived from command line args */
int S; /* number of sets S = 2^s In C, you can use "pow" function*/
int B; /* block size (bytes) B = 2^b In C, you can use "pow" function*/

/* Counters used to record cache statistics */
int miss_count = 0;
int hit_count = 0;
int eviction_count = 0;

// Global variable to keep cont of the maximum count of all lines in a set
int maxCount = 0;
/*****************************************************************************/


/* Type: Memory address 
 * Use this type whenever dealing with addresses or address masks
  */
typedef unsigned long long int mem_addr_t;

/* Type: Cache line
 * TODO 
 * 
 * NOTE: 
 * You will need to add an extra field to this struct
 * depending on your implementation of the LRU policy for evicting a cache line
 * 
 * For example, to use a linked list based LRU,
 * you might want to have a field "struct cache_line * next" in the struct 
 */
typedef struct cache_line {
    char valid;
    mem_addr_t tag;
    //added counter to keep track of LRU
    int count;
} cache_line_t;

typedef cache_line_t* cache_set_t;
typedef cache_set_t* cache_t;


/* The cache we are simulating */
cache_t cache;  

/* TODO - COMPLETE THIS FUNCTION
 * initCache - 
 * Allocate data structures to hold info regrading the sets and cache lines
 * use struct "cache_line_t" here
 * Initialize valid and tag field with 0s.
 * use S (= 2^s) and E while allocating the data structures here
 */
void initCache()
{
  //get S size and B size by using 2 to the power s & b
  S = (int) pow(2,s);
  B = (int) pow(2,b);
  int setIndex;
  int lineIndex;
  int numSets = S;
  int numLines = E;

  //dynamically allocate the row of array
  cache =  (cache_set_t*) malloc(sizeof(cache_set_t) * numSets);
  //check library function return the right value
  if(cache == NULL){
	printf("Memory Not Allocated");
	exit(1);
  }
  //dynamically allocated memory for each line
  for (setIndex = 0; setIndex < numSets; setIndex++){
        cache[setIndex] = (cache_line_t*) malloc(sizeof(cache_line_t)*numLines);
	//also check whether library function return right value
	if(cache[setIndex] == NULL){
		printf("Memory Not Allocated");
		exit(1);
	}
	//initialize every member in the struct to 0
	for(lineIndex = 0; lineIndex < numLines; lineIndex++){
		  cache[setIndex][lineIndex].valid = 0;
		  cache[setIndex][lineIndex].tag = 0;
		  cache[setIndex][lineIndex].count = 0;
	}
  }
}


/* TODO - COMPLETE THIS FUNCTION 
 * freeCache - free each piece of memory you allocated using malloc 
 * inside initCache() function
 */
void freeCache()
{
 int setIndex;
 //free cache from the most recently allocated memory 
 for (setIndex = 0; setIndex < S; setIndex++){
	free(cache[setIndex]);
 //set pointer to null
	cache[setIndex] = NULL;
 }
 //free the outer cache
 free(cache);
 //set cache pointer to null
 cache = NULL;
}


/* TODO - COMPLETE THIS FUNCTION 
 * accessData - Access data at memory address addr.
 *   If it is already in cache, increase hit_count
 *   If it is not in cache, bring it in cache, increase miss count.
 *   Also increase eviction_count if a line is evicted.
 *   you will manipulate data structures allocated in initCache() here
 */
void accessData(mem_addr_t addr)
{
 //Get the tag size since we know it's 64 bit per block
 int tagSize = 64 - (s + b);
 /*extract tag wanted and set index by using mask, get the mask by using OxFF(x16) 
   until only s bits are all 1 and others all zero through simple right and left
   binary shift operators*/
 //get tag wanted by just shifting everything to the right until tag size remaining
 mem_addr_t tagBit = addr >> (s+b);
 unsigned long long sMask = 0xFFFFFFFFFFFFFF >> s;
 sMask = sMask << (tagSize + s);
 sMask = sMask >> tagSize;
 int sIndex = (addr & sMask) >> b; 
 
 int numLines = E;
 int found = 0;
 int miss = 0;
 //gain access to the set specify the mem addr 
 cache_set_t setToCheck = cache[sIndex];
 //iterate through every line int he set to check for equivalent tag
 for(int lineIndex = 0; lineIndex < numLines && !found && !miss; lineIndex++){
	//if found, check validity
	if(setToCheck[lineIndex].tag == tagBit){
		if(setToCheck[lineIndex].valid == 1){
	//if valid, increase hit count, stop loop and change the counter of the line
			hit_count++;
			setToCheck[lineIndex].count = ++maxCount;
			found = 1;
		}else{
	//otherwise stop teh loop and handle the miss below
			miss = 1;
		}
	}
 }

 if(!found){
 //if not found means miss, so increase miss count
	miss_count++;
 //get the min counter of the set by iterate thoguht the loop 
 	int minIndex = 0;
	for(int lineIndex = 0; lineIndex < numLines; lineIndex++){
		if(setToCheck[minIndex].count > setToCheck[lineIndex].count){
				minIndex = lineIndex;
			}
		}
	//if min couter is 0, means cache is not full so add the tag and change the 
	// counter and validity
	if(setToCheck[minIndex].count == 0){
		setToCheck[minIndex].valid = 1;
		setToCheck[minIndex].count = ++maxCount;
		setToCheck[minIndex].tag = tagBit;
	//otherwise,cache is full, increase eviction count and change count, tag and
	// validity
	}else{
		eviction_count++;
		setToCheck[minIndex].count = ++maxCount;
		setToCheck[minIndex].tag = tagBit;
		setToCheck[minIndex].valid = 1;
	}
 }
}


/* TODO - FILL IN THE MISSING CODE
 * replayTrace - replays the given trace file against the cache 
 * reads the input trace file line by line
 * extracts the type of each memory access : L/S/M
 * YOU MUST TRANSLATE one "L" as a load i.e. 1 memory access
 * YOU MUST TRANSLATE one "S" as a store i.e. 1 memory access
 * YOU MUST TRANSLATE one "M" as a load followed by a store i.e. 2 memory accesses 
 */
void replayTrace(char* trace_fn)
{
    char buf[1000];
    mem_addr_t addr=0;
    unsigned int len=0;
    FILE* trace_fp = fopen(trace_fn, "r");

    if(!trace_fp){
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);
    }

    while( fgets(buf, 1000, trace_fp) != NULL) {
        if(buf[1]=='S' || buf[1]=='L' || buf[1]=='M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);
      
            if(verbosity)
                printf("%c %llx,%u ", buf[1], addr, len);

           // TODO - MISSING CODE
           // now you have: 
           // 1. address accessed in variable - addr 
           // 2. type of acccess(S/L/M)  in variable - buf[1] 
           // call accessData function here depending on type of access
	   if(buf[1] == 'S'||buf[1] == 'L'){
		accessData(addr);
	   }else if(buf[1] == 'M'){
		accessData(addr);
		accessData(addr);
	   }	


            if (verbosity)
                printf("\n");
        }
    }

    fclose(trace_fp);
}

/*
 * printUsage - Print usage info
 */
void printUsage(char* argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}

/*
 * main - Main routine 
 */
int main(int argc, char* argv[])
{
    char c;
    
    // Parse the command line arguments: -h, -v, -s, -E, -b, -t 
    while( (c=getopt(argc,argv,"s:E:b:t:vh")) != -1){
        switch(c){
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            trace_file = optarg;
            break;
        case 'v':
            verbosity = 1;
            break;
        case 'h':
            printUsage(argv);
            exit(0);
        default:
            printUsage(argv);
            exit(1);
        }
    }

    /* Make sure that all required command line args were specified */
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        printUsage(argv);
        exit(1);
    }


    /* Initialize cache */
    initCache();

#ifdef DEBUG_ON
    printf("DEBUG: S:%u E:%u B:%u trace:%s\n", S, E, B, trace_file);
#endif
 
    replayTrace(trace_file);

    /* Free allocated memory */
    freeCache();

    /* Output the hit and miss statistics for the autograder */
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
