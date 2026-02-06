#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(char **argv) {
    fprintf(stderr, "usage: %s [-f <file name>] [-i <interval secs>]\n", argv[0]);
    fprintf(
        stderr,
        "\t-f <file name>\tset name of a file in /var/tmp/test_module/ to which test_module writes "
        "messages\n"
    );
    fprintf(stderr, "\t-i <interval secs>\tset interval in seconds; 0 disables timer\n");
    fprintf(stderr, "\n");
    fprintf(
        stderr,
        "When no options provided, %s prints the current parameters of the test_module.\n",
        argv[0]
    );
}

static void suggest_help(char **argv) {
    fprintf(stderr, "type %s -h for help\n", argv[0]);
}

static FILE *open_sysfs_attr(const char *path, const char *modes) {
    FILE *f = fopen(path, modes);
    if (f == NULL) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        if (errno != EACCES)
            fprintf(stderr, "perhaps the test_module is not loaded?\n");
    }
    return f;
}

static int write_sysfs_attr(const char *path, const char *fmt, ...) {
    FILE *f = open_sysfs_attr(path, "w");
    if (f == NULL)
        return -errno;

    va_list vl;
    va_start(vl, fmt);
    int ret = vfprintf(f, fmt, vl);
    if (ret < 0) {
        perror("failed to write parameter");
        ret = -errno;
    }
    va_end(vl);

    if (fclose(f) == EOF) {
        // fwrite может записывать в свой буфер, и только fclose приведёт к реальному вызову write.
        // Поэтому только в этот момент можем получить ошибку от модуля.
        perror("failed to write parameter");
        ret = -errno;
    }

    return ret;
}

static int read_sysfs_attr(const char *path, char *buf, size_t n) {
    int ret;

    FILE *f = open_sysfs_attr(path, "r");
    if (f == NULL) {
        ret = -errno;
        goto out;
    }

    ret = fread(buf, 1, n - 1, f);
    if (!feof(f)) {
        fprintf(stderr, "fread() failed: %s\n", strerror(ferror(f)));
        ret = -ferror(f);
        goto out;
    }
    buf[ret] = 0;

out:
    if (f != NULL)
        fclose(f);
    return ret;
}

static void remove_trailing_newline(char *str) {
    size_t len = strlen(str);
    if (len != 0 && str[len - 1] == '\n')
        str[len - 1] = 0;
}

int main(int argc, char **argv) {
    int opt;
    const char *filename = NULL;
    int interval_secs = -1;
    while ((opt = getopt(argc, argv, "hf:i:")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv);
            return 0;
        case 'f':
            filename = optarg;
            break;
        case 'i':
            errno = 0;
            unsigned long parsed_ulong = strtoul(optarg, NULL, 10);
            if (!errno && parsed_ulong > INT_MAX)
                errno = ERANGE;
            if (errno) {
                perror("invalid interval in seconds");
                suggest_help(argv);
                return 1;
            }
            interval_secs = parsed_ulong;
            break;
        default:
            suggest_help(argv);
            return 1;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "unrecognized argument: %s\n", argv[optind]);
        suggest_help(argv);
        return 1;
    }

    const char *SYSFS_FILENAME = "/sys/kernel/test_module/filename";
    if (filename != NULL) {
        if (write_sysfs_attr(SYSFS_FILENAME, "%s", filename) < 0) {
            if (interval_secs >= 0)
                fprintf(stderr, "interval not updated\n");
            return 1;
        }
    }

    const char *SYSFS_INTERVAL = "/sys/kernel/test_module/interval_secs";
    if (interval_secs >= 0)
        if (write_sysfs_attr(SYSFS_INTERVAL, "%d", interval_secs) < 0)
            return 1;

    if (interval_secs < 0 && filename == NULL) {
        const size_t SYSFS_VALUE_MAX_SIZE = NAME_MAX + 1; // +1 for trailing newline
        char interval_buf[SYSFS_VALUE_MAX_SIZE + 1], filename_buf[SYSFS_VALUE_MAX_SIZE];

        if (read_sysfs_attr(SYSFS_FILENAME, filename_buf, sizeof filename_buf) < 0)
            return 1;
        remove_trailing_newline(filename_buf);

        if (read_sysfs_attr(SYSFS_INTERVAL, interval_buf, sizeof interval_buf) < 0)
            return 1;
        remove_trailing_newline(interval_buf);

        const char *timer_status = "";
        if (strtoul(interval_buf, NULL, 10) == 0 && !errno)
            timer_status = " (timer disabled)";

        printf("interval: %s seconds%s\n", interval_buf, timer_status);
        printf("file: /var/tmp/test_module/%s\n", filename_buf);
    }

    return 0;
}
