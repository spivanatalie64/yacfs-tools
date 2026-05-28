/*
 * yacfs-snap-rotate — Automated snapshot rotation
 *
 * Usage: yacfs-snap-rotate <pool_root> <keep>
 *
 * Keeps the <keep> most recent snapshots, deletes the rest.
 * Snapshot age is determined by directory mtime.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

struct snap_entry {
    char name[256];
    time_t mtime;
};

static int cmp_snap(const void *a, const void *b) {
    const struct snap_entry *sa = (const struct snap_entry *)a;
    const struct snap_entry *sb = (const struct snap_entry *)b;
    if (sa->mtime < sb->mtime) return 1;
    if (sa->mtime > sb->mtime) return -1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <pool_root> <keep>\n", argv[0]);
        fprintf(stderr, "Keeps the <keep> most recent snapshots, removes older ones.\n");
        return 1;
    }

    const char *pool = argv[1];
    int keep = atoi(argv[2]);
    if (keep < 1) { fprintf(stderr, "Keep must be >= 1\n"); return 1; }

    char snapdir[4096];
    snprintf(snapdir, sizeof(snapdir), "%s/.snapshots", pool);

    DIR *d = opendir(snapdir);
    if (!d) {
        fprintf(stderr, "Cannot open %s\n", snapdir);
        return 1;
    }

    size_t cap = 128;
    struct snap_entry *snaps = malloc(cap * sizeof(struct snap_entry));
    int n = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (n >= (int)cap) {
            cap *= 2;
            snaps = realloc(snaps, cap * sizeof(struct snap_entry));
        }
        strncpy(snaps[n].name, de->d_name, 255);

        char fpath[4096];
        snprintf(fpath, sizeof(fpath), "%s/%s", snapdir, de->d_name);
        struct stat sb;
        if (stat(fpath, &sb) == 0)
            snaps[n].mtime = sb.st_mtime;
        else
            snaps[n].mtime = 0;
        n++;
    }
    closedir(d);

    if (n <= keep) {
        printf("Only %d snapshots, keeping all (keep=%d)\n", n, keep);
        free(snaps);
        return 0;
    }

    qsort(snaps, n, sizeof(struct snap_entry), cmp_snap);

    int deleted = 0;
    for (int i = keep; i < n; i++) {
        char fpath[4096];
        snprintf(fpath, sizeof(fpath), "%s/%s", snapdir, snaps[i].name);

        // Remove all files in the snapshot directory first
        DIR *sd = opendir(fpath);
        if (sd) {
            struct dirent *se;
            while ((se = readdir(sd)) != NULL) {
                if (se->d_name[0] == '.') continue;
                char spath[4096];
                snprintf(spath, sizeof(spath), "%s/%s", fpath, se->d_name);
                unlink(spath);
            }
            closedir(sd);
        }
        rmdir(fpath);
        printf("Removed: %s\n", snaps[i].name);
        deleted++;
    }

    printf("Rotation complete: %d deleted, %d kept\n", deleted, keep);
    free(snaps);
    return 0;
}
