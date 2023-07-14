#include "userfs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file {
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
    bool lazy_delete;
    size_t size;
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
    int flags;
    size_t offset;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

int
ufs_open(const char *filename, int flags)
{
    struct file *f_ptr = NULL;
    // Look for a file in the file list
    for(struct file *f_iter = file_list; f_iter != NULL; f_iter = f_iter->next)
        if(!strcmp(f_iter->name, filename) && !f_iter->lazy_delete)
            f_ptr = f_iter;            
    
    // No file is found
    if(!f_ptr) {
        // Cannot create file error
        if(!(flags & UFS_CREATE)) {
            ufs_error_code = UFS_ERR_NO_FILE;
            return -1;
        }
        // Create a file otherwise
        f_ptr = (struct file*) malloc(sizeof(struct file));
        // Out of memory error
        if(!f_ptr) {
            ufs_error_code = UFS_ERR_NO_MEM;
            return -1;
        }
        // Initialization
        *f_ptr = (struct file) {
            .name = strdup(filename),
            .last_block = NULL,
            .block_list = NULL,
            .next = NULL,
            .prev = NULL,
            .lazy_delete = false,
            .refs = 0,
            .size = 0,
        };
        // File list exists
        if(file_list) {
            struct file* f_iter;
            // Find the last file in the list
            for(f_iter = file_list; f_iter->next != NULL; f_iter = f_iter->next);
            // Set links
            f_iter->next = f_ptr;
            f_ptr->prev = f_iter;
            f_ptr->next = NULL;
        }
        else {
            file_list = f_ptr;
        }
    }
    // Keep reference count
    ++(f_ptr->refs);

    // Create a file descriptor
    struct filedesc *fd_ptr = (struct filedesc*) malloc(sizeof(struct filedesc));
    // Out of memory error
    if(!fd_ptr) {
        ufs_error_code = UFS_ERR_NO_MEM;
        return -1;
    }
    *fd_ptr = (struct filedesc) {
      .file = f_ptr,
      .flags = flags,
      .offset = 0,
    };
    int fd = -1;
    // Initialize the descriptors stack
    if(!file_descriptor_capacity) {
        file_descriptors = realloc(file_descriptors, (++file_descriptor_capacity) * sizeof(struct filedesc*));
        file_descriptors[0] = NULL;
    }
    // Look up for a free file descriptor
    for(int fd_iter = file_descriptor_count; fd_iter < file_descriptor_capacity; ++fd_iter)
        if(!file_descriptors[fd_iter]) {
            fd = fd_iter;
            break;
        }
    // No available descriptors
    if(fd == -1) {
        // Double the size of the file descriptor list to do fewer reallocations in case of large amount of files
        file_descriptors = realloc(file_descriptors, (file_descriptor_capacity *= 2) * sizeof(struct filedesc*) );
        fd = file_descriptor_count;
    }
    file_descriptors[fd] = fd_ptr;
    // Increment the descriptor count
    ++file_descriptor_count;
    return fd;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
    // File not found error
	if(fd < 0 || fd >= file_descriptor_capacity || !file_descriptors[fd]) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    struct filedesc *fd_ptr = file_descriptors[fd];
    // No permission error
    if(fd_ptr->flags & UFS_READ_ONLY) {
        ufs_error_code = UFS_ERR_NO_PERMISSION;
        return -1;
    }

    struct file *f_ptr = fd_ptr->file;
    // Initalize the block list (if needed)
    if(!f_ptr->block_list) {
        f_ptr->last_block = (struct block*) malloc(sizeof(struct block));
        f_ptr->block_list = f_ptr->last_block;
        *f_ptr->last_block = (struct block) {
            .memory = (char*)malloc(BLOCK_SIZE * sizeof(char)),
            .prev = NULL,
            .next = NULL,
            .occupied = 0,
        };
    }
    // Fix the offset (if needed)
    fd_ptr->offset = f_ptr->size < fd_ptr->offset ? f_ptr->size : fd_ptr->offset;
    int b_id = 0;
    struct block *b_ptr = f_ptr->block_list;
    // Find the last block
    while(BLOCK_SIZE * (++b_id) < fd_ptr->offset) {
        b_ptr = b_ptr->next;
        // Create missing blocks (if needed)
        if(!b_ptr) {
            b_ptr = (struct block*) malloc(sizeof(struct block));
            *b_ptr = (struct block) {
              .memory = (char *) malloc(BLOCK_SIZE * sizeof(char)),
              .prev = NULL,
              .next = NULL,
              .occupied = 0,
            };
            f_ptr->last_block->next = b_ptr;
            f_ptr->last_block = b_ptr;
            f_ptr->last_block->next = NULL;
        }
    }
    // Offset in this block
    size_t b_offset = fd_ptr->offset % BLOCK_SIZE;
    size_t w_bytes = 0;
    // Write the given bytes
    while(w_bytes < size) {
        // Create a new block if the current one is full
        if(b_ptr->occupied == BLOCK_SIZE) {
            struct block *b_new = (struct block*) malloc(sizeof(struct block));
            *b_new = (struct block) {
                    .memory = (char *) malloc(BLOCK_SIZE * sizeof(char)),
                    .prev = f_ptr->last_block,
                    .next = NULL,
                    .occupied = 0,
            };
            b_ptr = b_new;
            f_ptr->last_block->next = b_ptr;
            f_ptr->last_block = b_ptr;
            f_ptr->last_block->next = NULL;
            b_offset = 0;
        }
        // In case the data to write is larger than available space in block
        size_t w_size = BLOCK_SIZE - b_offset > size - w_bytes ? size - w_bytes : BLOCK_SIZE - b_offset;
        // File is too large error
        if(f_ptr->size + w_size > MAX_FILE_SIZE) {
            ufs_error_code = UFS_ERR_NO_MEM;
            return -1;
        }
        // Write data
        memcpy(b_ptr->memory + b_offset, buf + w_bytes, w_size);
        // Update the offsets and sizes
        b_offset += w_size, fd_ptr->offset += w_size, w_bytes += w_size;
        b_ptr->occupied = b_offset < b_ptr->occupied ? b_ptr->occupied : b_offset;
        f_ptr->size = fd_ptr->offset > f_ptr->size ? fd_ptr->offset : f_ptr->size;
    }
	return w_bytes;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
    // File not found error
    if(fd < 0 || fd >= file_descriptor_capacity || !file_descriptors[fd]) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    struct filedesc *fd_ptr = file_descriptors[fd];
    // No permission error
    if(fd_ptr->flags & UFS_WRITE_ONLY) {
        ufs_error_code = UFS_ERR_NO_PERMISSION;
        return -1;
    }
    struct file *f_ptr = fd_ptr->file;
    // Fix the offset (if needed)
    fd_ptr->offset = f_ptr->size < fd_ptr->offset ? f_ptr->size : fd_ptr->offset;
    if(fd_ptr->offset == f_ptr->size)
        fd_ptr->offset = 0;
    struct block *b_ptr;
    int b_id;
    // Look up for the needed block
    for(b_ptr = f_ptr->block_list, b_id = 0; b_id < fd_ptr->offset / BLOCK_SIZE; b_ptr = b_ptr->next, ++b_id)
        if(!b_ptr)
            return 0;
    // Offset in this block
    size_t b_offset = fd_ptr->offset % BLOCK_SIZE;
    size_t r_bytes = 0;
    // Read the given bytes
    while(r_bytes < size) {
        // All the data is read
        if(!b_ptr || b_ptr->occupied == b_offset)
            return r_bytes;
        // Get the correct byte count to read in current block
        size_t r_size = BLOCK_SIZE - b_offset > size - r_bytes ?
                        size - r_bytes : BLOCK_SIZE - b_offset;
        r_size = r_size > b_ptr->occupied - b_offset ?
                 b_ptr->occupied - b_offset : r_size;

        memcpy(buf + r_bytes, b_ptr->memory + b_offset, r_size);
        // Update the offsets
        fd_ptr->offset += r_size, r_bytes += r_size, b_offset += r_size;
        // Skip to the next block if the current one is fully read
        if(b_offset == BLOCK_SIZE) {
            b_ptr = b_ptr->next;
            b_offset = 0;
        }
    }
    return r_bytes;
}

