#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include "queue.h"
#include <sys/stat.h>

#define PORT 9000
#define START_LEN 128

typedef struct socket_thread_data_s socket_thread_data_t;
struct socket_thread_data_s
{
    pthread_mutex_t *m_mutex_exit;
    int m_conn_fd;
    bool m_thread_complete;
    bool m_exit_requested;
};

typedef struct slist_data_s slist_data_t;
struct slist_data_s
{
    pthread_t *thread;
    socket_thread_data_t *thread_data;
    struct in_addr client_addr;
    SLIST_ENTRY(slist_data_s)
    entries;
};

int socket_fd;
volatile sig_atomic_t exitRequested = 0;
#if USE_AESD_CHAR_DEVICE == 0
volatile sig_atomic_t timestampRequested = 0;
#endif
bool test = false;
pthread_mutex_t mutex;

#if USE_AESD_CHAR_DEVICE == 0
#define LOG_FILE "/var/tmp/aesdsocketdata"
#else
#define LOG_FILE "/dev/aesdchar"
#endif

void thread_cleanup(char *buffer, char *sendBuf)
{
    if (buffer != NULL)
    {
        free(buffer);
    }
    if (sendBuf != NULL)
    {
        free(sendBuf);
    }
}

void *process_connection(void *thread_func_data)
{
    socket_thread_data_t *thread_data = (socket_thread_data_t *)thread_func_data;

    char *buffer = NULL;
    char *sendBuf = NULL;
    ssize_t len_recv, len_write, len_read;
    int len, buflen, pos;
    char *temp = NULL;
    off_t fsize;

    FILE *fd;

    while (1)
    {
        buflen = START_LEN;
        pos = 0;
        buffer = (char *)malloc(sizeof(char) * buflen);
        if (buffer == NULL)
        {
            perror("malloc");
            printf("buffer malloc failed\n");
            thread_cleanup(buffer, sendBuf);
            thread_data->m_thread_complete = true;
            return thread_data;
        }
        memset(buffer, 0, buflen);
        do
        {
            // pthread_mutex_lock(thread_data->m_mutex_exit);
            if (thread_data->m_exit_requested)
            {
                thread_cleanup(buffer, sendBuf);
                thread_data->m_thread_complete = true;
                pthread_mutex_unlock(thread_data->m_mutex_exit);
                return thread_data;
            }
            // pthread_mutex_unlock(thread_data->m_mutex_exit);

            len_recv = recv(thread_data->m_conn_fd, buffer + pos, buflen - pos - 1, 0);
            if (len_recv == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    continue;
                }
                perror("recv");
                printf("Receive failed\n");
                thread_cleanup(buffer, sendBuf);
                thread_data->m_thread_complete = true;
                return thread_data;
            }
            else if (len_recv == buflen - pos - 1)
            {
                buflen += START_LEN;
                pos = buflen - START_LEN - 1;
                temp = (char *)realloc(buffer, buflen);
                if (temp == NULL)
                {
                    perror("realloc");
                    printf("Realloc failed\n");
                    thread_cleanup(buffer, sendBuf);
                    thread_data->m_thread_complete = true;
                    return thread_data;
                }
                buffer = temp;
            }
            else if (len_recv == 0)
            {
                thread_cleanup(buffer, sendBuf);
                thread_data->m_thread_complete = true;
                return thread_data;
            }
            else
            {
                pos += len_recv;
            }
            *(buffer + (buflen - 1)) = 0;
        } while (strchr(buffer, '\n') == NULL);

        pthread_mutex_lock(&mutex);
        fd = fopen(LOG_FILE, "a+");
        if (!fd)
        {
            perror("fopen");
            thread_cleanup(buffer, sendBuf);
            thread_data->m_thread_complete = true;
            return thread_data;
        }
        len_write = fwrite(buffer, strlen(buffer), 1, fd);
        fclose(fd);
        pthread_mutex_unlock(&mutex);
        if (len_write == -1)
        {
            perror("write");
            printf("Write failed\n");
            thread_cleanup(buffer, sendBuf);
            thread_data->m_thread_complete = true;
            return thread_data;
        }
        else if (len_write != 1)
        {
            perror("write");
            printf("Partial write occurred\n");
            thread_cleanup(buffer, sendBuf);
            thread_data->m_thread_complete = true;
            return thread_data;
        }

        pthread_mutex_lock(&mutex);
        int fp = open(LOG_FILE, O_RDWR);
        fsize = lseek(fp, 0, SEEK_END);
        if (fsize == -1)
        {
            perror("lseek");
            thread_cleanup(buffer, sendBuf);
            thread_data->m_thread_complete = true;
            return thread_data;
        }
        lseek(fp, 0, SEEK_SET);
        close(fp);
        pthread_mutex_unlock(&mutex);
        len = fsize;

        sendBuf = (char *)malloc(sizeof(char) * (len + 1));
        if (sendBuf == NULL)
        {
            perror("malloc");
            printf("sendBuf malloc failed\n");
            thread_cleanup(buffer, sendBuf);
            thread_data->m_thread_complete = true;
            return thread_data;
        }

        temp = sendBuf;
        pthread_mutex_lock(&mutex);
        fd = fopen(LOG_FILE, "r+");
        len_read = fread(temp, len, 1, fd);
        if (len_read != 1)
        {
            perror("read");
            printf("File read failure\n");
            thread_cleanup(buffer, sendBuf);
            thread_data->m_thread_complete = true;
            return thread_data;
        }

        fclose(fd);
        pthread_mutex_unlock(&mutex);

        temp = sendBuf;
        len = fsize;

        while (len != 0 && (len_write = write(thread_data->m_conn_fd, temp, len)) != 0)
        {
            if (len_write == -1)
            {
                if (errno == EINTR)
                    continue;
                perror("write");
                printf("Socket write failure\n");
                thread_cleanup(buffer, sendBuf);
                thread_data->m_thread_complete = true;
                return thread_data;
            }
            len -= len_write;
            temp += len_write;
        }
    }
}

