// tinyfs.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)

struct timespec64 current_kernel_time(void) {
    struct timespec64 now;
    ktime_get_coarse_real_ts64(&now);
    return now;
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,14)
    #define CURRENT_TIME current_kernel_time()
#endif


#include "tinyfs.h"

struct file_blk block[MAX_FILES + 1];
// 表示当前的文件个数
int curr_count = 0;

// 我勒个去，竟然使用了全局变量！
// 获得一个尚未使用的文件块，保存新创建的文件或者目录
static int get_block(void){
    int i;
    // 就是一个遍历，但实现快速。
    for(i = 2;i < MAX_FILES; i++)
    {
        if(!block[i].busy) {
            block[i].busy =1;
            return i;
        }
    }
    return -1;
}

static struct inode_operations tinyfs_inode_ops;
// 读取目录的实现
//static int tinyfs_readdir(struct file * filp, void * dirent, filldir_t filldir)
static int tinyfs_readdir(struct file *filp, struct dir_context *ctx)
{
    loff_t pos;
    struct file_blk * blk;
    struct dir_entry * entry;
    int i;
    pos = filp ->f_pos;
    if (pos)
        return 0;
    blk = (struct file_blk *)filp->f_path.dentry->d_inode->i_private;
    if(!S_ISDIR(blk->mode))
    {
        return -ENOTDIR;
    }
    // 循环获取一个目录的所有文件的文件名
    entry = (struct dir_entry *)&blk->data[0];
    for(i = 0; i < blk->dir_children; i++)
    {
        // ctx->actotr(dirent, entry[i].filename, MAXLEN, pos, entry[i].idx, DT_UNKNOWN);
        // ctx->actor(ctx, name, namelen, ctx->pos, ino, type) == 0
        ctx->actor(ctx, entry[i].filename, MAXLEN, ctx->pos, entry[i].idx, DT_UNKNOWN);
        filp->f_pos += sizeof(struct dir_entry);
        pos += sizeof(struct dir_entry);
        ctx->pos = pos;
    }
    return 0;
}

// read实现
ssize_t tinyfs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos)
{
    struct file_blk * blk;
    char * buffer;
    blk = (struct file_blk *)filp->f_path.dentry->d_inode->i_private;
    if (*ppos >= blk->file_size)
        return 0;
    buffer = (char *)&blk->data[0];
    len = min((size_t)blk->file_size,len);
    if(copy_to_user(buf, buffer, len))
    {
        return -EFAULT;
    }
    *ppos += len;
    return len;
}

// write实现
ssize_t tinyfs_write( struct file * filp, const char __user * buf, size_t len, loff_t * ppos)
{
    struct file_blk * blk;
    char * buffer;
    blk = filp->f_path.dentry->d_inode->i_private;
    buffer = (char *)&blk->data[0];
    buffer += *ppos;
    if (copy_from_user(buffer, buf, len))
    {
        return -EFAULT;
    }
    *ppos += len;
    if (*ppos > blk->file_size)
        blk ->file_size = *ppos;
    return len;
}

// The implement of read the link from soft link file
const char *tinyfs_get_link(struct dentry *dentry, struct inode *inode,
              struct delayed_call *callback)
{
    return ((struct file_blk *)(inode->i_private))->data;
}

int tinyfs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
    return 0;
}

const struct file_operations tinyfs_file_operations = {
    .read = tinyfs_read,
    .write = tinyfs_write,
    .llseek = generic_file_llseek,
    .fsync = tinyfs_fsync
};

const struct file_operations tinyfs_dir_operations = {
    .owner = THIS_MODULE,
    // .readdir = tinyfs_readdir,
    .iterate = tinyfs_readdir,
    .llseek = generic_file_llseek
};

const struct inode_operations tinyfs_symlink_inode_ops = {
    .get_link = tinyfs_get_link
};