int
ufs_close(int fd)
{
    // Check the correctness of the file descriptor
    if(fd < 0 || fd >= file_descriptor_capacity || !file_descriptors[fd]) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    // Get the file descriptor
    struct filedesc *fd_ptr = file_descriptors[fd];
    struct file *f_ptr = fd_ptr->file;
    // Decrement the reference counter
    --(f_ptr->refs);
    // Perform a lazy deletion in case this was the last reference
    if(!f_ptr->refs && f_ptr->lazy_delete) {
        struct block *b_ptr = NULL;
        // Clear the memory blocks
        for(struct block *b_iter = f_ptr->block_list; b_iter != NULL; b_iter = b_iter->next) {
            // Free previous block
            free(b_iter->memory);
            if(b_iter->prev)
                free(b_iter->prev);
            b_ptr = b_iter;
        }
        // Free the last block
        free(b_ptr);
        // Free the filename
        free((void *)f_ptr->name);
        // Link previous and next files in the list (if possible)
        if(f_ptr->next && f_ptr->prev) {
            f_ptr->next->prev = f_ptr->prev;
            f_ptr->prev->next = f_ptr->next;
        }
        else if(f_ptr->next)
            f_ptr->next->prev = NULL;
        else if(f_ptr->prev)
            f_ptr->prev->next = NULL;
        // Set the next pointer as the beginning of the list (if needed)
        if(file_list == f_ptr)
            file_list = f_ptr->next ? f_ptr->next : NULL;
        // Free the file
        free(f_ptr);
    }
    // Free the file descriptor
    free(file_descriptors[fd]);
    file_descriptors[fd] = NULL;
    // Shrink the file descriptor list (if needed)
    --file_descriptor_count;
    while(file_descriptors && file_descriptor_capacity > file_descriptor_count && !file_descriptors[file_descriptor_capacity - 1])
        free(file_descriptors[--file_descriptor_capacity]);
    // Reallocate
    file_descriptors = realloc(file_descriptors, file_descriptor_capacity * sizeof(struct filedesc*));
    return 0;
}

