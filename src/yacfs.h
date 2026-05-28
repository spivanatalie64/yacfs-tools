#ifndef YACFS_TOOLS_H
#define YACFS_TOOLS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLOCK_HDR_MAGIC 0x5A4653
#define BLOCK_SIZE      65536

/* Standard exit codes */
#define EXIT_OK          0
#define EXIT_USAGE       1
#define EXIT_POOL_INVALID 2
#define EXIT_IO_ERR      3
#define EXIT_CORRUPTION  4

struct block_hdr {
    uint32_t magic;
    uint32_t checksum;
    uint32_t orig_size;
    uint32_t comp_size;
    uint8_t  compress;
    uint8_t  checksum_type;
    uint16_t _pad;
} __attribute__((packed));

struct yacfs_inode {
    uint64_t    ino;
    uint64_t    size;
    uint64_t    mtime;
    uint64_t    ctime;
    uint32_t    mode;
    uint32_t    uid;
    uint32_t    gid;
    uint32_t    nblocks;
    uint32_t    block_size;
    uint32_t    checksum_type;
    uint32_t    compress_type;
};

/* Validate that pool is a directory with required subdirectories.
 * Returns 0 on success, -1 on failure (message written to errbuf). */
static inline int validate_pool(const char *pool, char *errbuf, size_t errbuf_size) {
    struct stat sb;
    if (stat(pool, &sb) < 0) {
        snprintf(errbuf, errbuf_size, "Pool not found: %s", pool);
        return -1;
    }
    if (!S_ISDIR(sb.st_mode)) {
        snprintf(errbuf, errbuf_size, "Not a directory: %s", pool);
        return -1;
    }

    char path[4096];
    const char *subdirs[] = { "meta", "blocks", ".snapshots" };
    for (size_t i = 0; i < sizeof(subdirs)/sizeof(subdirs[0]); i++) {
        snprintf(path, sizeof(path), "%s/%s", pool, subdirs[i]);
        if (stat(path, &sb) < 0 || !S_ISDIR(sb.st_mode)) {
            snprintf(errbuf, errbuf_size, "Invalid pool '%s': missing %s/ directory",
                     pool, subdirs[i]);
            return -1;
        }
    }
    return 0;
}

/* Format bytes into human-readable string (e.g. 1.5 MiB) */
static inline void fmt_bytes(uint64_t bytes, char *buf, size_t len) {
    const char *units[] = { "B", "KiB", "MiB", "GiB", "TiB" };
    double val = (double)bytes;
    int unit = 0;
    while (val >= 1024.0 && unit < 4) {
        val /= 1024.0;
        unit++;
    }
    snprintf(buf, len, "%.2f %s", val, units[unit]);
}

#endif
