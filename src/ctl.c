#include "yacfs.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int cmd_export(const char *pool);
static int cmd_import(const char *pool);

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <pool_root> <command> [args]\n"
        "\nCommands:\n"
        "  status                  Pool status and statistics\n"
        "  snapshots               List all snapshots\n"
        "  snapshot <name>         Create a snapshot\n"
        "  rollback <name>         Rollback to a snapshot\n"
        "  export                  Export (lock) pool for safe detachment\n"
        "  import                  Import (unlock) pool for mounting\n"
        "  info                    Detailed pool information\n"
        "\nExamples:\n"
        "  %s /data/pool status\n"
        "  %s /data/pool snapshot before-update\n"
        "  %s /data/pool snapshots\n",
        prog, prog, prog, prog);
}

static int cmd_status(const char *pool) {
    char path[4096];
    struct stat sb;

    snprintf(path, sizeof(path), "%s/meta", pool);
    int nmeta = 0;
    if (stat(path, &sb) == 0) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (strstr(de->d_name, ".ino")) nmeta++;
            }
            closedir(d);
        }
    }

    snprintf(path, sizeof(path), "%s/blocks", pool);
    int nblocks = 0;
    if (stat(path, &sb) == 0) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (strstr(de->d_name, ".blk")) nblocks++;
            }
            closedir(d);
        }
    }

    snprintf(path, sizeof(path), "%s/.snapshots", pool);
    int nsnaps = 0;
    if (stat(path, &sb) == 0) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (de->d_name[0] != '.') nsnaps++;
            }
            closedir(d);
        }
    }

    snprintf(path, sizeof(path), "%s/blocks", pool);
    uint64_t blk_bytes = 0;
    if (stat(path, &sb) == 0) blk_bytes = sb.st_size;

    uint64_t meta_bytes = 0;
    snprintf(path, sizeof(path), "%s/meta", pool);
    if (stat(path, &sb) == 0) meta_bytes = sb.st_size;

    printf("YAcFS Pool Status: %s\n", pool);
    printf("  Inodes:   %d\n", nmeta);
    printf("  Blocks:   %d\n", nblocks);
    printf("  Snapshots: %d\n", nsnaps);
    printf("  Block storage: %lu bytes\n", (unsigned long)blk_bytes);
    printf("  Metadata:     %lu bytes\n", (unsigned long)meta_bytes);
    return 0;
}

static int cmd_snapshots(const char *pool) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/.snapshots", pool);

    DIR *d = opendir(path);
    if (!d) {
        if (errno == ENOENT) {
            printf("No snapshots directory (no snapshots created yet)\n");
            return 0;
        }
        fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
        return 1;
    }

    printf("Snapshots in %s:\n", pool);
    struct dirent *de;
    int n = 0;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char spath[4096];
        snprintf(spath, sizeof(spath), "%s/%s", path, de->d_name);
        struct stat sb;
        if (stat(spath, &sb) == 0) {
            int nf = 0;
            DIR *sd = opendir(spath);
            if (sd) {
                struct dirent *se;
                while ((se = readdir(sd)) != NULL) {
                    if (strstr(se->d_name, ".ino")) nf++;
                }
                closedir(sd);
            }
            printf("  %-30s %lu inodes  %s",
                   de->d_name, (unsigned long)nf, ctime(&sb.st_mtime));
        }
        n++;
    }
    closedir(d);
    if (n == 0) printf("  (none)\n");
    printf("Total: %d snapshot(s)\n", n);
    return 0;
}

static int cmd_snapshot(const char *pool, const char *name) {
    char src[4096], dst[4096];
    snprintf(src, sizeof(src), "%s/meta", pool);
    snprintf(dst, sizeof(dst), "%s/.snapshots/%s", pool, name);

    struct stat sb;
    if (stat(dst, &sb) == 0) {
        fprintf(stderr, "Snapshot '%s' already exists\n", name);
        return 1;
    }

    if (mkdir(dst, 0755) < 0) {
        fprintf(stderr, "Cannot create snapshot directory: %s\n", strerror(errno));
        return 1;
    }

    DIR *d = opendir(src);
    if (!d) { rmdir(dst); fprintf(stderr, "Cannot open meta: %s\n", strerror(errno)); return 1; }

    int copied = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sf[4096], df[4096];
        snprintf(sf, sizeof(sf), "%s/%s", src, de->d_name);
        snprintf(df, sizeof(df), "%s/%s", dst, de->d_name);

        int sfd = open(sf, O_RDONLY);
        if (sfd < 0) continue;
        int dfd = open(df, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (dfd < 0) { close(sfd); continue; }

        char buf[262144];
        ssize_t n;
        while ((n = read(sfd, buf, sizeof(buf))) > 0) {
            ssize_t w = 0;
            while (w < n) { ssize_t r = write(dfd, buf + w, n - w); if (r <= 0) break; w += r; }
        }
        close(sfd);
        close(dfd);
        copied++;
    }
    closedir(d);

    printf("Snapshot '%s' created (%d inodes)\n", name, copied);
    return 0;
}