// 创建文件的实现
static int tinyfs_do_create( struct inode * dir, struct dentry * dentry, umode_t mode)
{
    struct inode * inode;
    struct super_block * sb;
    struct dir_entry * entry;
    struct file_blk * blk, *pblk;
    int idx;
    sb = dir->i_sb;
    if (strlen(dentry->d_name.name) > MAXLEN-1){
        return -E2BIG;
    }
    if (curr_count >= MAX_FILES)
    {
        return -ENOSPC;
    }
    if (!S_ISDIR(mode) && !S_ISREG(mode) && !S_ISLNK(mode))
    {
        return -EINVAL;
    }
    inode = new_inode(sb);
    if (!inode)
    {
        return -ENOMEM;
    }
    inode->i_sb = sb;
    if (S_ISDIR(mode)) {
        inode->i_op = &tinyfs_inode_ops;
    }
    if (S_ISREG(mode)) {
        inode->i_op = &tinyfs_inode_ops;
    }
    if (S_ISLNK(mode))
        inode->i_op = &tinyfs_symlink_inode_ops;
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    idx = get_block();
    // 获取一个空闲的文件块保存新文件
    blk = &block[idx];
    inode->i_ino = idx;
    blk->mode = mode;
    curr_count++;
    // TODO
    if(S_ISDIR(mode))
    {
        blk->dir_children =0;
        inode->i_fop = &tinyfs_dir_operations;
    }
    else if(S_ISREG(mode))
    {
        blk->file_size =0;
        inode->i_fop = &tinyfs_file_operations;
    }
    inode->i_private = blk;
    pblk =(struct file_blk *)dir->i_private;
    entry = (struct dir_entry *)&pblk->data[0];
    entry += pblk->dir_children;
    pblk->dir_children++;
    entry->idx = idx;
    strcpy(entry->filename, dentry->d_name.name);
    // VFS穿针引线的关键步骤，将VFS的inode链接到链表
    inode_init_owner(inode, dir, mode);
    d_add(dentry, inode);
    return 0;
}

static int tinyfs_mkdir(struct inode * dir, struct dentry * dentry, umode_t mode)
{
    return tinyfs_do_create(dir, dentry, S_IFDIR|mode);
}

static int tinyfs_create(struct inode * dir, struct dentry * dentry, umode_t mode, bool excl)
{
    return tinyfs_do_create(dir, dentry, mode);
}

static struct inode * tinyfs_iget(struct super_block * sb, int idx)
{
    struct inode * inode;
    struct file_blk * blk;
    inode = new_inode(sb);
    inode -> i_ino = idx;
    inode -> i_sb = sb;
    if (S_ISLNK(inode->i_mode))
        inode -> i_op = &tinyfs_symlink_inode_ops;
    else
        inode -> i_op = &tinyfs_inode_ops;
    
    blk = &block[idx];
    if(S_ISDIR(blk->mode))
        inode->i_fop = &tinyfs_dir_operations;
    else
        if(S_ISREG(blk->mode))
            inode->i_fop = &tinyfs_file_operations;
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    inode->i_private = blk;
    return inode;
}

struct dentry* tinyfs_lookup(struct inode * parent_inode, struct dentry * child_dentry, unsigned int flags)
{
    struct super_block * sb = parent_inode ->i_sb;
    struct file_blk * blk;
    struct dir_entry * entry;
    int i;
    blk = (struct file_blk *) parent_inode->i_private;
    entry = (struct dir_entry *)&blk->data[0];
    for ( i = 0; i < blk->dir_children; i++)
    {
        if (!strcmp(entry[i].filename, child_dentry->d_name.name))
        {
            struct inode * inode = tinyfs_iget(sb, entry[i].idx);
            struct file_blk * inner = (struct file_blk *)inode->i_private;
            inode_init_owner(inode, parent_inode, inner->mode);
            d_add(child_dentry, inode);
            return NULL;
        }
    }
    return NULL;
}

int tinyfs_rmdir(struct inode * dir, struct dentry * dentry)
{
    struct inode * inode = dentry->d_inode;
    struct file_blk * blk = (struct file_blk *)inode->i_private;
    blk->busy = 0;
    return simple_rmdir(dir, dentry);
}

static int tinyfs_link (struct dentry * old_dentry, struct inode * dir, struct dentry *dentry) {
    struct inode *inode = d_inode(old_dentry);
    struct file_blk * pblk;
    int idx;
    struct dir_entry * entry;

    pblk =(struct file_blk *)dir->i_private;
    entry = (struct dir_entry *)&pblk->data[0];
    entry += pblk->dir_children;
    pblk->dir_children++;
    idx = inode->i_ino;
    entry->idx = idx;
    strcpy(entry->filename, dentry->d_name.name);
    //d_add(dentry, inode);
    return 0;
}


int tinyfs_unlink(struct inode * dir, struct dentry * dentry)
{
    int i;
    struct inode * inode = dentry->d_inode;
    struct file_blk * blk = (struct file_blk *)inode->i_private;
    struct file_blk * pblk = (struct file_blk *)dir->i_private;
    struct dir_entry * entry;
    // 更新其上层目录
    entry = (struct dir_entry *)&pblk->data[0];
    for (i = 0; i < pblk->dir_children; i++)
    {
        if (!strcmp(entry[i].filename,dentry->d_name.name))
        {
            int j;
            for (j = i; j < pblk->dir_children - 1; j++)
            {
                memcpy(&entry[j] , &entry[j +1], sizeof(struct dir_entry));
            }
            pblk->dir_children--;
            break;
        }
    }
    blk->busy = 0;
    curr_count--;
    return simple_unlink(dir, dentry);
}

