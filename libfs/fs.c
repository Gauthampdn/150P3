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

int NumOfFreeFATs(void){
	int total = 0;
	for(int i=0; i < supB.numDblocks; i++){
		if(FAT[i] == 0){
			total++;
		}
	}
	return total;
}

uint16_t allocateNextFAT(uint16_t curr_DB){
	uint16_t i = -1;
	do{
		i++;
	}
	while(i < supB.numDblocks && FAT[i] != 0);

	if(i < supB.numDblocks){
		FAT[curr_DB] = i;
		FAT[i] = FAT_EOC;
		return i;
	}
	return FAT_EOC;
}

int NumOfFreeRootEntries(void){
	int total = 0;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(rDir[i].filename[0] == '\0')
			total++;
	}
	return total;
}

bool IsFilenameValid(const char *filename){
	int fnamelen = strlen(filename);
	return filename != NULL && fnamelen < FS_FILENAME_LEN && filename[fnamelen] == '\0';
}

bool IsvalidSignature(){
	char buf[9];
	memcpy(buf, &supB.signature, 8);
	buf[8] = '\0';
	return strcmp(buf, "ECS150FS") == 0;
}

int fs_mount(const char *diskname) {
	if(block_disk_open(diskname) != 0){
		return -1;
	}
 
	bool supBvalid = block_read(0, &supB) == 0 && 
	 				 IsvalidSignature() && 
	 				 supB.totBlocks == block_disk_count() &&
					 supB.dataBStartIndex == supB.rootDirBlockIndex + 1 &&
					 supB.dataBStartIndex + supB.numDblocks == supB.totBlocks;

	if(!supBvalid){
		block_disk_close();
		return -1;	
	}

	FAT = (uint16_t *)malloc(supB.numFATBs * BLOCK_SIZE);  // making an array of FATS 
	
	if(FAT == NULL){
		block_disk_close();
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
		return -1;
	}
		
	if(-1 == block_read(supB.rootDirBlockIndex, rDir)){  // copying over the root block dir
		free(FAT);
		block_disk_close();
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
	if(!mounted){
		return -1;
	}
	for(int i=0; i<FS_OPEN_MAX_COUNT; i++){
		if(fdTable[i].placeInRD != -1){	// there are still open file descriptors
			return -1;
		}
	}

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
	printf("fat_free_ratio=%d/%d\n",NumOfFreeFATs(), supB.numDblocks);
	printf("rdir_free_ratio=%d/%d\n",NumOfFreeRootEntries(), BLOCK_SIZE/32);
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
		} else if(strcmp((char *)rDir[i].filename, filename) == 0) { // if file already exists
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
	if(!mounted || !IsFilenameValid(filename)){
		return -1;
	}

	int RDindex = 0;
	while(RDindex < FS_FILE_MAX_COUNT && strcmp((char *)rDir[RDindex].filename, filename) != 0){
		++RDindex;
	}
	if(RDindex >= FS_FILE_MAX_COUNT){
		return -1;
	}

	for(int i=0; i<FS_OPEN_MAX_COUNT; i++){
		if(fdTable[i].placeInRD == RDindex){
			return -1;
		}
	}

	if(rDir[RDindex].firstDBIndex != FAT_EOC){
		uint16_t index = rDir[RDindex].firstDBIndex + supB.dataBStartIndex;
		uint16_t next_index = FAT[index];
		while(next_index != FAT_EOC){
			FAT[index] = 0;
			index = next_index;
			next_index = FAT[index];
		}
		FAT[index] = 0;
	}

	rDir[RDindex].filename[0] = '\0';
	block_write(supB.rootDirBlockIndex, rDir);

	return 0;
}

int fs_ls(void)
{
	if(!mounted){
		return -1;
	}
	printf("FS Ls:\n");
	for(int i=0; i<FS_FILE_MAX_COUNT; i++){
		if(rDir[i].filename[0] != '\0'){
			printf("file: %s, size: %d, data_blk: %d\n", rDir[i].filename, rDir[i].fileSize, rDir[i].firstDBIndex);
		}
	}
	return 0;
}


// phase 3

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
	if(!mounted || !isFDValid(fd) || offset > rDir[fdTable[fd].placeInRD].fileSize) {
		return -1;
	}
	fdTable[fd].offset = offset;
	return 0;
}


// phase 4

uint16_t getNextBlock(uint16_t currBlock, bool write){
	if(currBlock == FAT_EOC){
		return FAT_EOC;
	}
	uint16_t next = FAT[currBlock];
	if(write && next == FAT_EOC){
		next = allocateNextFAT(currBlock);
	}
	return next;
}

uint16_t findCurrBlock(size_t offset, uint16_t block, size_t *relativeOffset){
	*relativeOffset = offset;
	uint16_t curr_block = block;
	while(*relativeOffset >= BLOCK_SIZE){
		*relativeOffset -= BLOCK_SIZE;
		curr_block = getNextBlock(curr_block, false);
	}
	return curr_block;
}

int dataBlockWrite(int blockIndex, int startOffset, int byteCount, void* buf) {
	if(byteCount == 0){
		return 0;
	}
	uint8_t bounce_buf[BLOCK_SIZE];
	block_read(blockIndex+supB.dataBStartIndex, bounce_buf);

	memcpy(&bounce_buf[startOffset], buf, byteCount);
	if(-1 == block_write(blockIndex+supB.dataBStartIndex, bounce_buf)){
		return -1;
	}
	
	return byteCount;
}


