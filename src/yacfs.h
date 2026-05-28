#ifndef YACFS_TOOLS_H
#define YACFS_TOOLS_H

#include <stdint.h>
#include <stdio.h>

#define BLOCK_HDR_MAGIC 0x5A4653
#define BLOCK_SIZE      65536

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

#endif
