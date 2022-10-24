#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pthread.h>
#include <regex.h>
#include <ctype.h>
#include "List.h"

#define OPTIONS              "t:l:"
#define BUF_SIZE             4096
#define DEFAULT_THREAD_COUNT 4

#define METHOD  "[a-zA-Z]{1,8}"
#define URI     "/[a-zA-Z0-9_.]{1,19}"
#define VERSION "HTTP/1.1"
#define REQUEST "^" METHOD " " URI " " VERSION "\r\n"
#define HEADER  "[[a-zA-Z0-9_.-]:[ ]+[0-9]+\r\n]*"

static FILE *logfile;

List queue;

int flag = 0;
int readers = 0, writers = 0;

struct ThreadInfo {
    int count;
    pthread_t *dispatcher;
} ThreadInfo;

pthread_mutex_t lock, readwrite_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t notempty, reading, writing = PTHREAD_COND_INITIALIZER;

// Sends status message back to client.
void send_status(char msg[], int connfd, int code, char content[]) {
    sprintf(msg, "HTTP/1.1 %d %s\r\nContent-Length: %ld\r\n\r\n%s\n", code, content,
        strlen(content) + 1, content);
    send(connfd, msg, strlen(msg), 0);
}

void send_log(char *method, char *uri, int code, int request) {
    fprintf(logfile, "%s,/%s,%d,%d\n", method, uri, code, request);
    fflush(logfile);
}

// Converts a string to an 16 bits unsigned integer.
// Returns 0 if the string is malformed or out of the range.
static size_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}

// Creates a socket for listening for connections.
// Closes the program and prints an error message on error.
static int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(EXIT_FAILURE, "socket error");
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) < 0) {
        err(EXIT_FAILURE, "bind error");
    }
    if (listen(listenfd, 128) < 0) {
        err(EXIT_FAILURE, "listen error");
    }
    return listenfd;
}