int fs_write(int fd, void *buf, size_t count)
{
	if(!mounted || !isFDValid(fd) || buf==NULL){
		return -1;
	} else if(count == 0){
		return 0;
	}

	int RDIndex = fdTable[fd].placeInRD;
	size_t offset = fdTable[fd].offset;
	size_t startingDBInd = rDir[RDIndex].firstDBIndex;

	if(startingDBInd == FAT_EOC){
		uint16_t i = -1;
		do{
			i++;
		}
		while(i < supB.numDblocks && FAT[i] != 0);

		if(i < supB.numDblocks){
			rDir[RDIndex].firstDBIndex = i;
			startingDBInd = i;
			FAT[i] = FAT_EOC;
		} else {
			return 0;
		}
	}

	size_t relativeOffset;
	size_t currBlock = findCurrBlock(offset, startingDBInd, &relativeOffset);

	int buf_index = 0;

	int byteCount = count;
	if(count > BLOCK_SIZE - relativeOffset){
		byteCount = BLOCK_SIZE - relativeOffset;
	} 

	int BytesWritten = dataBlockWrite(currBlock, relativeOffset, byteCount, buf);
	if(BytesWritten == -1){
		return 0; 
	}
	buf_index += BytesWritten;
	currBlock = getNextBlock(currBlock, true);
	if(currBlock == FAT_EOC){
		fdTable[fd].offset += buf_index + BytesWritten;
		return buf_index;
	}

	// middle full blocks
	while(count - buf_index >= BLOCK_SIZE){
		BytesWritten = dataBlockWrite(currBlock, 0, BLOCK_SIZE, &((uint8_t*)buf)[buf_index]);
		if(BytesWritten == -1){
			return buf_index;  
		}
		buf_index += BytesWritten; 
		currBlock = getNextBlock(currBlock, true);
		if(currBlock == FAT_EOC){
			fdTable[fd].offset += buf_index + BytesWritten;
			return buf_index;
		}
	}

	BytesWritten = dataBlockWrite(currBlock, 0, count - buf_index, &((uint8_t*)buf)[buf_index]);
	if(BytesWritten == -1){
		return buf_index; 
	}

	fdTable[fd].offset += buf_index + BytesWritten;

	if(fdTable[fd].offset > rDir[fdTable[fd].placeInRD].fileSize){
		rDir[fdTable[fd].placeInRD].fileSize = fdTable[fd].offset;
	}

	block_write(supB.rootDirBlockIndex, rDir);

	return buf_index + BytesWritten;
}




int dataBlockRead(int blockIndex, int startOffset, int byteCount, void* buf) {
	uint8_t bounce_buf[BLOCK_SIZE];
	if(byteCount == 0){
		return 0;
	}
	if(-1 == block_read(blockIndex+supB.dataBStartIndex, bounce_buf)){
		return -1;
	}
	memcpy(buf, &bounce_buf[startOffset], byteCount);
	return byteCount;
}


int fs_read(int fd, void *buf, size_t count)
{
    if(!mounted || !isFDValid(fd) || buf==NULL){
		return -1;
	} else if(count == 0){
		return 0;
	}

	int RDIndex = fdTable[fd].placeInRD;
	size_t offset = fdTable[fd].offset;
	size_t startingDBInd = rDir[RDIndex].firstDBIndex;

	size_t relativeOffset;
	size_t currBlock = findCurrBlock(offset, startingDBInd, &relativeOffset);

	int buf_index = 0;

	// BytesToRead = minimum of count and whats left of file 
	size_t BytesLeftOfFile = rDir[RDIndex].fileSize - offset;
	size_t BytesToRead = count;
	if(BytesLeftOfFile < count){
		BytesToRead = BytesLeftOfFile;
	}

	int BytesCopied = 0;
	int BytesToReadInBlock;

	// first block
	if(BytesToRead > BLOCK_SIZE - relativeOffset){
		BytesToReadInBlock = BLOCK_SIZE - relativeOffset;
	} else {
		BytesToReadInBlock = BytesToRead;
	}
	
	BytesCopied = dataBlockRead(currBlock, relativeOffset, BytesToReadInBlock, buf);
	if(BytesCopied == -1){
		return 0;  
	}
	BytesToRead -= BytesCopied;
	buf_index += BytesCopied; 
	currBlock = getNextBlock(currBlock, false);

	// middle full blocks
	while(BytesToRead >= BLOCK_SIZE){
		BytesCopied = dataBlockRead(currBlock, 0, BLOCK_SIZE, &((uint8_t*)buf)[buf_index]);
		if(BytesCopied == -1){
			return buf_index;  
		}
		BytesToRead -= BytesCopied;
		buf_index += BytesCopied; 
		currBlock = getNextBlock(currBlock, false);
	}

	// last block
	BytesCopied = dataBlockRead(currBlock, 0, BytesToRead, &((uint8_t*)buf)[buf_index]);
	if(BytesCopied == -1){
		return buf_index;  
	}

	fdTable[fd].offset +=  buf_index + BytesCopied;
	return buf_index + BytesCopied;
}
