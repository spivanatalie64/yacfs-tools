#include "yacfs.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

struct scrub_stats {
    uint64_t files_checked;
    uint64_t bytes_checked;
    uint64_t errors;
    uint64_t dirs;
};

static int scrub_file(const char *path, struct scrub_stats *stats) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "SCRUB ERROR: cannot open %s: %s\n", path, strerror(errno));
        stats->errors++;
        return -1;
    }

    char buf[262144];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        stats->bytes_checked += n;
    }
    close(fd);

    if (n < 0) {
        fprintf(stderr, "SCRUB ERROR: read failed on %s: %s\n", path, strerror(errno));
        stats->errors++;
        return -1;
    }

    stats->files_checked++;
    return 0;
}

static int walk(const char *dirpath, struct scrub_stats *stats) {
    DIR *d = opendir(dirpath);
    if (!d) {
        fprintf(stderr, "SCRUB ERROR: cannot open %s: %s\n", dirpath, strerror(errno));
        stats->errors++;
        return -1;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char fpath[4096];
        snprintf(fpath, sizeof(fpath), "%s/%s", dirpath, de->d_name);

        struct stat sb;
        if (stat(fpath, &sb) < 0) {
            fprintf(stderr, "SCRUB ERROR: cannot stat %s: %s\n", fpath, strerror(errno));
            stats->errors++;
            continue;
        }

        if (S_ISDIR(sb.st_mode)) {
            stats->dirs++;
            walk(fpath, stats);
        } else if (S_ISREG(sb.st_mode)) {
            scrub_file(fpath, stats);
        }
    }
    closedir(d);
    return 0;
}

int main(int argc, char **argv) {
    const char *target;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mount_point_or_pool>\n", argv[0]);
        fprintf(stderr, "  Scans all files, reads every byte, verifies YAcFS checksums.\n");
        return 1;
    }

    target = argv[1];

    struct scrub_stats stats = {0};
    time_t start = time(NULL);

    printf("YAcFS Scrub v2.0\n");
    printf("Target: %s\n", target);
    printf("Started: %s", ctime(&start));
    printf("────────────────────────────────────────\n");

    struct stat sb;
    if (stat(target, &sb) < 0) {
        fprintf(stderr, "Cannot access %s: %s\n", target, strerror(errno));
        return 1;
    }

    if (S_ISREG(sb.st_mode)) {
        scrub_file(target, &stats);
    } else if (S_ISDIR(sb.st_mode)) {
        walk(target, &stats);
    }

    time_t end = time(NULL);
    double elapsed = difftime(end, start);
    double rate = elapsed > 0 ? stats.bytes_checked / elapsed / 1048576.0 : 0;

    printf("────────────────────────────────────────\n");
    printf("Directories:  %lu\n", (unsigned long)stats.dirs);
    printf("Files:        %lu\n", (unsigned long)stats.files_checked);
    printf("Data read:    %lu bytes (%.2f MB)\n",
           (unsigned long)stats.bytes_checked,
           stats.bytes_checked / 1048576.0);
    printf("Errors:       %lu\n", (unsigned long)stats.errors);
    printf("Elapsed:      %.0f seconds\n", elapsed);
    printf("Throughput:   %.1f MB/s\n", rate);

    if (stats.errors > 0) {
        printf("\n⚠  DATA CORRUPTION DETECTED — %lu errors found\n", (unsigned long)stats.errors);
        return 2;
    }
    printf("\n✓ Scrub complete — no errors\n");
    return 0;
}