static int cmd_rollback(const char *pool, const char *name) {
    char snapdir[4096], metadir[4096], tmpdir[4096];
    snprintf(metadir, sizeof(metadir), "%s/meta", pool);
    snprintf(snapdir, sizeof(snapdir), "%s/.snapshots/%s", pool, name);
    snprintf(tmpdir, sizeof(tmpdir), "%s/meta.rollback", pool);

    struct stat sb;
    if (stat(snapdir, &sb) < 0) {
        fprintf(stderr, "Snapshot '%s' not found\n", name);
        return 1;
    }

    printf("Rolling back to snapshot '%s'...\n", name);
    printf("Current meta will be saved as a backup.\n");

    if (rename(metadir, tmpdir) < 0) {
        fprintf(stderr, "Failed to backup current meta: %s\n", strerror(errno));
        return 1;
    }

    if (rename(snapdir, metadir) < 0) {
        rename(tmpdir, metadir);
        fprintf(stderr, "Failed to restore snapshot: %s\n", strerror(errno));
        return 1;
    }

    char backup[4096];
    snprintf(backup, sizeof(backup), "%s/.snapshots/rollback-%s", pool, name);
    rename(tmpdir, backup);

    printf("Rollback complete. Previous state saved as 'rollback-%s'\n", name);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) { usage(argv[0]); return 1; }

    const char *pool = argv[1];
    const char *cmd = argv[2];

    struct stat sb;
    if (stat(pool, &sb) < 0) {
        fprintf(stderr, "Pool not found: %s\n", pool);
        return 1;
    }

    if (strcmp(cmd, "status") == 0 || strcmp(cmd, "info") == 0)
        return cmd_status(pool);
    else if (strcmp(cmd, "snapshots") == 0 || strcmp(cmd, "list") == 0)
        return cmd_snapshots(pool);
    else if (strcmp(cmd, "snapshot") == 0 || strcmp(cmd, "create") == 0) {
        if (argc < 4) { fprintf(stderr, "Missing snapshot name\n"); return 1; }
        return cmd_snapshot(pool, argv[3]);
    } else if (strcmp(cmd, "rollback") == 0) {
        if (argc < 4) { fprintf(stderr, "Missing snapshot name\n"); return 1; }
        return cmd_rollback(pool, argv[3]);
    } else if (strcmp(cmd, "export") == 0) {
        return cmd_export(pool);
    } else if (strcmp(cmd, "import") == 0) {
        return cmd_import(pool);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(argv[0]);
        return 1;
    }
}

static int cmd_export(const char *pool) {
    char lockfile[4096];
    snprintf(lockfile, sizeof(lockfile), "%s/.yacfs-lock", pool);
    struct stat sb;
    if (stat(lockfile, &sb) == 0) {
        fprintf(stderr, "Pool %s is locked (already exported or in use)\n", pool);
        return 1;
    }
    int fd = open(lockfile, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        fprintf(stderr, "Cannot lock pool: %s\n", strerror(errno));
        return 1;
    }
    dprintf(fd, "YAcFS Pool Export\n");
    dprintf(fd, "Exported at: %lu\n", (unsigned long)time(NULL));
    dprintf(fd, "Pool: %s\n", pool);
    close(fd);
    printf("Pool exported: %s\n", pool);
    printf("Lock file: %s\n", lockfile);
    printf("To import: yacfs-ctl %s import\n", pool);
    return 0;
}

static int cmd_import(const char *pool) {
    char lockfile[4096];
    snprintf(lockfile, sizeof(lockfile), "%s/.yacfs-lock", pool);
    if (unlink(lockfile) < 0) {
        if (errno == ENOENT) {
            printf("Pool %s is already unlocked (no lock file)\n", pool);
            return 0;
        }
        fprintf(stderr, "Cannot unlock pool: %s\n", strerror(errno));
        return 1;
    }
    printf("Pool imported: %s\n", pool);
    printf("Lock file removed. Pool is ready for mounting.\n");
    return 0;
}