void cleanup()
{
    if (socket_fd != -1)
    {
        close(socket_fd);
    }
#if USE_AESD_CHAR_DEVICE == 0
    remove("/var/tmp/aesdsocketdata");
#endif
    closelog();
}

static void sigint_handler(int signo)
{
    syslog(LOG_DEBUG, "Caught signal, exiting");
    exitRequested = 1;
}
#if USE_AESD_CHAR_DEVICE == 0
void append_timestamp(int signo)
{
    timestampRequested = 1;
}
#endif
int main(int argc, char *argv[])
{
    int conn_fd;
    int reuseaddr = 1;
    struct sockaddr_in server, client;
    char client_str[INET_ADDRSTRLEN];
    socklen_t client_size;
    bool useDaemon;
    pid_t pid;
    slist_data_t *datap = NULL;
    slist_data_t *datap_temp = NULL;
    socket_thread_data_t *thread_data = NULL;

    struct timeval ts;
    ts.tv_sec = 0;
    ts.tv_usec = 50000;

#if USE_AESD_CHAR_DEVICE == 0
    FILE *fd;
    struct itimerval delay;
    delay.it_value.tv_sec = 0;
    delay.it_value.tv_usec = 1;
    delay.it_interval.tv_sec = 10;
    delay.it_interval.tv_usec = 0;

    time_t t;
    struct tm *wallTime;
    char time_str[16];
    char str[] = "timestamp:";
#endif
    pthread_mutex_init(&mutex, NULL);

    SLIST_HEAD(slisthead, slist_data_s)
    head;
    SLIST_INIT(&head);

    if (argc == 2)
    {
        if (strcmp(argv[1], "-d") == 0)
        {
            useDaemon = true;
        }
        else
        {
            printf("Command Line arg not recognized. Only -d can be used.\n");
            cleanup();
            exit(-1);
        }
    }
    else
    {
        useDaemon = false;
    }

    openlog("aesdsocket.c", 0, LOG_USER);

    if (signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        perror("signal");
        printf("Couldn't handle SIGINT");
        cleanup();
        exit(-1);
    }
    if (signal(SIGTERM, sigint_handler) == SIG_ERR)
    {
        perror("signal");
        printf("Couldn't handle SIGTERM");
        cleanup();
        exit(-1);
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        perror("socket");
        printf("Socket couldn't be created\n");
        cleanup();
        exit(-1);
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1)
    {
        perror("setsockopt");
        printf("Couldn't set socket reusability\n");
        exit(-1);
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &ts, sizeof(ts)) == -1)
    {
        perror("setsockopt");
        printf("Couldn't set socket timeout\n");
        exit(-1);
    }

    fcntl(socket_fd, F_SETFL, O_NONBLOCK);

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if ((bind(socket_fd, (struct sockaddr *)&server, sizeof(server))) < 0)
    {
        perror("bind");
        printf("Socket bind failed\n");
        cleanup();
        exit(-1);
    }

    if (useDaemon)
    {
        pid = fork();
        if (pid == -1)
        {
            perror("fork");
            printf("Fork failed\n");
            cleanup();
            exit(-1);
        }
        else if (pid != 0)
        {
            cleanup();
            exit(EXIT_SUCCESS);
        }
    }