void get_handler(int connfd, char *uri, int request) {
    int fd = open(uri, O_RDONLY);
    char msg[BUF_SIZE] = { 0 };

    struct stat fs;
    stat(uri, &fs);
    if (S_ISDIR(fs.st_mode) || errno == EACCES) {
        send_status(msg, connfd, 403, "Forbidden");
        close(fd);
        return;
    }

    if (fd > 0) {
        int size = fs.st_size;
        sprintf(msg, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", size);
        send(connfd, msg, strlen(msg), 0);

        char buffer[BUF_SIZE] = { 0 };
        ssize_t bytes = 0, curr_write = 0;

        while ((bytes = read(fd, buffer, BUF_SIZE)) > 0) {
            curr_write = send(connfd, buffer, bytes, 0);
            if (curr_write < 0) {
                send_status(msg, connfd, 500, "Internal Server Error");
                send_log("GET", uri, 500, request);
                close(fd);
                return;
            }
        }
        send_log("GET", uri, 200, request);

    } else {
        if (errno == ENOENT) {
            send_status(msg, connfd, 404, "Not Found");
            send_log("GET", uri, 404, request);
        } else {
            send_status(msg, connfd, 500, "Internal Server Error");
            send_log("GET", uri, 500, request);
        }
    }
    close(fd);
}

// Reads in from connfd and writes to file fd.
int file_write(int connfd, int fd, char buffer[], long len, int bytes_init) {
    int bytes_read = bytes_init, bytes = 0, curr_write = 0;
    char msg[BUF_SIZE] = { 0 };

    while (bytes_read < len) {
        bytes = recv(connfd, buffer, BUF_SIZE, 0);
        if (bytes <= 0) {
            break;
        }
        if ((bytes_read + bytes) > len) {
            curr_write = write(fd, buffer, (len - bytes_read));
        } else {
            curr_write = write(fd, buffer, bytes);
        }
        if (curr_write <= 0) {
            send_status(msg, connfd, 500, "Internal Server Error");
            return 500;
        }
        bytes_read += bytes;
    }
    return 200;
}

void put_handler(int connfd, char *uri, char *length, char *msgBuf, int msgBufLen, int request) {
    // check if file does not exist, then send CREATED instead of OK
    char msg[BUF_SIZE] = { 0 };
    char buffer[BUF_SIZE] = { 0 };
    long len = strtol(length, NULL, 0);

    if (access(uri, F_OK) == -1) { // from delftstack.com
        int fd = open(uri, O_CREAT | O_WRONLY | O_TRUNC, 0622);
        if (msgBufLen > 0) {
            if (len <= msgBufLen) {
                write(fd, msgBuf, len);
                send_status(msg, connfd, 201, "Created");
                close(fd);
                send_log("PUT", uri, 201, request);
                return;
            } else {
                write(fd, msgBuf, msgBufLen); // initial msg body from first read
            }
        }

        int bytes_read = 0;
        bytes_read += msgBufLen;
        if (file_write(connfd, fd, buffer, len, bytes_read) == 500) {
            close(fd);
            send_log("PUT", uri, 500, request);
            return;
        }

        send_status(msg, connfd, 201, "Created");
        close(fd);
        send_log("PUT", uri, 201, request);
        return;
    }

    // file already exists
    int fd = open(uri, O_WRONLY | O_TRUNC, 0622);
    struct stat fs;
    stat(uri, &fs);
    if (S_ISDIR(fs.st_mode) || errno == EACCES) {
        send_status(msg, connfd, 403, "Forbidden");
        close(fd);
        return;
    }
    if (msgBufLen > 0) {
        if (len <= msgBufLen) {
            write(fd, msgBuf, len);
            send_status(msg, connfd, 200, "OK");
            close(fd);
            send_log("PUT", uri, 200, request);
            return;
        } else {
            write(fd, msgBuf, msgBufLen); // initial msg body from first read
        }
    }

    int bytes_read = 0;
    bytes_read += msgBufLen;
    if (file_write(connfd, fd, buffer, len, bytes_read) == 500) {
        close(fd);
        send_log("PUT", uri, 500, request);
        return;
    }

    send_status(msg, connfd, 200, "OK");
    close(fd);
    send_log("PUT", uri, 200, request);
}

void append_handler(int connfd, char *uri, char *length, char *msgBuf, int msgBufLen, int request) {
    char msg[BUF_SIZE] = { 0 };
    char buffer[BUF_SIZE] = { 0 };
    long len = strtol(length, NULL, 0);

    int fd = open(uri, O_APPEND | O_WRONLY);
    if (errno == ENOENT && (fd < 0)) {
        send_status(msg, connfd, 404, "Not Found");
        close(fd);
        send_log("APPEND", uri, 404, request);
        return;
    }
    struct stat fs;
    stat(uri, &fs);
    if (S_ISDIR(fs.st_mode) || errno == EACCES) {
        send_status(msg, connfd, 403, "Forbidden");
        close(fd);
        return;
    }
    if (msgBufLen > 0) {
        if (len <= msgBufLen) {
            write(fd, msgBuf, len);
            send_status(msg, connfd, 200, "OK");
            close(fd);
            send_log("APPEND", uri, 200, request);
            return;
        } else {
            write(fd, msgBuf, msgBufLen); // initial msg body from first read
        }
    }

    int bytes_read = 0;
    bytes_read += msgBufLen;
    if (file_write(connfd, fd, buffer, len, bytes_read) == 500) {
        close(fd);
        send_log("APPEND", uri, 500, request);
        return;
    }

    send_status(msg, connfd, 200, "OK");
    close(fd);
    send_log("APPEND", uri, 200, request);
}

static void *handle_connection(int connfd) {
    char buffer[BUF_SIZE] = { 0 };
    char method[BUF_SIZE] = { 0 };
    char uri[BUF_SIZE] = { 0 };
    char version[BUF_SIZE] = { 0 };
    char msg[BUF_SIZE] = { 0 };
    char length[BUF_SIZE] = { 0 };
    regex_t regr, regc;

    if (regcomp(&regr, REQUEST, REG_EXTENDED)) {
        printf("Failed to compile regex.\n");
        return NULL;
    }
    if (regcomp(&regc, HEADER, REG_EXTENDED)) {
        printf("Failed to compile regex.\n");
        return NULL;
    }

    size_t bytes_read = recv(connfd, buffer, BUF_SIZE - 1, 0);
    if (regexec(&regr, buffer, 0, NULL, 0) == REG_NOMATCH) {
        send_status(msg, connfd, 400, "Bad Request");
        regfree(&regr);
        regfree(&regc);
        return NULL;
    }
    sscanf(buffer, "%s %s %s ", method, uri, version);
    memmove(uri, uri + 1, strlen(uri)); // delete "/" in front

    int request = 0;

    char *r = strstr(buffer, "Request-Id:");
    if (r != NULL) {
        sscanf(r, "Request-Id: %d", &request);
    }

    if (strcmp(method, "GET") == 0 || (strcmp(method, "get") == 0)) {
        pthread_mutex_lock(&readwrite_lock);
        while (writers > 0) { // currently writing to file, so wait until done
            pthread_cond_wait(&reading, &readwrite_lock);
        }
        readers += 1;
        pthread_mutex_unlock(&readwrite_lock);
        get_handler(connfd, uri, request);
        pthread_mutex_lock(&readwrite_lock);
        readers -= 1;
        if (readers == 0) { // able to write since no more readers
            pthread_cond_signal(&writing);
        }
        pthread_mutex_unlock(&readwrite_lock);
    } else if (strcmp(method, "PUT") == 0 || (strcmp(method, "put") == 0)) {
        pthread_mutex_lock(&readwrite_lock);
        while (writers > 0 || readers > 0) { // allow one writer at a time
            pthread_cond_wait(&writing, &readwrite_lock);
        }
        writers = 1;
        if (regexec(&regc, buffer, 0, NULL, 0) == REG_NOMATCH) {
            send_status(msg, connfd, 400, "Bad Request");
        } else {
            char *c = strstr(buffer, "Content-Length");
            sscanf(c, "%s %s", msg, length);
            char *token = strstr(buffer, "\r\n\r\n");
            token += 4;

            put_handler(connfd, uri, length, token, bytes_read - (token - buffer), request);
        }
        writers = 0;
        pthread_cond_signal(&writing);
        pthread_cond_broadcast(&reading); // broadcast all threads because more than 1 readers
        pthread_mutex_unlock(&readwrite_lock);
    } else if (strcmp(method, "APPEND") == 0 || (strcmp(method, "append") == 0)) {
        pthread_mutex_lock(&readwrite_lock);
        while (writers > 0 || readers > 0) {
            pthread_cond_wait(&writing, &readwrite_lock);
        }
        writers = 1;
        if (regexec(&regc, buffer, 0, NULL, 0) == REG_NOMATCH) {
            send_status(msg, connfd, 400, "Bad Request");
        } else {
            char *c = strstr(buffer, "Content-Length:");
            sscanf(c, "%s %s", msg, length);
            char *token = strstr(buffer, "\r\n\r\n");
            token += 4;

            append_handler(connfd, uri, length, token, bytes_read - (token - buffer), request);
        }
        writers = 0;
        pthread_cond_signal(&writing);
        pthread_cond_broadcast(&reading);
        pthread_mutex_unlock(&readwrite_lock);
    } else {
        send_status(msg, connfd, 501, "Not Implemented");
    }

    regfree(&regr);
    regfree(&regc);
    return NULL;
}

static void sigterm_handler(int sig) {
    if (sig == SIGTERM) {
        warnx("received SIGTERM");
        flag = 1;
        pthread_cond_broadcast(&notempty);

        for (int i = 0; i < ThreadInfo.count; i++) {
            if (pthread_join(ThreadInfo.dispatcher[i], NULL) != 0) { //EINVAL occuring
                err(1, "pthread_join failed");
            }
        }
        freeList(&queue);
        fclose(logfile);
        free(ThreadInfo.dispatcher);
        exit(EXIT_SUCCESS);
    } else if (sig == SIGINT) {
        warnx("received SIGINT");
        flag = 1;
        pthread_cond_broadcast(&notempty);

        for (int i = 0; i < ThreadInfo.count; i++) {
            if (pthread_join(ThreadInfo.dispatcher[i], NULL) != 0) {
                err(1, "pthread_join failed");
            }
        }
        freeList(&queue);
        fclose(logfile);
        free(ThreadInfo.dispatcher);
        exit(EXIT_SUCCESS);
    }
}

void *thread_handler(void *arg) {
    int worker = *((int *) arg);
    printf("thread: %d\n", worker);

    while (flag == 0) {
        int cfd = -1;
        pthread_mutex_lock(&lock);
        while (length(queue) == 0) {
            pthread_cond_wait(&notempty, &lock);
            if (flag != 0) {
                pthread_mutex_unlock(&lock);
                return NULL;
            }
        }
        if (length(queue) > 0) {
            cfd = front(queue);
            deleteFront(queue);
        }
        pthread_mutex_unlock(&lock);
        if (cfd != -1) {
            handle_connection(cfd);
            close(cfd);
        }
    }

    return NULL;
}

static void usage(char *exec) {
    fprintf(stderr, "usage: %s [-t threads] [-l logfile] <port>\n", exec);
}

int main(int argc, char *argv[]) {
    int opt = 0;
    int threads = DEFAULT_THREAD_COUNT;
    logfile = stderr;

    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            threads = strtol(optarg, NULL, 10);
            if (threads <= 0) {
                errx(EXIT_FAILURE, "bad number of threads");
            }
            break;
        case 'l':
            logfile = fopen(optarg, "w");
            if (!logfile) {
                errx(EXIT_FAILURE, "bad logfile");
            }
            break;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        warnx("wrong number of arguments");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port = strtouint16(argv[optind]);
    if (port == 0) {
        errx(EXIT_FAILURE, "bad port number: %s", argv[optind]);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    int listenfd = create_listen_socket(port);
    int worker[threads];

    ThreadInfo.count = threads;
    ThreadInfo.dispatcher = malloc(threads * sizeof(pthread_t));

    queue = newList();
    for (int i = 0; i < threads; i++) {
        worker[i] = i;
        if (pthread_create(&(ThreadInfo.dispatcher[i]), NULL, thread_handler, &worker[i]) != 0) {
            err(EXIT_FAILURE, "pthread_create() failed");
        }
    }

    for (;;) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }
        pthread_mutex_lock(&lock);
        append(queue, connfd); // worker queue
        pthread_cond_signal(&notempty);
        pthread_mutex_unlock(&lock);
    }

    return EXIT_SUCCESS;
}
