/*
 * yacfs-pool-create — Initialize a new YAcFS pool directory
 *
 * Usage: yacfs-pool-create <pool_root> [pool_root2 ...]
 *
 * Creates the blocks/, meta/, and .snapshots/ directories
 * and initializes the root inode.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pool_root> [pool_root2 ...]\n", argv[0]);
        fprintf(stderr, "Creates YAcFS pool directories and initializes root inode.\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        const char *pool = argv[i];

        struct stat sb;
        if (stat(pool, &sb) == 0) {
            fprintf(stderr, "Warning: %s already exists, checking structure...\n", pool);
        } else {
            if (mkdir(pool, 0755) < 0) {
                fprintf(stderr, "Cannot create pool %s: %s\n", pool, strerror(errno));
                return 1;
            }
        }

        char path[4096];
        snprintf(path, sizeof(path), "%s/blocks", pool);
        mkdir(path, 0755);

        snprintf(path, sizeof(path), "%s/meta", pool);
        mkdir(path, 0755);

        snprintf(path, sizeof(path), "%s/.snapshots", pool);
        mkdir(path, 0755);

        // Initialize root inode
        snprintf(path, sizeof(path), "%s/meta/00000000000000000001.ino", pool);
        if (stat(path, &sb) < 0) {
            struct {
                uint64_t ino;
                uint64_t size;
                uint64_t mtime;
                uint64_t ctime;
                uint32_t mode;
                uint32_t uid;
                uint32_t gid;
                uint32_t nblocks;
                uint32_t block_size;
                uint32_t checksum_type;
                uint32_t compress_type;
            } root_inode = {
                .ino = 1,
                .size = 0,
                .mtime = time(NULL),
                .ctime = time(NULL),
                .mode = S_IFDIR | 0755,
                .uid = 0,
                .gid = 0,
                .nblocks = 0,
                .block_size = 65536,
                .checksum_type = 1,
                .compress_type = 1,
            };

            int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
            if (fd < 0) {
                fprintf(stderr, "Cannot create root inode: %s\n", strerror(errno));
                return 1;
            }
            write(fd, &root_inode, sizeof(root_inode));
            close(fd);
        }

        printf("Pool initialized: %s\n", pool);
        printf("  blocks/     — block storage\n");
        printf("  meta/       — inode metadata\n");
        printf("  .snapshots/ — snapshot storage\n");
    }

    return 0;
}