int
ufs_delete(const char *filename)
{
    struct file *f_ptr = NULL;
    // Look up for the file
    for(struct file *f_iter = file_list; f_iter != NULL; f_iter = f_iter->next) {
        if(!f_iter->lazy_delete && !strcmp(f_iter->name, filename))
            f_ptr = f_iter;
    }
    // No file error
    if(!f_ptr) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    // In case file has no active references
    if(!f_ptr->refs) {
        struct block *b_ptr = NULL;
        int cnt = 0;
        // Clear the memory blocks
        for(struct block *b_iter = f_ptr->block_list; b_iter != NULL; b_iter = b_iter->next) {
            // Free previous block
            free(b_iter->memory);
            if(b_iter->prev)
                free(b_iter->prev);
            b_ptr = b_iter;
        }
        // Free the last block
        free(b_ptr);
        // Free the filename
        free((void *)f_ptr->name);
        // Link previous and next files in the list (if possible)
        if(f_ptr->prev && f_ptr->next) {
            f_ptr->next->prev = f_ptr->prev;
            f_ptr->prev->next = f_ptr->next;
        }
        else if(f_ptr->next)
            f_ptr->next->prev = NULL;
        else if(f_ptr->prev)
            f_ptr->prev->next = NULL;
        // Set the next pointer as the beginning of the list (if needed)
        if(file_list == f_ptr)
            file_list = f_ptr->next ? f_ptr->next : NULL;
        // Free the file
        free(f_ptr);
    }
    // Set the lazy deletion flag otherwise (will be deleted when closed)
    else {
        f_ptr->lazy_delete = true;
    }
    return 0;
}
int
ufs_resize(int fd, size_t new_size) {
    if(fd < 0 || fd >= file_descriptor_count || !file_descriptors[fd]) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    if(new_size > MAX_FILE_SIZE) {
        ufs_error_code = UFS_ERR_NO_MEM;
        return -1;
    }
    struct file *f_ptr = file_descriptors[fd]->file;
    if(!f_ptr) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    // Count the blocks
    int blocks = f_ptr->size / BLOCK_SIZE + (f_ptr->size % BLOCK_SIZE ? 1 : 0);
    // If shrink is needed
    if(new_size < f_ptr->size) {
        while((--blocks) * BLOCK_SIZE > new_size)
            if(f_ptr->last_block->prev) {
                struct block *b_ptr = f_ptr->last_block->prev;
                free(b_ptr->next->memory);
                free(b_ptr->next);
                b_ptr->next = NULL;
                f_ptr->last_block = b_ptr;
            }
    }
    // Otherwise
    else {
        while((blocks++) * BLOCK_SIZE < new_size) {
            struct block *b_ptr = (struct block *) malloc(sizeof(struct block));
            *b_ptr = (struct block) {
                .memory = (char *)malloc(BLOCK_SIZE * sizeof(char)),
                .next = NULL,
                .prev = f_ptr->last_block,
                .occupied = 0
            };
            if(!f_ptr->block_list) {
                f_ptr->block_list = b_ptr;
                f_ptr->last_block = b_ptr;
            }
            else {
                f_ptr->last_block->next = b_ptr;
                f_ptr->last_block = b_ptr;
            }
        }
    }
    // New size is not the exact size needed since operation is done by blocks
    f_ptr->size = new_size;
    // Correct the number of occupated bytes in the last block (if needed)
    if(f_ptr->last_block)
        f_ptr->last_block->occupied = new_size % BLOCK_SIZE;
    return 0;
}

void
ufs_destroy(void)
{
    struct file *f_ptr = NULL;
    for(int fd_iter = 0; fd_iter < file_descriptor_capacity; ++fd_iter) {
        if(file_descriptors[fd_iter]) {
            free(file_descriptors[fd_iter]->file);
            free(file_descriptors[fd_iter]);
        }
    }
    free(file_descriptors);
    for(f_ptr = file_list; f_ptr != NULL; f_ptr = f_ptr->next) {
        struct block *b_ptr = NULL;
        int cnt = 0;
        // Clear the memory blocks
        for(struct block *b_iter = f_ptr->block_list; b_iter != NULL; b_iter = b_iter->next) {
            // Free previous block
            free(b_iter->memory);
            if(b_iter->prev)
                free(b_iter->prev);
            b_ptr = b_iter;
        }
        // Free the last block
        free(b_ptr);
        // Free previous reference (if needed)
        if(f_ptr->prev)
            free(f_ptr->prev);
        // Free the filename
        free((void *)f_ptr->name);
        // Free the file
        free(f_ptr);
    }
    free(file_list);
    return;
}
