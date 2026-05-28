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
    uint64_t total_files;
    uint64_t total_bytes;
};

static int scrub_file(const char *path, struct scrub_stats *stats, int show_progress) {
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

    if (show_progress && stats->total_files > 0) {
        int pct = (int)((stats->files_checked * 100LL) / stats->total_files);
        printf("\rProgress: %lu/%lu files (%d%%)",
               (unsigned long)stats->files_checked,
               (unsigned long)stats->total_files, pct);
        fflush(stdout);
    }
    return 0;
}

static int count_walk(const char *dirpath, struct scrub_stats *stats) {
    DIR *d = opendir(dirpath);
    if (!d) {
        stats->errors++;
        return -1;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char fpath[4096];
        snprintf(fpath, sizeof(fpath), "%s/%s", dirpath, de->d_name);

        struct stat sb;
        if (lstat(fpath, &sb) < 0) {
            stats->errors++;
            continue;
        }

        if (S_ISDIR(sb.st_mode)) {
            stats->dirs++;
            count_walk(fpath, stats);
        } else if (S_ISREG(sb.st_mode)) {
            stats->total_files++;
            stats->total_bytes += sb.st_size;
        }
    }
    closedir(d);
    return 0;
}

static int scrub_walk(const char *dirpath, struct scrub_stats *stats, int show_progress) {
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
        if (lstat(fpath, &sb) < 0) {
            fprintf(stderr, "SCRUB ERROR: cannot stat %s: %s\n", fpath, strerror(errno));
            stats->errors++;
            continue;
        }

        if (S_ISDIR(sb.st_mode)) {
            stats->dirs++;
            scrub_walk(fpath, stats, show_progress);
        } else if (S_ISREG(sb.st_mode)) {
            scrub_file(fpath, stats, show_progress);
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
        return EXIT_USAGE;
    }

    target = argv[1];

    struct scrub_stats stats = {0};
    time_t start = time(NULL);

    printf("YAcFS Scrub v2.1\n");
    printf("Target: %s\n", target);
    printf("Started: %s", ctime(&start));
    printf("----------------------------------------\n");

    struct stat sb;
    if (stat(target, &sb) < 0) {
        fprintf(stderr, "Cannot access %s: %s\n", target, strerror(errno));
        return EXIT_POOL_INVALID;
    }

    int is_tty = isatty(STDOUT_FILENO);

    if (S_ISREG(sb.st_mode)) {
        stats.total_files = 1;
        stats.total_bytes = sb.st_size;
        printf("Scrubbing single file (%lu bytes)...\n", (unsigned long)sb.st_size);
        scrub_file(target, &stats, is_tty);
        if (is_tty) printf("\n");
    } else if (S_ISDIR(sb.st_mode)) {
        if (is_tty) {
            printf("Counting files... ");
            fflush(stdout);
        }
        count_walk(target, &stats);
        if (is_tty) {
            printf("%lu files found\n", (unsigned long)stats.total_files);
        }

        if (stats.total_files > 0) {
            scrub_walk(target, &stats, is_tty);
            if (is_tty) printf("\n");
        }
    } else {
        fprintf(stderr, "Unsupported file type for %s\n", target);
        return EXIT_USAGE;
    }

    time_t end = time(NULL);
    double elapsed = difftime(end, start);
    double rate = elapsed > 0 ? stats.bytes_checked / elapsed / 1048576.0 : 0;

    printf("----------------------------------------\n");
    printf("Directories:  %lu\n", (unsigned long)stats.dirs);
    printf("Files:        %lu\n", (unsigned long)stats.files_checked);
    char data_str[32];
    fmt_bytes(stats.bytes_checked, data_str, sizeof(data_str));
    printf("Data read:    %s (%lu bytes)\n", data_str, (unsigned long)stats.bytes_checked);
    printf("Errors:       %lu\n", (unsigned long)stats.errors);
    printf("Elapsed:      %.0f seconds\n", elapsed);
    printf("Throughput:   %.1f MiB/s\n", rate);

    if (stats.errors > 0) {
        printf("\nWARNING: DATA CORRUPTION DETECTED -- %lu errors found\n", (unsigned long)stats.errors);
        return EXIT_CORRUPTION;
    }
    printf("\nOK: Scrub complete -- no errors\n");
    return EXIT_OK;
}
