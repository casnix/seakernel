#ifndef __SEA_DM_PIPE_H
#define __SEA_DM_PIPE_H

#include <sea/ll.h>
#include <sea/mutex.h>
#include <sea/types.h>
#include <sea/fs/inode.h>

typedef struct pipe_struct {
	size_t pending;
	size_t write_pos, read_pos;
	char *buffer;
	off_t length;
	mutex_t lock;
	_Atomic int count, wrcount;
	struct blocklist read_blocked, write_blocked;
} pipe_t;

int sys_mkfifo(char *path, mode_t mode);
void fs_pipe_free(struct inode *i);
int fs_pipe_read(struct inode *ino, int flags, char *buffer, size_t length);
int fs_pipe_write(struct inode *ino, int flags, char *buffer, size_t length);
int fs_pipe_select(struct inode *in, int rw);
int sys_pipe(int *files);
pipe_t *fs_pipe_create();

#endif

