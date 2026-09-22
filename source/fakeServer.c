#include "fakeServer.h"
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "knxHandle.h"
#include "zlog.h"

char usage[] = "Usage: log <sync>\n"
               "          knx <group> <value>\n"
               "          mbus <packet>\n"
               "          help\n"
               "          quit\n"
               "          esc\n";

#define SHM_NAME "/fake_shm"
#define SHM_SIZE 4096
#define SEM_SERVER_NAME "/fake_server"
#define SEM_CLIENT_NAME "/fake_client"

typedef struct {
    pthread_mutex_t lock;
    char buffer[512];
} shm_data_t;

void fakeServer (UNUSED void *param) {
    char buffer[512];
    int fd = shm_open (SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) { perror ("shm_open"); }
    int ret = ftruncate (fd, SHM_SIZE);
    if (ret < 0) { perror ("ftruncate"); }
    shm_data_t *data = mmap (NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    pthread_mutexattr_t mattr;
    pthread_mutexattr_init (&mattr);
    pthread_mutexattr_setpshared (&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init (&data->lock, &mattr);

    sem_t *sem_server = sem_open (SEM_SERVER_NAME, O_CREAT, 0666, 0);
    sem_t *sem_client = sem_open (SEM_CLIENT_NAME, O_CREAT, 0666, 0);

    while (true) {
        sem_wait (sem_server);
        memset (buffer, 0, sizeof (buffer));
        pthread_mutex_lock (&data->lock);
        memcpy (buffer, data->buffer, sizeof (data->buffer));
        pthread_mutex_unlock (&data->lock);

        if (strncmp (buffer, "help", strlen ("help")) == 0) {
            pthread_mutex_lock (&data->lock);
            memcpy (data->buffer, usage, strlen (usage) + 1);
            pthread_mutex_unlock (&data->lock);
        } else if (strncmp (buffer, "log", strlen ("log")) == 0) {
            if (strncmp (buffer + strlen ("log "), "sync", strlen ("sync")) == 0) {
                zlog_reload ("/opt/smarthome/config/zlog.conf");
                continue;
            } else {
                pthread_mutex_lock (&data->lock);
                memcpy (data->buffer, usage, strlen (usage) + 1);
                pthread_mutex_unlock (&data->lock);
            }
        } else if (strncmp (buffer, "knx", strlen ("knx")) == 0) {
            char group[32], value1[32], value2[32];
            ret = sscanf (buffer + strlen ("knx "), "%s %s %s", group, value1, value2);
            // printf("----group=%s, value1=%s, value2=%s\n", group, value1, value2);
            if (ret == 2) {
                KNX_FakePacket(group, value1);
            } else if (ret == 3) {
                sprintf(value1 + strlen(value1), " %s", value2);
                // printf("----group=%s, value=%s\n", group, value1);
                KNX_FakePacket(group, value1);
            } else {
                pthread_mutex_lock (&data->lock);
                memcpy (data->buffer, usage, strlen (usage) + 1);
                pthread_mutex_unlock (&data->lock);
                continue;
            }
            continue;
        } else if (strncmp (buffer, "mbus", strlen ("mbus")) == 0) {
            char packet[256];
            sscanf (buffer + strlen ("mbus "), "%s", packet);
            printf ("[Server] Received MBUS packet: %s\n", packet);
            continue;
        } else {
            pthread_mutex_lock (&data->lock);
            memcpy (data->buffer, usage, strlen (usage) + 1);
            pthread_mutex_unlock (&data->lock);
        }

        sem_post (sem_client);
    }

    munmap (data, SHM_SIZE);
    shm_unlink (SHM_NAME);
    sem_close (sem_server);
    sem_close (sem_client);
    sem_unlink (SEM_SERVER_NAME);
    sem_unlink (SEM_CLIENT_NAME);
}

#if 0

void enable_raw_mode() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);

    tty.c_lflag &= ~(ICANON | ECHO);  // 关闭规范模式和回显
    tty.c_cc[VMIN] = 1;               // 至少读1字节
    tty.c_cc[VTIME] = 0;              // 不超时

    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

void disable_raw_mode() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);

    tty.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

