#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include "monitor_ioctl.h"

#define STACK_SIZE (1024 * 1024)
#define LOG_BUFFER_CAPACITY 64
#define MAX_LOG_LINE 256

typedef struct {
    char container_id[32];
    char data[MAX_LOG_LINE];
    size_t length;
} log_item_t;

typedef struct {
    log_item_t items[LOG_BUFFER_CAPACITY];
    int head, tail, count;
    int shutting_down;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} bounded_buffer_t;

typedef struct {
    char id[32];
    char rootfs[PATH_MAX];
    char command[256];
    int log_write_fd;
} child_config_t;

// Implementation of push/pop logic
int bounded_buffer_push(bounded_buffer_t *buffer, const log_item_t *item) {
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->count == LOG_BUFFER_CAPACITY && !buffer->shutting_down)
        pthread_cond_wait(&buffer->not_full, &buffer->mutex);
    if (buffer->shutting_down) { pthread_mutex_unlock(&buffer->mutex); return -1; }
    buffer->items[buffer->tail] = *item;
    buffer->tail = (buffer->tail + 1) % LOG_BUFFER_CAPACITY;
    buffer->count++;
    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

int bounded_buffer_pop(bounded_buffer_t *buffer, log_item_t *item) {
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->count == 0 && !buffer->shutting_down)
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    if (buffer->count == 0 && buffer->shutting_down) { pthread_mutex_unlock(&buffer->mutex); return -1; }
    *item = buffer->items[buffer->head];
    buffer->head = (buffer->head + 1) % LOG_BUFFER_CAPACITY;
    buffer->count--;
    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

int child_fn(void *arg) {
    child_config_t *config = (child_config_t *)arg;
    sethostname(config->id, strlen(config->id));
    if (chroot(config->rootfs) != 0 || chdir("/") != 0) return 1;
    mount("proc", "/proc", "proc", 0, NULL);
    dup2(config->log_write_fd, STDOUT_FILENO);
    dup2(config->log_write_fd, STDERR_FILENO);
    char *args[] = {"/bin/sh", "-c", config->command, NULL};
    execv("/bin/sh", args);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: sudo ./engine run <id> <rootfs> <cmd> [--hard-mib <num>]\n");
        return 1;
    }
    child_config_t config;
    strncpy(config.id, argv[2], 31);
    realpath(argv[3], config.rootfs);
    strncpy(config.command, argv[4], 255);
    config.log_write_fd = STDOUT_FILENO;

    unsigned long hard_limit = 50 * 1024 * 1024; // Default 50MB
    if (argc > 6 && strcmp(argv[5], "--hard-mib") == 0) {
        hard_limit = strtoul(argv[6], NULL, 10) * 1024 * 1024;
    }

    void *stack = malloc(STACK_SIZE);
    pid_t pid = clone(child_fn, (char *)stack + STACK_SIZE, CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD, &config);
    
    int fd = open("/dev/container_monitor", O_RDWR);
    if (fd >= 0) {
        struct monitor_request req = {.pid = pid, .hard_limit_bytes = hard_limit, .soft_limit_bytes = hard_limit / 2};
        strncpy(req.container_id, config.id, MONITOR_NAME_LEN - 1);
        ioctl(fd, MONITOR_REGISTER, &req);
        close(fd);
    }
    
    waitpid(pid, NULL, 0);
    return 0;
}
