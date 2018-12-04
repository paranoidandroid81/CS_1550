/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

//disk = 5MB = 5242880 bytes
#define DISK_SIZE 5242880

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//bitmap will need to be max_blocks / 8, as 1 char/byte = 8 bits
#define BITMAP_SIZE (DISK_SIZE / BLOCK_SIZE) / 8

//bitmap starts on disk at end - 1 (index starts at 0) - size of bitmap
#define MAP_START (DISK_SIZE - 1 - BITMAP_SIZE)

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//for debugging, converts byte to equivalent binary w masking
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')


//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

struct cs1550_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

//struct to hold info on allocated block bitwise index and actual index on disk
struct cs1550_free_block
{
	int bit_idx;
	int actual_idx;
};

typedef struct cs1550_free_block cs1550_free_block;

//helper to get the root directory from the disk
static cs1550_root_directory* get_root(FILE* this_disk)
{
	fseek(this_disk, 0, SEEK_SET);						//root is 1st block on disk
	cs1550_root_directory* this_root = (cs1550_root_directory*)malloc(sizeof(cs1550_root_directory));
	fread(this_root, BLOCK_SIZE, 1, this_disk);				//read 1 block
	return this_root;
}

//helper to write modified block to disk
static void write_block(FILE* this_disk, void* mod_block, int block_idx)
{
	//printf("block idx: %d\n", block_idx);								//DEBUG
	//go from beginning of disk to start write index
	fseek(this_disk, (block_idx * BLOCK_SIZE), SEEK_SET);					//byte idx on disk = block_idx * num_bytes per block
	fwrite(mod_block, BLOCK_SIZE, 1, this_disk);
}

//helper to retrieve current bitmap from disk (stored at end)
static void get_bitmap(FILE* this_disk, unsigned char** map)
{
	//printf("bitmap size: %d\n", BITMAP_SIZE);
	fseek(this_disk, MAP_START, SEEK_SET);					//go to beginning of map from disk
	fread(*map, sizeof(unsigned char), BITMAP_SIZE, this_disk);
}

//helper to find free block on bitmap, should skip 1st as == root
static cs1550_free_block* find_free(unsigned char** map)
{
	cs1550_free_block* this_block = (cs1550_free_block*)malloc(sizeof(cs1550_free_block));
	int map_idx, bit_idx;
	unsigned char curr_bit;
//	int bitmap_idx;
	/*for (bitmap_idx = 0; bitmap_idx < 8; bitmap_idx++)
	{
		printf("curr_byte mapb4disk: " BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY((*map)[bitmap_idx]));					//DEBUG
	}*/
	for (map_idx = 0; map_idx < BITMAP_SIZE; map_idx++)
	{
	//	printf("Current map idx: %d\n", map_idx);					//DEBUG
	//	printf("curr_byte: " BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY((*map)[map_idx]));					//DEBUG
		//all bits 1s, no free blocks
		if (map_idx != 0 && ((*map)[map_idx] == 0xFF))
		{
			//completely full block (other than root) will be all 1s, = 0xFF
			continue;
		} else if (map_idx == 0 && ((*map)[map_idx] == 0x7F))
		{
			//first byte, since root is unused, is only 0x7F
			continue;
		}
		//must iterate thru each 8 bits in char on disk map
		for (bit_idx = 7; bit_idx >= 0; bit_idx--)
		{
		//	printf("bit idx: %d\n", bit_idx);						//DEBUG
			//ignore 1st bit, root dir
			if (map_idx == 0 && bit_idx == 7)										continue;
			//access single bit and mask out rest
			unsigned char mask = 0x01 << bit_idx;			//00000001b to mask out bit in question

			curr_bit = (*map)[map_idx] & mask;
			//printf("curr_bit: " BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(curr_bit));								//DEBUG
			//0 bit is free, 1 bit is used
			if (!curr_bit)
			{
				//we've found a free block!
				//translate to block index for actual and bit (different)
				this_block->bit_idx = (map_idx * 8) + bit_idx;				//bit index
				this_block->actual_idx = (map_idx * 8) + (7 - bit_idx);	//actual index
				return this_block;
			}
		}
	}
	return this_block;
}

