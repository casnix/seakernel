#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/errno.h>
#include <sea/cpu/atomic.h>

static int do_fs_unlink(struct inode *node, const char *name, size_t namelen, int rec)
{
	if(!vfs_inode_check_permissions(node, MAY_WRITE, 0))
		return -EACCES;
	struct dirent *dir = fs_dirent_lookup(node, name, namelen);
	if(!dir)
		return -ENOENT;
	struct inode *target = fs_dirent_readinode(dir, 1);
	if(S_ISDIR(target->mode) && rec) {
		do_fs_unlink(target, "..", 2, 0);
		do_fs_unlink(target, ".", 1, 0);
	}
	rwlock_acquire(&dir->lock, RWL_WRITER);
	dir->flags |= DIRENT_UNLINK;
	rwlock_release(&dir->lock, RWL_WRITER);
	vfs_dirent_release(dir);
	return 0;
}

int fs_unlink(struct inode *node, const char *name, size_t namelen)
{
	return do_fs_unlink(node, name, namelen, 1);
}

int fs_link(struct inode *dir, struct inode *target, const char *name, size_t namelen)
{
	rwlock_acquire(&dir->lock, RWL_WRITER);
	if(!vfs_inode_check_permissions(dir, MAY_WRITE, 0))
		return -EACCES;
	if(!S_ISDIR(dir->mode))
		return -ENOTDIR;
	int r = fs_callback_inode_link(dir, target, name, namelen);
	rwlock_release(&dir->lock, RWL_WRITER);
	if(r)
		return r;
	return 0;
}
