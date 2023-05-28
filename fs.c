#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#include <stdint.h>

struct Superblock {
	char signature[8];
	int16_t total_blocks_amt;
	int16_t root_dir_index;
	int16_t data_block_start;
	int16_t data_blocks_amt;
	int8_t num_FAT_blocks;
	uint8_t padding[4079];
};

struct FAT {

};

struct Directory {

};

void exit_with_error(char message[]){
	fprintf(stderr, message);
	block_disk_close();
	return -1;
}

int fs_mount(const char *diskname)
{
	if(block_disk_open(diskname) == -1){
		fprintf(stderr, "Failure opening superblock - invalid diskname\n");
		return -1;
	}

	struct Superblock superblock;

	if(block_read(0, &superblock) == -1){
		exit_with_error("Failure reading superblock\n");
	}

	if(strcmp(superblock.signature, "ECS150FS") != 0){
		exit_with_error("Invalid signature\n");
	}

	if(block_disk_count() != superblock.total_blocks_amt){
		exit_with_error("Invalid block amount\n");
	}

}

int fs_umount(void)
{
	/* TODO: Phase 1 */
}

int fs_info(void)
{
	/* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}
