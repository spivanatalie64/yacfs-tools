# YAcFS Tools

Management tools for [YAcFS](https://github.com/spivanatalie64/spivanatalie64.github.io) pools: garbage collection, data scrubbing, snapshot management.

## Tools

| Tool | Description |
|---|---|
| `yacfs-gc` | Garbage collection — removes orphaned block files not referenced by any inode |
| `yacfs-scrub` | Data integrity scrub — walks all files, reads every byte, verifies checksums |
| `yacfs-ctl` | Pool control — status, snapshot management, rollback |

## Install

```bash
make && sudo make install
```

## Usage

### Garbage Collection

```bash
# Remove orphaned blocks from a pool
yacfs-gc /data/pool
```

Orphaned blocks occur when files are deleted but their block files remain. Run this periodically to reclaim space.

### Data Scrubbing

```bash
# Scrub a mounted YAcFS filesystem
yacfs-scrub /mnt/yacfs

# Scrub a single file
yacfs-scrub /mnt/yacfs/important.db
```

Scrub reads every byte through the FUSE mount, triggering YAcFS's on-read checksum verification. Corrupted blocks produce `-EIO` errors and are reported.

### Pool Control

```bash
# Pool status
yacfs-ctl /data/pool status

# List snapshots
yacfs-ctl /data/pool snapshots

# Create snapshot
yacfs-ctl /data/pool snapshot before-update

# Rollback to snapshot
yacfs-ctl /data/pool rollback before-update
```

## Protocol

YAcFS Tools operate directly on the pool directory structure. No running daemon or socket required.

```
<pool_root>/
├── blocks/        # Scanned by yacfs-gc for orphan detection
├── meta/          # Read by yacfs-ctl for snapshot management
└── .snapshots/    # Managed by yacfs-ctl create/rollback
```

## Building

```bash
make
sudo make install
```

Dependencies: `libxxhash`, `libzstd`, POSIX libc.

## CI/CD

See [.github/workflows](.github/workflows/) for the build and test pipeline.
---

## 🤖 Pullfrog AI Review

This repository uses **Pullfrog AI** to automatically review pull requests.

Pullfrog is an AI-powered code review agent that analyzes every PR for code quality,
security issues, performance problems, and best practice violations. Reviews appear
as inline PR comments and checks. Trigger manually by commenting `@pullfrog` on any PR.

Powered by OpenRouter.