int fakeClient (void *param) {
    int fd = shm_open (SHM_NAME, O_RDWR, 0666);
    if (fd < 0) {
        perror ("shm_open");
        exit (1);
    }
    enable_raw_mode();
    shm_data_t *data = mmap (NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    sem_t *sem_server = sem_open (SEM_SERVER_NAME, 0);
    sem_t *sem_client = sem_open (SEM_CLIENT_NAME, 0);
    char *line = NULL;
    size_t len = 0;
    char buf[512];
    int idx = 0;
    char c;

    while (true) {
        printf ("-> ");
        fflush (stdout);
#if 0
        printf ("-> ");
        fflush (stdout);
        ssize_t size = getline (&line, &len, stdin);
        if (size == -1) break;
        if (size == 1) {
            if (line[0] == '\n') continue;
        }
        if (line[size - 1] == '\n') line[size - 1] = 0;
        else if (line[size - 1] == 27) continue;

        pthread_mutex_lock (&data->lock);
        memcpy (data->buffer, line, size);
        pthread_mutex_unlock (&data->lock);
#else
        while (true) {
            if (read(STDIN_FILENO, &c, 1) <= 0) continue;
            if (c == 27) {  // ESC
                int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
                fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
                char seq[3];
                if (read(STDIN_FILENO, &seq[0], 1) <= 0) {
                    printf ("Exiting...\n");
                    fcntl(STDIN_FILENO, F_SETFL, flags);
                    goto FAKE_END;
                }
                fcntl(STDIN_FILENO, F_SETFL, flags);
                if (seq[0] == '[') {
                    read(STDIN_FILENO, &seq[1], 1);
                    if (seq[1] == 'A') printf("Up\n");
                    else if (seq[1] == 'B') printf("Down\n");
                    else if (seq[1] == 'C') printf("Right\n");
                    else if (seq[1] == 'D') printf("Left\n");
                    idx = 0;
                    printf ("-> ");
                    fflush (stdout);
                    continue;
                }
            } else if (c == '\n') {
                buf[idx] = 0;
                printf ("\n");
                if (idx == 0) {
                    printf ("-> ");
                    fflush (stdout);
                    continue;
                } else {
                    if (strncmp (buf, "quit", strlen ("quit")) == 0) {
                        printf ("Exiting...\n");
                        goto FAKE_END;
                    } else if (strncmp (buf, "clear", strlen ("clear")) == 0) {
                        printf ("\033[H\033[J");
                        printf ("-> ");
                        fflush (stdout);
                        idx = 0;
                        continue;
                    }
                    pthread_mutex_lock (&data->lock);
                    memcpy (data->buffer, buf, idx);
                    pthread_mutex_unlock (&data->lock);
                }
                idx = 0; // 清空输入缓冲
                break;
            } else {
                if (idx < sizeof(buf) - 1) buf[idx++] = c;
                printf("%c", c);
                fflush(stdout);
            }
        }
#endif
        sem_post (sem_server);
#if 0
        sem_timedwait(sem_client, &(struct timespec){.tv_sec = time(NULL) + 1});
#else
        struct timespec ts;
        clock_gettime (CLOCK_REALTIME, &ts);
        ts.tv_nsec += 250 * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        if (sem_timedwait (sem_client, &ts) == -1) {
            if (errno == ETIMEDOUT) continue;
        }
#endif
        pthread_mutex_lock (&data->lock);
        printf ("-> %s\n", data->buffer);
        pthread_mutex_unlock (&data->lock);
    }

    FAKE_END:

    disable_raw_mode();
    munmap (data, SHM_SIZE);
    sem_close (sem_server);
    sem_close (sem_client);
    return 0;
}
#endif
