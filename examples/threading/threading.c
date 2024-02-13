#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg, ...)
// #define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("threading ERROR: " msg "\n", ##__VA_ARGS__)

void *threadfunc(void *thread_param)
{
    struct thread_data *thread_func_args = (struct thread_data *)thread_param;

    struct timespec ts;
    int ret;

    // Wait for desired milliseconds before acquiring mutex
    ts.tv_sec = thread_func_args->m_wait_to_obtain_ms / 1000;
    ts.tv_nsec = (thread_func_args->m_wait_to_obtain_ms % 1000) * 1000000;
    ret = nanosleep(&ts, NULL);
    if (ret != 0)
    {
        ERROR_LOG("Nanosleep Error");
    }

    // Acquire mutex and block if not available
    pthread_mutex_lock(thread_func_args->m_mutex);

    // Wait for desired milliseconds before releasing mutex
    ts.tv_sec = thread_func_args->m_wait_to_release_ms / 1000;
    ts.tv_nsec = (thread_func_args->m_wait_to_release_ms % 1000) * 1000000;
    ret = nanosleep(&ts, NULL);
    if (ret != 0)
    {
        ERROR_LOG("Nanosleep Error");
    }

    // Release the mutex
    pthread_mutex_unlock(thread_func_args->m_mutex);

    // Mark the thread completed
    thread_func_args->thread_complete_success = true;

    // Return from the thread
    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    // Allocate mem for thread data struct
    struct thread_data *thread_param = malloc(sizeof(struct thread_data));
    // Populate struct fields
    thread_param->m_mutex = mutex;
    thread_param->m_wait_to_obtain_ms = wait_to_obtain_ms;
    thread_param->m_wait_to_release_ms = wait_to_release_ms;
    thread_param->thread_complete_success = false;
    // Spawn the thread with threadfunc as the entrypoint
    int ret = pthread_create(thread, NULL, threadfunc, (void *)thread_param);
    if (ret != 0)
    {
        // Return false if thread couldn't be created
        errno = ret;
        perror("pthread_create");
        return false;
    }
    // Thread creation successful, return true
    return true;
}
