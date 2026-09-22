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
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

#define SHM_NAME "/fake_shm"
#define SHM_SIZE 4096
#define SEM_SERVER_NAME "/fake_server"
#define SEM_CLIENT_NAME "/fake_client"

#define HISTORY_SIZE 50 // 历史命令保存条数
#define CMD_MAX_LEN 512

typedef struct {
    pthread_mutex_t lock;
    char buffer[512];
} shm_data_t;

static char history[HISTORY_SIZE][CMD_MAX_LEN];
static int history_count = 0; // 已存条数
static int history_index = 0; // 当前游标

void enable_raw_mode() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);

    tty.c_lflag &= ~(ICANON | ECHO); // 关闭规范模式和回显
    tty.c_cc[VMIN] = 1; // 至少读1字节
    tty.c_cc[VTIME] = 0; // 不超时

    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

void disable_raw_mode() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);

    tty.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

void save_history(const char* cmd) {
    if (cmd[0] == '\0') return; // 空命令不保存
    strncpy(history[history_count % HISTORY_SIZE], cmd, CMD_MAX_LEN - 1);
    history[history_count % HISTORY_SIZE][CMD_MAX_LEN - 1] = '\0';
    history_count++;
    history_index = history_count; // 重置到最新位置
}

const char* history_navigation(int direction) {
    if (history_count == 0) return NULL;

    if (direction < 0) { // Up
        if (history_index > 0) history_index--;
    } else if (direction > 0) { // Down
        if (history_index < history_count) history_index++;
    }

    if (history_index == history_count) {
        return ""; // 最新位置，返回空
    }
    return history[history_index % HISTORY_SIZE];
}

const char *autocomplete(const char *prefix) {
    size_t len = strlen(prefix);
    if (len == 0) return NULL;

    for (int i = history_count - 1; i >= 0; i--) {
        if (strncmp(history[i % HISTORY_SIZE], prefix, len) == 0) {
            return history[i % HISTORY_SIZE];
        }
    }
    return NULL;
}

