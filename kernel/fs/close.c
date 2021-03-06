/* Closes an open file descriptor. If its a pipe, we shutdown the pipe too */
#include <sea/tm/blocking.h>
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/sys/fcntl.h>
#include <sea/dm/block.h>
#include <sea/dm/char.h>
#include <sea/rwlock.h>
#include <stdatomic.h>
#include <sea/fs/file.h>
#include <sea/fs/pipe.h>
#include <sea/errno.h>
#include <sea/fs/socket.h>
#include <sea/fs/dir.h>

int sys_close(int fp)
{
	return file_close_fd(fp);
}
#if 0
int sys_close(int fp)
{
	struct file *f = file_get(fp);
	if(!f)
		return -EBADF;
	assert(f->inode);
	/* handle sockets calling close. We just translate it to a call to shutdown.
	 * be aware that shutdown does end up calling close! */
	//if(f->socket) {
	//	fs_fput(current_process, fp, 0);
	//	sys_sockshutdown(fp, SHUT_RDWR);
	//	return 0;
	//}
	if(f->inode->pipe)
	{
		atomic_fetch_sub(&f->inode->pipe->count, 1);
		if(f->flags & _FWRITE) {
			atomic_fetch_sub(&f->inode->pipe->wrcount, 1);
		}
		tm_blocklist_wakeall(&f->inode->pipe->read_blocked);
		tm_blocklist_wakeall(&f->inode->pipe->write_blocked);
	}
	/* close devices */
	if(S_ISCHR(f->inode->mode) && !fp)
		dm_char_rw(CLOSE, f->inode->phys_dev, 0, 0);
	else if(S_ISBLK(f->inode->mode) && !fp)
		dm_block_device_rw(CLOSE, f->inode->phys_dev, 0, 0, 0);
	if(f->dirent)
		vfs_dirent_release(f->dirent);
	vfs_icache_put(f->inode);
	fs_fput(current_process, fp, FPUT_CLOSE);
	return 0;
}
#endif
