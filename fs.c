#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define RDENTRYSIZE 32
#define FAT_EOC 0xFFFF


struct __attribute__((packed)) SuperBlock {
	uint64_t signature;
	uint16_t totBlocks;
	uint16_t rootDirBlockIndex;
	uint16_t dataBStartIndex;
	uint16_t numDblocks;
	uint16_t numFATBs;
	uint8_t padding[BLOCK_SIZE-17];
};

struct __attribute__((packed)) RDentry {
	uint8_t filename[FS_FILENAME_LEN];
	uint32_t fileSize;
	uint16_t firstDBIndex;
	uint8_t padding[RDENTRYSIZE-22];
};

struct __attribute__((packed)) fileDesc{
	size_t offset;
	int placeInRD;
};


struct SuperBlock supB;
struct RDentry rDir[FS_FILE_MAX_COUNT];
struct fileDesc fdTable[FS_OPEN_MAX_COUNT];

uint16_t *FAT;	// since 16 bits per entry (2 bytes)

bool mounted = false;



int FreeFATs(void){
	int total = 0;
	for(int i=0; i < supB.numDblocks; i++){
		if(FAT[i] == 0){
			total++;
		}
	}
	return total;
}

int emptyRootNum(void){
	int total = 0;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(rDir[i].filename[0] == '\0')
			total++;
	}
	return total;
}



bool IsFilenameValid(const char *filename){
	int fnamelen = strlen(filename);
	return filename != NULL && fnamelen + 1 <= FS_FILENAME_LEN && filename[fnamelen] == '\0';
}



int fs_mount(const char *diskname) {
	if(block_disk_open(diskname) != 0){
		return -1;
	}

	bool supBvalid = block_read(0, &supB) == 0 && \
					 strcmp((char *)supB.signature, "ECS150FS") == 0 && \
					 supB.totBlocks == block_disk_count();
					 // need more checks 

	if(!supBvalid){
		block_disk_close();
		fprintf(stderr,"Failure reading superblock\n");
		return -1;	
	}

	FAT = (uint16_t *)malloc(supB.numFATBs * BLOCK_SIZE);  // making an array of FATS 
	
	if(FAT == NULL){
		block_disk_close();
		fprintf(stderr,"error with malloc fat\n");
		return -1;
	}

	bool fatCopySuccess = true;
	for(int i = 0; i < supB.numFATBs ; i++){  // copying each FAT block to the proper FAT number
		if (-1 == block_read(i+1, &FAT[i*BLOCK_SIZE/2])) {
			fatCopySuccess = false;
		}
	}
	if (!fatCopySuccess){
		free(FAT);
		block_disk_close();
		fprintf(stderr,"error with reading fat\n");
		return -1;
	}

	if(-1 == block_read(supB.rootDirBlockIndex, rDir)){  // copying over the root block dir
		free(FAT);
		block_disk_close();
		fprintf(stderr,"error with reading root directory\n");
		return -1;
	}

	for(int i=0; i<FS_OPEN_MAX_COUNT; i++){
		fdTable[i].placeInRD = -1;
	}
	
	mounted = true;
	return 0;
}



int fs_umount(void)
{	
	mounted = false;

	for(int i = 0; i < supB.numFATBs ; i++){ 
		block_write(i+1, &FAT[i*BLOCK_SIZE/2]);
	}
	free(FAT);

	block_write(supB.rootDirBlockIndex, rDir);

	if( block_disk_close() != 0 ){
		return -1;
	}
	
	return 0;
}


int fs_info(void)
{
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", supB.totBlocks);
	printf("fat_blk_count=%d\n", supB.numFATBs);
	printf("rdir_blk=%d\n", supB.rootDirBlockIndex);
	printf("data_blk=%d\n", supB.dataBStartIndex);
	printf("data_blk_count=%d\n", supB.numDblocks);
	printf("fat_free_ratio=%d/%d\n",FreeFATs(), supB.numDblocks);
	printf("rdir_free_ratio=%d/%d\n",emptyRootNum(), BLOCK_SIZE/32);
	return 0;
}



int fs_create(const char *filename)
{
	if(!mounted || !IsFilenameValid(filename)){
		return -1;
	}

	int freeRDentry = -1;

	for(int i=0; i<FS_FILE_MAX_COUNT; i++){ 
		if(rDir[i].filename[0] == '\0'){
			if(freeRDentry == -1){
				freeRDentry = i;
			}
		} else if(strcmp((char *)rDir[freeRDentry].filename, filename) == 0) { // if file already exists
			return -1;
		} 
	}

	if(freeRDentry == -1) {
		return -1;
	}

	strncpy((char *)rDir[freeRDentry].filename, filename, strlen(filename)+1);
	rDir[freeRDentry].fileSize = 0;
	rDir[freeRDentry].firstDBIndex = FAT_EOC;
	block_write(supB.rootDirBlockIndex, rDir);
	return 0;
}



int fs_delete(const char *filename)
{
	printf("%s\n", filename);
	return 0;
}

int fs_ls(void)
{
	
	return 0;
}



int fs_open(const char *filename)
{
	if(!mounted || !IsFilenameValid(filename)){
		return -1;
	}

	// Find the first open FD in table
	int fd = 0;
	while(fd<FS_OPEN_MAX_COUNT && fdTable[fd].placeInRD >= 0){
		++fd;
	}
	if( fd >= FS_OPEN_MAX_COUNT) {	// there are already FS_OPEN_MAX_COUNT files currently open
		return -1;
	}

	// Looking for filename in root directory
	int rDirIndex = 0;
	while(rDirIndex<FS_FILE_MAX_COUNT && strcmp((char *)rDir[rDirIndex].filename, filename) != 0){
		++rDirIndex;
	}

	if(rDirIndex >= FS_FILE_MAX_COUNT){	// File not found in root directory
		return -1;
	}

	// Store the file descriptor
    fdTable[fd].offset = 0;
	fdTable[fd].placeInRD = rDirIndex;
    return fd;	// return the file descriptor (index in array)
}


bool isFDValid(int fd) {
	return fd >= 0 && fd < FS_OPEN_MAX_COUNT && fdTable[fd].placeInRD >= 0;
}


int fs_close(int fd)
{
	if(!mounted || !isFDValid(fd)){
		return -1;
	}
	fdTable[fd].placeInRD = -1;
	return 0;
}


int fs_stat(int fd)
{
	if(!mounted || !isFDValid(fd)){
		return -1;
	}
	int index = fdTable[fd].placeInRD;
	return rDir[index].fileSize;
}


int fs_lseek(int fd, size_t offset)
{
	if(!mounted || !isFDValid(fd) || offset >= rDir[fdTable[fd].placeInRD].fileSize) {
		return -1;
	}
	fdTable[fd].offset = offset;
	return 0;
}



int fs_write(int fd, void *buf, size_t count)
{
	
	return 0;
}


int fs_read(int fd, void *buf, size_t count)
{
    if(!mounted || !isFDValid(fd)){
		return -1;
	}

	size_t offset = fdTable[fd].offset;
	size_t startingDBInd = rDir[fdTable[fd].placeInRD].firstDBIndex;

	//minimum of count and whats left of file 
	//partial blocks need to go into temp buffers and then do memcpy
}

