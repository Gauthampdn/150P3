#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

/* TODO: Phase 1 */

#define RDENTRYSIZE 32

struct SuperBlock {
	uint64_t signature;
	uint16_t totBlocks;
	uint16_t rootDirBlockIndex;
	uint16_t dataBStartIndex;
	uint16_t numDblocks;
	uint16_t numFATBs;
	uint8_t padding[BLOCK_SIZE-17];
};



struct __attribute__((packed)) FAT{
	uint16_t FATentry[BLOCK_SIZE/2];
	// since 16 bits per entry (2 bytes)
	// since each block is 4096 bytes, there has to be 4096/2 entries
};



struct __attribute__((packed)) RDentry {
	uint8_t filename[16];
	uint32_t fileSize;
	uint16_t firstDBIndex;
	uint8_t padding[RDENTRYSIZE-22];
};

struct __attribute__((packed)) rootDir{
	struct RDentry RDlist[FS_FILE_MAX_COUNT];
};


struct __attribute__((packed)) fileDesc{
	size_t location;
	size_t offset;
	uint8_t filename[16];
};


struct __attribute__((packed)) FDtable{
	struct fileDesc FDlist[32];
};


struct SuperBlock *supB;
struct FAT *fat;
struct rootDir *rDir;
struct FDtable *fdTable;

//GLOBAL VALUES

long unsigned int ECSHEX = 0x539B2F7DBE23A8E5;
int numMaxFATentries = BLOCK_SIZE/2; 
unsigned short int FAT_EOC = 0xFFFF;

//------------


int FreeFATs(void){
	int total = 0;

	for(int i = 0; i < supB->numFATBs-1; i++){ 
		for(int j = 0; j < numMaxFATentries; j++){
			if(fat[i].FATentry[j] == 0)
				total++;
		}
	}

	for(int j = 0; j < (supB->numDblocks-(numMaxFATentries*(supB->numFATBs-1))); j++){
		if(fat[supB->numFATBs-1].FATentry[j] == 0)
				total++;
	}

	return total;
}


int emptyRootNum(void){
	int total = 0;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(rDir->RDlist[i].filename[0] == '\0')
			total++;
	}
	return total;
}




int fs_mount(const char *diskname)
{
	if(block_disk_open(diskname) != 0){
		return -1;
	}
	supB = malloc(sizeof(struct SuperBlock));
	if(supB==NULL){
		fprintf(stderr,"error with malloc SuperBlock\n");
		return -1;	
	}
	block_read(0,supB);

	if(supB->signature == ECSHEX){
		fprintf(stderr,"signature is not ECS150FS\n");
		return -1;
	}
	if(supB->totBlocks!=block_disk_count()){
		fprintf(stderr,"total blocks don't match\n");
		return -1;
	}

	fat = malloc(supB->numFATBs * sizeof(struct FAT));  // making an array of FATS (1 to 4)
	if(fat==NULL){
		fprintf(stderr,"error with malloc fat\n");
		return -1;
	}

	rDir = malloc(sizeof(struct rootDir));  // making the root dir
	if(rDir==NULL){
		fprintf(stderr,"error with malloc rDir\n");
		return -1;
	}

	for(int i = 1; i <= supB->numFATBs ; i++){  // copying each FAT block to the proper FAT number
		block_read(i,&fat[i-1]);
	}

	block_read(supB->rootDirBlockIndex,rDir);  // copying over the root block dir
	
	
	
	fdTable = malloc(sizeof(struct FDtable)); // making the FD table
	
	if(fdTable==NULL){
		fprintf(stderr,"error with malloc fdTable\n");
		return -1;
	}
	return 0;
}



int fs_umount(void)
{
	if( block_disk_close() != 0 ){
		return -1;
	}

	free(supB);
	free(fat);
	free(rDir);
	free(fdTable);

	return 0;
}

int fs_info(void)
{
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", supB->totBlocks);
	printf("fat_blk_count=%d\n", supB->numFATBs);
	printf("rdir_blk=%d\n", supB->rootDirBlockIndex);
	printf("data_blk=%d\n", supB->dataBStartIndex);
	printf("data_blk_count=%d\n", supB->numDblocks);
	printf("fat_free_ratio=%d/%d\n",FreeFATs(), supB->numDblocks);
	printf("rdir_free_ratio=%d/%d\n",emptyRootNum(), BLOCK_SIZE/32);
	return 0;
}



int fs_create(const char *filename)
{
    if (supB == NULL ){
        return -1;
	}
	int fnamelen = strlen(filename);
    int fileCount = emptyRootNum();


	if (fnamelen + 1 > FS_FILENAME_LEN || filename[fnamelen] != '\0' || fileCount <= 0)
	{
	    return -1;
	}


	int freeRDentry = 0;

	while(freeRDentry < 128){ 
		if (strcmp((char *)rDir->RDlist[freeRDentry].filename, filename) == 0) // if file already exists
		{
			return -1;
		}
		if(rDir->RDlist[freeRDentry].filename[0] == '\0') // if found a null character filename
		{
			strncpy((char *)rDir->RDlist[freeRDentry].filename, filename, strlen(filename)+1);
			rDir->RDlist[freeRDentry].fileSize = 0;
			rDir->RDlist[freeRDentry].firstDBIndex = FAT_EOC;
			block_write(supB->rootDirBlockIndex, rDir);
			return 0;
		}
		freeRDentry++;
	}
    return -1;  // File creation failed
}




int fs_delete(const char *filename)
{
	printf("Deleting file: %s\n", filename);
	return 0;
}

int fs_ls(void)
{
	printf("Listing files\n");
	return 0;
}

int fs_open(const char *filename)
{
	printf("Opening file: %s\n", filename);
	return 0;
}

int fs_close(int fd)
{
	printf("Closing file descriptor: %d\n", fd);
	return 0;
}

int fs_stat(int fd)
{
	printf("Getting stats for file descriptor: %d\n", fd);
	return 0;
}

int fs_lseek(int fd, size_t offset)
{
	printf("Seeking file descriptor: %d to offset: %zu\n", fd, offset);
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	printf("Writing to file descriptor: %d, buffer: %p, count: %zu\n", fd, buf, count);
	return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	printf("Reading from file descriptor: %d, buffer: %p, count: %zu\n", fd, buf, count);
	return 0;
}