//helper to set a particular bit on the bitmap, translates from block to map location
//flag = 0 if free, 1 is used
static void set_bit(unsigned char** map, int block_idx, unsigned char flag)
{
	int map_idx, bit_idx;
	map_idx = block_idx / 8;					//index of char on bitmap of block. int rounds down
	bit_idx = block_idx % 8;								//where within 8 bits of char is block represented? 0-7 possible bit indices
	//printf("set_bit block_idx: %d\n", block_idx);				//DEBUG
	//printf("map_idx: %d\n", map_idx);								//DEBUG
	//printf("bit_idx = %d\n", bit_idx);							//DEBUG
//	printf("curr_byte set_bit: " BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY((*map)[map_idx]));					//DEBUG
	//if 0, must 0 out bit w/ mask, otherwise change to 1
	unsigned char mask = 1 << bit_idx;			//00000001b to mask out bit in question
//	printf("mask set_bit: " BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(mask));					//DEBUG
	if (!flag)
	{
		(*map)[map_idx] &= ~(mask);				//zero out bit, mask to keep rest
	} else
	{
	//	printf("setting used flag!\n");						//DEBUG
		(*map)[map_idx] |= mask;						//otherwise make the digit in question a one
	//	printf("curr_byte after_set_bit flag: " BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(*map[map_idx]));					//DEBUG
	}
}


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));			//reset stat buf for attributes

	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		//store path data for validation and scanning into
		char directory[MAX_FILENAME + 1];				//directory name + space for nul
		char filename[MAX_FILENAME + 1];					//filename + space for nul
		char extension[MAX_EXTENSION + 1];				//extension + space for nul

		//parse file info from path into buffers, error if less than 1 arg
		int path_count = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		if (path_count < 1)
			return -ENOENT;
		//first, we need to open the disk for reading/writing in binary, return error if device can't be opened
		FILE* disk = fopen(".disk", "r+b");
		if (!disk)						return -ENXIO;
		//first get the root from the disk so we can access subdirectories
		cs1550_root_directory* root = get_root(disk);
		if (!root) 						return -ENOENT;						//couldn't find root dir
		int found_idx = -1;						//use to hold subdir index once found, -1 as flag if not found
		int dir_idx;					//use to iterate through subdirectories, looking for subdirectory
		for (dir_idx = 0; dir_idx < root->nDirectories; dir_idx++)
		{
			if (strcmp(root->directories[dir_idx].dname, directory) == 0)
			{
				//found!
				found_idx = dir_idx;
				break;
			}
		}
		//nothing ever found!
		if (found_idx == -1)						return -ENOENT;
		//Check if name is subdirectory
		//if only 1 var filled, must be subdir
		if (path_count == 1)
		{
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			res = 0; //no error
		} else
		{
			//otherwise, must load subdir and find file
			cs1550_directory_entry* sub_dir = (cs1550_directory_entry*)malloc(sizeof(cs1550_directory_entry));
			//sub dir byte offset from start of disk = block location on disk * bytes in a block
			fseek(disk, root->directories[found_idx].nStartBlock * BLOCK_SIZE, SEEK_SET);
			//each dir is 1 block
			fread(sub_dir, BLOCK_SIZE, 1, disk);
			if (!sub_dir)									return -ENOENT;					//sub dir not found!
			//now we search for regular file
			found_idx = -1;								//flag once again
			for (dir_idx = 0; dir_idx < sub_dir->nFiles; dir_idx++)
			{
				if (strcmp(sub_dir->files[dir_idx].fname, filename) == 0
						&& strcmp(sub_dir->files[dir_idx].fext, extension) == 0)
				{
					//found file!
					found_idx = dir_idx;
					stbuf->st_mode = S_IFREG | 0666;
					stbuf->st_nlink = 1; //file links
					stbuf->st_size = 0; //file size - make sure you replace with real size!
					break;
				}
			}
			if (found_idx == -1)
			{
				res = -ENOENT;			//file doesn't exist
			} else
			{
				res = 0;				//success!
			}
			free(sub_dir);				//free malloced sub directory
		}
		free(root);						//free malloced root
		//close the open disk file
		fclose(disk);
	}
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	//store path data for validation and scanning into
	char directory[MAX_FILENAME + 1];				//directory name + space for nul
	char filename[MAX_FILENAME + 1];					//filename + space for nul
	char extension[MAX_EXTENSION + 1];				//extension + space for nul

	//parse dir info from path into buffers, error if more than 1 arg as should be dir
	int path_count = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (path_count > 1)
		return -ENOENT;

	//the filler function allows us to add entries to the listing
	//read the fuse.h file for a description (in the ../include dir)
	//fill with generic working and prev directory entries
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//first, we need to open the disk for reading/writing in binary, return error if device can't be opened
	FILE* disk = fopen(".disk", "r+b");
	if (!disk)						return -ENXIO;
	//first get the root from the disk so we can access subdirectories
	cs1550_root_directory* root = get_root(disk);
	if (!root) 						return -ENOENT;						//couldn't find root dir

	int dir_idx;
	//first, let's see if the path is just the root
	//if so, we just need to list all the subdirectories'
	if (strcmp(path, "/") == 0)
	{
		//iterate through and add entries thru subdirectories on disk
		for (dir_idx = 0; dir_idx < root->nDirectories; dir_idx++)
		{
			filler(buf, root->directories[dir_idx].dname, NULL, 0);
		}
	} else
	{
		//this is a subdirectory, must search for it then list all files within
		int found_idx = -1;					//flag for search
		for (dir_idx = 0; dir_idx < root->nDirectories; dir_idx++)
		{
			if (strcmp(root->directories[dir_idx].dname, directory) == 0)
			{
				//found!
				found_idx = dir_idx;
				break;
			}
		}
		//nothing ever found!
		if (found_idx == -1)						return -ENOENT;
		//now that we found it, need to load correct subdir
		cs1550_directory_entry* sub_dir = (cs1550_directory_entry*)malloc(sizeof(cs1550_directory_entry));
		//sub dir byte offset from start of disk = block location on disk * bytes in a block
		fseek(disk, root->directories[found_idx].nStartBlock * BLOCK_SIZE, SEEK_SET);
		//each dir is 1 block
		fread(sub_dir, BLOCK_SIZE, 1, disk);
		if (!sub_dir)									return -ENOENT;					//sub dir not found!
		//now list the files in the subdir
		for (dir_idx = 0; dir_idx < sub_dir->nFiles; dir_idx++)
		{
			//full filename should be listed
			//e.g. filename.extension
			//max full filename = max name + max_ext + 1 for . + 1 for nul
			char file_buf[MAX_FILENAME + MAX_EXTENSION + 2];
			//append strings of given file together then print with filler
			snprintf(file_buf, sizeof(file_buf), "%s.%s", sub_dir->files[dir_idx].fname, sub_dir->files[dir_idx].fext);
			filler(buf, file_buf, NULL, 0);
		}
		free(sub_dir);				//now can free sub dir as done reading
	}
	free(root);					//done with root
	//close the open disk file
	fclose(disk);
	return 0;						//success!
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) mode;						//cast as we're not using

	//store path data for validation and scanning into
	char directory[MAX_FILENAME + 1];				//directory name + space for nul
	char filename[MAX_FILENAME + 1];					//filename + space for nul
	char extension[MAX_EXTENSION + 1];				//extension + space for nul

	//parse dir info from path into buffers, error if more than 1 arg as should be dir directly under root
	int path_count = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (path_count > 1)
		return -EPERM;

	//make sure attempted name of dir not too long
	if (strlen(directory) > MAX_FILENAME)								return -ENAMETOOLONG;
	//first, we need to open the disk for reading/writing in binary, return error if device can't be opened
	FILE* disk = fopen(".disk", "r+b");
	if (!disk)						return -ENXIO;
	//first get the root from the disk so we can create subdirectory
	cs1550_root_directory* root = get_root(disk);
	if (!root) 						return -ENOENT;						//couldn't find root dir
	//first, we must check to make sure root has enough space, error if not
	if (root->nDirectories >= MAX_DIRS_IN_ROOT)									return -ENOSPC;
	//now make sure it doesn't already exist
	int dir_idx;
	for (dir_idx = 0; dir_idx < root->nDirectories; dir_idx++)
	{
		if (strcmp(root->directories[dir_idx].dname, directory) == 0)
		{
			return -EEXIST;						//already exists, error!
		}
	}
	//since we've confirmed it's valid, now we must create a new dir
	//must find a free block on the disk
	//retrieve the bitmap for this disk
	unsigned char* bitmap = (unsigned char*) malloc(BITMAP_SIZE);
	memset(bitmap, 0, BITMAP_SIZE);					//zero out bitmap before use
	get_bitmap(disk, &bitmap);
	/*
	int bitmap_idx;
	for (bitmap_idx = 0; bitmap_idx < 8; bitmap_idx++)
	{
		printf("curr_byte mapb4disk: " BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(bitmap[bitmap_idx]));					//DEBUG
	}
	*/
	//find a free block on the disk, store both bitwise and actual
	cs1550_free_block* free_idx = find_free(&bitmap);
	//allocate memory for a new directory
	cs1550_directory_entry* new_dir = (cs1550_directory_entry*)malloc(sizeof(cs1550_directory_entry));
	//now add the new directory to the root struct
	strcpy(root->directories[root->nDirectories].dname, directory);				//copy name of new directory into root storage
	root->directories[root->nDirectories].nStartBlock = free_idx->actual_idx;					//add start index of block
	//printf("start block: %ld\n", root->directories[root->nDirectories].nStartBlock);					//DEBUG
	root->nDirectories++;								//increment number of used directories
	//printf("num_dirs: %d\n", root->nDirectories);								//DEBUG
	new_dir->nFiles = 0;							//initally 0 files
	//set the bit for the bitmap on this directory to used (1)
	set_bit(&bitmap, free_idx->bit_idx, 1);
	//printf("actual idx: %d\n", free_idx->actual_idx);						//DEBUG
	//now we must save the new directory block to disk and write modified root to disk
	write_block(disk, new_dir, free_idx->actual_idx);
	write_block(disk, root, 0);					//root 1st block
	//also must write modified bitmap to disk
	/*
	//DEBUG BEFORE write
	for (bitmap_idx = 0; bitmap_idx < 8; bitmap_idx++)
	{
		printf("curr_byte mapb4disk: " BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(bitmap[bitmap_idx]));					//DEBUG
	}*/
	fseek(disk, MAP_START, SEEK_SET);
	fwrite(bitmap, sizeof(unsigned char), BITMAP_SIZE, disk);
	//now can free and close stuff
	free(new_dir);
	free(root);
	free(bitmap);
	free(free_idx);
	fclose(disk);
	//printf("done freeing....\n");			//DEBUG
	return 0;						//success!
}