#if USE_AESD_CHAR_DEVICE == 0
    if (signal(SIGALRM, append_timestamp) == SIG_ERR)
    {
        perror("signal");
        printf("Couldn't handle SIGALRM");
        cleanup();
        exit(-1);
    }
    if (setitimer(ITIMER_REAL, &delay, NULL))
    {
        perror("setitimer");
        printf("Error starting timer");
        cleanup();
        exit(-1);
    }
#endif
    if ((listen(socket_fd, 10)) != 0)
    {
        perror("listen");
        printf("Listen failed\n");
        cleanup();
        exit(-1);
    }

    client_size = sizeof(client);
    while (exitRequested == 0)
    {
#if USE_AESD_CHAR_DEVICE == 0
        if (timestampRequested)
        {
            memset(time_str, 0, sizeof(time_str));
            t = time(NULL);
            wallTime = localtime(&t);
            strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", wallTime);
            time_str[14] = '\n';
            pthread_mutex_lock(&mutex);
            fd = fopen(LOG_FILE, "a+");
            fwrite(str, strlen(str), 1, fd);
            fwrite(time_str, strlen(time_str), 1, fd);
            fclose(fd);
            pthread_mutex_unlock(&mutex);
            timestampRequested = 0;
        }
#endif
        conn_fd = accept(socket_fd, (struct sockaddr *)&client, &client_size);
        if (conn_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }
            perror("accept");
            printf("Accept failed\n");
            cleanup();
            exit(-1);
        }

        inet_ntop(AF_INET, &(client.sin_addr), client_str, INET_ADDRSTRLEN);
        syslog(LOG_DEBUG, "Accepted connection from %s", client_str);

        thread_data = (socket_thread_data_t *)malloc(sizeof(socket_thread_data_t));
        thread_data->m_conn_fd = conn_fd;
        thread_data->m_mutex_exit = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(thread_data->m_mutex_exit, NULL);
        thread_data->m_thread_complete = false;
        thread_data->m_exit_requested = false;

        datap = (slist_data_t *)malloc(sizeof(slist_data_t));
        datap->thread = (pthread_t *)malloc(sizeof(pthread_t));
        datap->thread_data = thread_data;
        datap->client_addr = client.sin_addr;
        SLIST_INSERT_HEAD(&head, datap, entries);

        pthread_create(datap->thread, NULL, process_connection, (void *)thread_data);

        SLIST_FOREACH_SAFE(datap, &head, entries, datap_temp)
        {
            if (datap->thread_data->m_thread_complete)
            {
                pthread_join(*(datap->thread), NULL);
                inet_ntop(AF_INET, &(datap->client_addr), client_str, INET_ADDRSTRLEN);
                syslog(LOG_DEBUG, "Closed connection from %s", client_str);
                close(datap->thread_data->m_conn_fd);
                SLIST_REMOVE(&head, datap, slist_data_s, entries);
                free(datap->thread_data->m_mutex_exit);
                free(datap->thread_data);
                free(datap->thread);
                free(datap);
            }
        }
    }

    SLIST_FOREACH(datap, &head, entries)
    {
        pthread_mutex_lock(datap->thread_data->m_mutex_exit);
        datap->thread_data->m_exit_requested = true;
        pthread_mutex_unlock(datap->thread_data->m_mutex_exit);
    }

    SLIST_FOREACH_SAFE(datap, &head, entries, datap_temp)
    {
        pthread_join(*(datap->thread), NULL);
        inet_ntop(AF_INET, &(datap->client_addr), client_str, INET_ADDRSTRLEN);
        syslog(LOG_DEBUG, "Closed connection from %s", client_str);
        close(datap->thread_data->m_conn_fd);
        SLIST_REMOVE(&head, datap, slist_data_s, entries);
        free(datap->thread_data->m_mutex_exit);
        free(datap->thread_data);
        free(datap->thread);
        free(datap);
    }
    cleanup();
    exit(EXIT_SUCCESS);
}