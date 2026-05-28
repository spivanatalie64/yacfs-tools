#include "yacfs.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int hash_from_name(const char *name, uint64_t *hash) {
    char *dot = strchr(name, '.');
    if (!dot || strcmp(dot, ".blk") != 0) return -1;
    char buf[17];
    size_t len = dot - name;
    if (len != 16) return -1;
    memcpy(buf, name, 16);
    buf[16] = 0;
    *hash = strtoull(buf, NULL, 16);
    return 0;
}

static int read_all_blocks(const char *blkdir, uint64_t **blocks, int *nblocks) {
    DIR *d = opendir(blkdir);
    if (!d) return -errno;

    size_t cap = 1024;
    *blocks = malloc(cap * sizeof(uint64_t));
    int n = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        uint64_t h;
        if (hash_from_name(de->d_name, &h) < 0) continue;
        if (n >= (int)cap) {
            cap *= 2;
            *blocks = realloc(*blocks, cap * sizeof(uint64_t));
        }
        (*blocks)[n++] = h;
    }
    closedir(d);
    *nblocks = n;
    return 0;
}

static int read_meta_blocks(const char *metapath, uint64_t **blocks, int *nblocks) {
    DIR *d = opendir(metapath);
    if (!d) return -errno;

    size_t cap = 1024;
    *blocks = malloc(cap * sizeof(uint64_t));
    int n = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char *dot = strrchr(de->d_name, '.');
        if (!dot || strcmp(dot, ".ino") != 0) continue;

        char fpath[4096];
        snprintf(fpath, sizeof(fpath), "%s/%s", metapath, de->d_name);

        int fd = open(fpath, O_RDONLY);
        if (fd < 0) continue;

        struct yacfs_inode in;
        if (read(fd, &in, sizeof(in)) != sizeof(in)) { close(fd); continue; }

        for (uint32_t i = 0; i < in.nblocks; i++) {
            uint64_t h;
            if (read(fd, &h, sizeof(h)) != sizeof(h)) break;
            if (n >= (int)cap) {
                cap *= 2;
                *blocks = realloc(*blocks, cap * sizeof(uint64_t));
            }
            (*blocks)[n++] = h;
        }
        close(fd);
    }
    closedir(d);
    *nblocks = n;
    return 0;
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t*)a;
    uint64_t y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pool_root>\n", argv[0]);
        fprintf(stderr, "Removes orphaned block files not referenced by any inode.\n");
        return 1;
    }

    char metadir[4096], blkdir[4096];
    snprintf(metadir, sizeof(metadir), "%s/meta", argv[1]);
    snprintf(blkdir, sizeof(blkdir), "%s/blocks", argv[1]);

    struct stat sb;
    if (stat(metadir, &sb) < 0 || stat(blkdir, &sb) < 0) {
        fprintf(stderr, "Invalid pool: %s (missing meta/ or blocks/)\n", argv[1]);
        return 1;
    }

    uint64_t *meta_blocks = NULL, *blk_blocks = NULL;
    int nmeta = 0, nblk = 0;

    if (read_meta_blocks(metadir, &meta_blocks, &nmeta) < 0) {
        fprintf(stderr, "Failed to scan meta/\n");
        return 1;
    }
    if (read_all_blocks(blkdir, &blk_blocks, &nblk) < 0) {
        fprintf(stderr, "Failed to scan blocks/\n");
        free(meta_blocks);
        return 1;
    }

    qsort(meta_blocks, nmeta, sizeof(uint64_t), cmp_u64);

    int orphaned = 0, kept = 0;
    for (int i = 0; i < nblk; i++) {
        int found = 0;
        if (bsearch(&blk_blocks[i], meta_blocks, nmeta, sizeof(uint64_t), cmp_u64))
            found = 1;

        char fname[64], fpath[4096];
        snprintf(fname, sizeof(fname), "%016lx.blk", blk_blocks[i]);
        snprintf(fpath, sizeof(fpath), "%s/%s", blkdir, fname);

        if (found) {
            kept++;
        } else {
            if (unlink(fpath) == 0) {
                printf("Removed orphan: %s\n", fname);
                orphaned++;
            } else {
                fprintf(stderr, "Failed to remove: %s\n", fname);
            }
        }
    }

    printf("\nSummary: %d blocks kept, %d orphaned and removed\n", kept, orphaned);
    free(meta_blocks);
    free(blk_blocks);
    return 0;
}
