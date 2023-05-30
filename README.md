# IMPORTANT INFOS:
### The size of virtual disk blocks is 4096 bytes EACH


The Superblock is the very first block of the disk and contains information about the file system (number of blocks, size of the FAT, etc.)(**only 1 superblock**)

The File Allocation Table is located on one or more blocks, and keeps track of both the free data blocks and the mapping between files and the data blocks holding their content.

The Root directory is in the following block and contains an entry for each file of the file system, defining its name, size and the location of the first data block for this file. (**only 1 root directory**)

Finally, all the remaining blocks are Data blocks and are used by the content of files.

- each entry of the FAT is **16-bits or 2 bytes per entry (or data block)** so for 100 data blocks you would need 100 entries and 100*2 bytes (200 bytes)
- **each disk block is 4096 bytes max** so you'd need **one block of FAT** for **100 data blocks**, (so each block of FAT can only hold 4096/2 entries of data blocks)

(**max data block is 1 to 8192, so mathematically the size of the FAT will be 8192 x 2 = 16384 bytes long, thus spanning 16384 / 4096 = 4 block**)