static int tinyfs_symlink (struct inode * dir, struct dentry * dentry, const char * symname) {
    int ret, i;
    struct file_blk * blk;
    char * buffer;
    int pos = 0;
    int inum = 0;
    ret = tinyfs_do_create(dir, dentry, S_IFLNK|0544);
    if (ret)
        return ret;
    blk = dir->i_private;
    buffer = (char *)&blk->data[0];
    for(i = 0; i < blk->dir_children; i++)
    {
        // Attention that the function `printk` is line buffer!
        if (strcmp(((struct dir_entry *)(buffer+pos))->filename, dentry->d_name.name) == 0) {
            inum = ((struct dir_entry *)(buffer+pos))->idx;
            break;
        }
        pos += sizeof(struct dir_entry);
    }
    if (! inum) {
        return -EFAULT;
    }
    strcpy((char*)(&block[inum].data[0]), symname);
    block[inum].file_size = strlen(symname);
    return 0;
}

// still have bug when use vim
static int tinyfs_rename (struct inode * old_dir, struct dentry * old_dentry,
            struct inode * new_dir, struct dentry * new_dentry,
            unsigned int flags) {
    int i, err, idx=-1;
    struct file_blk* old_dir_block, *new_dir_block;
    struct dir_entry * entry;
    struct inode * inode = old_dentry->d_inode;
    old_dir_block = (struct file_blk*)old_dir->i_private;
    new_dir_block = (struct file_blk*)new_dir->i_private;

    // 更新其上层目录
    entry = (struct dir_entry *)old_dir_block->data;
    for (i = 0; i < old_dir_block->dir_children; i++)
    {
        if (!strcmp(entry[i].filename, old_dentry->d_name.name))
        {
            int j;
            idx = entry[i].idx;
            for (j = i; j < old_dir_block->dir_children - 1; j++)
            {
                memcpy(&entry[j] , &entry[j +1], sizeof(struct dir_entry));
            }
            old_dir_block->dir_children--;
            break;
        }
    }
    err = simple_unlink(old_dir, old_dentry);
    if (err) {
        return err;
    }

    entry = (struct dir_entry *)new_dir_block->data;
    entry += (new_dir_block->dir_children);
    new_dir_block->dir_children += 1;
    if (idx!=-1)
        entry->idx = idx;
    strcpy(entry->filename, new_dentry->d_name.name);
    //new_dentry->d_inode = inode;
    return simple_link(old_dentry, new_dir, new_dentry);
    //d_add(dentry, inode);
}

// 是不是要把file与dir的inode_operations区分开
static struct inode_operations tinyfs_inode_ops = {
    .create = tinyfs_create,
    .lookup = tinyfs_lookup,
    .mkdir = tinyfs_mkdir,
    .rmdir = tinyfs_rmdir,
    .unlink = tinyfs_unlink,
    .link = tinyfs_link,
    .symlink = tinyfs_symlink,
    .rename = tinyfs_rename
};

int tinyfs_fill_super(struct super_block * sb, void * data, int silent)
{
    struct inode * root_inode;
    int mode = S_IFDIR;
    root_inode = new_inode(sb);
    root_inode->i_ino = 1;
    inode_init_owner(root_inode, NULL, mode);
    root_inode ->i_sb = sb;
    root_inode ->i_op = &tinyfs_inode_ops;
    root_inode ->i_fop =  &tinyfs_dir_operations;
    root_inode->i_atime =root_inode->i_mtime =root_inode->i_ctime =CURRENT_TIME;
    block[1].mode = mode;
    block[1].dir_children =0;
    block[1].idx =1;
    block[1].busy = 1;
    root_inode ->i_private = &block[1];
    sb->s_root = d_make_root( root_inode);
    curr_count++;
    return 0;
}

static struct dentry * tinyfs_mount(struct file_system_type * fs_type, int flags, const char * dev_name, void * data)
{
    return mount_nodev(fs_type, flags, data, tinyfs_fill_super);
}

static void tinyfs_kill_superblock( struct super_block * sb)
{
    kill_anon_super(sb);
}

struct file_system_type tinyfs_fs_type = {
    .owner = THIS_MODULE,
    .name = "tinyfs",
    .mount = tinyfs_mount,
    .kill_sb = tinyfs_kill_superblock,
};

static int tinyfs_init(void)
{
    int ret;
    memset(block, 0, sizeof(block));
    ret = register_filesystem(&tinyfs_fs_type);
    if (ret)
        printk( "register tinyfs failed\n");
    return ret;
}

static void tinyfs_exit( void)
{
    unregister_filesystem(&tinyfs_fs_type);
}

module_init( tinyfs_init);
module_exit(tinyfs_exit);
MODULE_LICENSE("GPL");
