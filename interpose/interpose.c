#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>


int last_fd = 0;
pthread_mutex_t fd_list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function pointers for the original functions
static int (*original_open)(const char *pathname, int flags, ...) = NULL;
static int (*original_open64)(const char *pathname, int flags, ...) = NULL;
static int (*original_creat)(const char *pathname, mode_t mode) = NULL;
static int (*original_creat64)(const char *pathname, mode_t mode) = NULL;

// Function to initialize the function pointers to the original functions
void init_original_functions()
{
    union {
        void *p;
        int (*f_open)(const char *, int, ...);
        int (*f_open64)(const char *, int, ...);
        int (*f_creat)(const char *, mode_t);
        int (*f_creat64)(const char *, mode_t);
    } u;

    u.p = dlsym(RTLD_NEXT, "open");
    original_open = u.f_open;

    u.p = dlsym(RTLD_NEXT, "open64");
    original_open64 = u.f_open64;

    u.p = dlsym(RTLD_NEXT, "creat");
    original_creat = u.f_creat;

    u.p = dlsym(RTLD_NEXT, "creat64");
    original_creat64 = u.f_creat64;
}

// Override open
int open(const char *pathname, int flags, ...)
{
    if (!original_open)
        init_original_functions();

    va_list args;
    va_start(args, flags);
    int mode = 0;
    if (flags & O_CREAT)
    {
        mode = va_arg(args, int);
    }
    va_end(args);

    int fd = original_open(pathname, flags, mode);

    pthread_mutex_lock(&fd_list_mutex);
    last_fd = fd;
    pthread_mutex_unlock(&fd_list_mutex);

    return fd;
}

// Override open64
int open64(const char *pathname, int flags, ...)
{
    if (!original_open64)
        init_original_functions();

    va_list args;
    va_start(args, flags);
    int mode = 0;
    if (flags & O_CREAT)
    {
        mode = va_arg(args, int);
    }
    va_end(args);

    int fd = original_open64(pathname, flags, mode);

    pthread_mutex_lock(&fd_list_mutex);
    last_fd = fd;
    pthread_mutex_unlock(&fd_list_mutex);

    return fd;
}

// Override creat
int creat(const char *pathname, mode_t mode)
{
    if (!original_creat)
        init_original_functions();

    int fd = original_creat(pathname, mode);

    pthread_mutex_lock(&fd_list_mutex);
    last_fd = fd;
    pthread_mutex_unlock(&fd_list_mutex);

    return fd;
}

// Override creat64
int creat64(const char *pathname, mode_t mode) {
    if (!original_creat64) init_original_functions();

    int fd = original_creat64(pathname, mode);

    pthread_mutex_lock(&fd_list_mutex);
    last_fd = fd;
    pthread_mutex_unlock(&fd_list_mutex);

    return fd;
}

// Function to get the list of file descriptors
int interpose_get_fd()
{
    pthread_mutex_lock(&fd_list_mutex);
    int fd = last_fd;
    pthread_mutex_unlock(&fd_list_mutex);

    return fd;
}