int main(void) {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open");
        exit(1);
    }

    enable_raw_mode();
    shm_data_t* data = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    sem_t* sem_server = sem_open(SEM_SERVER_NAME, 0);
    sem_t* sem_client = sem_open(SEM_CLIENT_NAME, 0);
    char buf[512];
    int idx = 0;
    int cursor = 0;
    char c;

    while (true) {
        printf("-> ");
        fflush(stdout);
        while (true) {
            if (read(STDIN_FILENO, &c, 1) <= 0) continue;
            if (c == 27) { // ESC
                int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
                fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
                char seq[3];
                if (read(STDIN_FILENO, &seq[0], 1) <= 0) {
                    printf("ESC pressed, exit\n");
                    fcntl(STDIN_FILENO, F_SETFL, flags);
                    goto FAKE_END;
                }
                fcntl(STDIN_FILENO, F_SETFL, flags);
                if (seq[0] == '[') {
                    read(STDIN_FILENO, &seq[1], 1);
                    if (seq[1] == 'A') {
                        const char* cmd = history_navigation(-1);
                        if (cmd) {
                            strncpy(buf, cmd, sizeof(buf) - 1);
                            idx = strlen(buf);
                            printf("\33[2K\r-> %s", buf);
                            fflush(stdout);
                        }
                        continue;
                    } else if (seq[1] == 'B') {
                        const char* cmd = history_navigation(1);
                        if (cmd) {
                            strncpy(buf, cmd, sizeof(buf) - 1);
                            idx = strlen(buf);
                            printf("\33[2K\r-> %s", buf);
                            fflush(stdout);
                        }
                        continue;
                    } else if (seq[1] == 'C') {
                        if (cursor < idx) {
                            printf("\33[C");
                            cursor++;
                        }
                        continue;
                    } else if (seq[1] == 'D') {
                        if (cursor > 0) {
                            printf("\33[D");
                            cursor--;
                        }
                        continue;
                    }else if (seq[1] == '3') { // Delete
                        char tilde;
                        read(STDIN_FILENO, &tilde, 1); // 读取 '~'
                        if (tilde == '~') {
                            if (cursor < idx) {
                                memmove(&buf[cursor], &buf[cursor+1], idx - cursor - 1);
                                idx--;
                                buf[idx] = '\0';
                                // 重绘
                                printf("\33[2K\r-> %s", buf);
                                int move = idx - cursor;
                                if (move > 0) printf("\33[%dD", move);
                                fflush(stdout);
                            }
                        }
                        continue;
                    }
                }
            } else if (c == '\t') {
                // Tab 自动补全
                buf[idx] = '\0';
                const char *match = autocomplete(buf);
                if (match) {
                    strncpy(buf, match, sizeof(buf)-1);
                    idx = strlen(buf);
                    cursor = idx;
                    printf("\33[2K\r-> %s", buf);
                    fflush(stdout);
                }
                continue;
            } else if (c == '\n') {
                buf[idx] = 0;
                printf("\n");
                if (idx == 0) {
                    printf("-> ");
                    fflush(stdout);
                    continue;
                } else {
                    if (strncmp(buf, "quit", strlen("quit")) == 0) {
                        printf("Exiting...\n");
                        goto FAKE_END;
                    } else if (strncmp(buf, "clear", strlen("clear")) == 0) {
                        printf("\033[H\033[J");
                        printf("-> ");
                        fflush(stdout);
                        idx = 0;
                        continue;
                    }
                    save_history(buf);
                    pthread_mutex_lock(&data->lock);
                    memcpy(data->buffer, buf, idx);
                    pthread_mutex_unlock(&data->lock);
                }
                idx = 0; // 清空输入缓冲
                cursor = 0;
                printf("-> ");
                fflush(stdout);
                sem_post (sem_server);

                // 等待回应
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
                pthread_mutex_lock (&data->lock);
                printf ("-> %s\n", data->buffer);
                pthread_mutex_unlock (&data->lock);
            } else {
                if (c == 127 || c == 8) { // Backspace
                    if (cursor > 0) {
                        memmove(&buf[cursor - 1], &buf[cursor], idx - cursor);
                        idx--;
                        cursor--;
                        buf[idx] = '\0';

                        // 重绘整行
                        printf("\33[2K\r-> %s", buf);
                        // 把光标移回正确位置
                        int move = idx - cursor;
                        if (move > 0) printf("\33[%dD", move);
                        fflush(stdout);
                    }
                } else if (isprint(c)) { // 可打印字符
                    if (idx < sizeof(buf) - 1) {
                        memmove(&buf[cursor + 1], &buf[cursor], idx - cursor);
                        buf[cursor] = c;
                        idx++;
                        cursor++;

                        buf[idx] = '\0';
                        // 重绘
                        printf("\33[2K\r-> %s", buf);
                        int move = idx - cursor;
                        if (move > 0) printf("\33[%dD", move);
                        fflush(stdout);
                    }
                }
            }
        }
        // sem_post(sem_server);
        //
        // struct timespec ts;
        // clock_gettime(CLOCK_REALTIME, &ts);
        // ts.tv_nsec += 250 * 1000000;
        // if (ts.tv_nsec >= 1000000000) {
        //     ts.tv_sec += 1;
        //     ts.tv_nsec -= 1000000000;
        // }
        // if (sem_timedwait(sem_client, &ts) == -1) {
        //     if (errno == ETIMEDOUT) continue;
        // }
        //
        // pthread_mutex_lock(&data->lock);
        // printf("-> %s\n", data->buffer);
        // pthread_mutex_unlock(&data->lock);
    }

FAKE_END:
    disable_raw_mode();
    munmap(data, SHM_SIZE);
    sem_close(sem_server);
    sem_close(sem_client);

    return 0;
}