/*
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{

	return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;

	//store path data for validation and scanning into
	char directory[MAX_FILENAME + 1];				//directory name + space for nul
	char filename[MAX_FILENAME + 1];					//filename + space for nul
	char extension[MAX_EXTENSION + 1];				//extension + space for nul

	//parse dir info from path into buffers, error if less than 3 args as should be dir + filename + ext
	int path_count = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (path_count < 3)
		return -ENOENT;

	//if directory var is empty, means trying to add to root dir, illegal!
	if (strlen(directory) == 0)								return -EPERM;
	//need to make sure filename + ext within bounds, otherwise illegal!
	if (strlen(filename) > MAX_FILENAME || strlen(extension) > MAX_EXTENSION)
	{
		return -ENAMETOOLONG;
	}

	//first, we need to open the disk for reading/writing in binary, return error if device can't be opened
	FILE* disk = fopen(".disk", "r+b");
	if (!disk)						return -ENXIO;
	//first get the root from the disk so we can create subdirectory
	cs1550_root_directory* root = get_root(disk);
	if (!root) 						return -ENOENT;						//couldn't find root dir
	//now we search for the desired subdirectory for the file
	int found_idx = -1;					//flag for search
	int dir_idx;
	for (dir_idx = 0; dir_idx < root->nDirectories; dir_idx++)
	{
		if (strcmp(root->directories[dir_idx].dname, directory) == 0)
		{
			//found!
			found_idx = dir_idx;
			break;
		}
	}
	//dir never found
	if (found_idx == -1)							return -ENOENT;
	//now that we found it, need to load correct subdir
	cs1550_directory_entry* sub_dir = (cs1550_directory_entry*)malloc(sizeof(cs1550_directory_entry));
	//sub dir byte offset from start of disk = block location on disk * bytes in a block
	fseek(disk, root->directories[found_idx].nStartBlock * BLOCK_SIZE, SEEK_SET);
	//store sub dir in question
	fread(sub_dir, BLOCK_SIZE, 1, disk);
	if (!sub_dir)									return -ENOENT;					//sub dir not found!
	//now iterate thru files in subdir ensure not already extant
	for (dir_idx = 0; dir_idx < sub_dir->nFiles; dir_idx++)
	{
		if (strcmp(sub_dir->files[dir_idx].fname, filename) == 0
		&& strcmp(sub_dir->files[dir_idx].fext, extension) == 0)
		{
			return -EEXIST;						//file already exists in dir
		}
	}
	//first, make sure directory has space for more files, error if not
	if (sub_dir->nFiles >= MAX_FILES_IN_DIR)								return -ENOSPC;
	//now get bitmap, use to find free block
	unsigned char* bitmap = (unsigned char*) malloc(BITMAP_SIZE);
	memset(bitmap, 0, BITMAP_SIZE);						//0 out just in case
	get_bitmap(disk, &bitmap);
  //find a free block on the disk, store both bitwise and actual
	cs1550_free_block* free_idx = find_free(&bitmap);
	//now load new attributes of file into directory
	strcpy(sub_dir->files[sub_dir->nFiles].fname, filename);
	strcpy(sub_dir->files[sub_dir->nFiles].fext, extension);
	sub_dir->nFiles++;
	//set the bit for the bitmap on this block to used (1)
	set_bit(&bitmap, free_idx->bit_idx, 1);
	//now we must save the modified dir block to disk
	write_block(disk, sub_dir, free_idx->actual_idx);
	//also must write modified bitmap to disk
	fseek(disk, MAP_START, SEEK_SET);
	fwrite(bitmap, sizeof(unsigned char), BITMAP_SIZE, disk);
	//now we can free stuff
	free(bitmap);
	free(root);
	free(sub_dir);
	fclose(disk);
	return 0;								//success!
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//set size and return, or error

	size = 0;

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//set size (should be same as input) and return, or error

	return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/*
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
