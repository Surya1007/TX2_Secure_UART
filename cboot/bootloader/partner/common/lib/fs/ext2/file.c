/*
 * Copyright (c) 2007 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <debug.h>
#include <trace.h>
#include <ext2_priv.h>
#include <ext2_dinode.h>

#define LOCAL_TRACE 0

int ext2_open_file(fscookie *cookie, const char *path, filecookie **fcookie)
{
    ext2_t *ext2 = (ext2_t *)cookie;
    int err;

    /* do a path lookup */
    inodenum_t inum;
    err = ext2_lookup(ext2, path, &inum);
    if (err < 0)
        return err;

    /* create the file object */
    ext2_file_t *file = malloc(sizeof(ext2_file_t));
    if (file == NULL) {
        TRACEF("Failed to allocate memory for ext2 file object\n");
        return ERR_NO_MEMORY;
    }
    memset(file, 0, sizeof(ext2_file_t));

    /* read in the inode */
    err = ext2_load_inode(ext2, inum, &file->inode);
    if (err < 0) {
        free(file);
        return err;
    }

    file->ext2 = ext2;
    *fcookie = (filecookie *)file;

    return 0;
}

ssize_t ext2_read_file(filecookie *fcookie, void *buf, off_t offset, size_t len)
{
    ext2_file_t *file = (ext2_file_t *)fcookie;
    int err;

    if (len == 0) {
        TRACEF("File length cannot be 0\n");
        return -1;
    }

    // test that it's a file
    if (!S_ISREG(file->inode.e2di_mode)) {
        dprintf(INFO, "ext2_read_file: not a file, mode: 0x%04x\n", file->inode.e2di_mode);
        return -1;
    }

    // read from the inode
    err = ext2_read_inode(file->ext2, &file->inode, buf, offset, len);

    return err;
}

int ext2_close_file(filecookie *fcookie)
{
    ext2_file_t *file = (ext2_file_t *)fcookie;

    // see if we need to free any of the cache blocks
    int i;
    for (i=0; i < 3; i++) {
        if (file->ind_cache[i].num != 0) {
            free(file->ind_cache[i].ptr);
        }
    }

    free(file);

    return 0;
}

off_t ext2_file_len(ext2_t *ext2, struct ext2fs_dinode *inode)
{
    /* calculate the file size */
    off_t len = inode->e2di_size;
    if ((ext2->super_blk.e2fs_features_rocompat & EXT2F_ROCOMPAT_LARGEFILE) && (S_ISREG(inode->e2di_mode))) {
        /* can potentially be a large file */
        len |= (off_t)inode->e2di_size_high << 32;
    }

    return len;
}

int ext2_stat_file(filecookie *fcookie, struct file_stat *stat)
{
    ext2_file_t *file = (ext2_file_t *)fcookie;

    stat->size = ext2_file_len(file->ext2, &file->inode);

    /* is it a dir? */
    stat->is_dir = false;
    if (S_ISDIR(file->inode.e2di_mode))
        stat->is_dir = true;

    return 0;
}

int ext2_read_link(ext2_t *ext2, struct ext2fs_dinode *inode, char *str, size_t len)
{
    LTRACEF("inode %p, str %p, len %zu\n", inode, str, len);

    off_t linklen = ext2_file_len(ext2, inode);

    if ((linklen == 0) || (linklen + 1 > (off_t)len))
        return ERR_NO_MEMORY;

    if (linklen > 60) {
        int err = ext2_read_inode(ext2, inode, str, 0, linklen);
        if (err < 0)
            return err;
        str[linklen] = 0;
    } else {
        memcpy(str, &inode->e2di_blocks[0], linklen);
        str[linklen] = 0;
    }

    LTRACEF("read link '%s'\n", str);

    return linklen;
}

