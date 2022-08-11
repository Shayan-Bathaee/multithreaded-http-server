#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "format.h"
#include "datastruct.h"

#define OPTIONS              "t:l:"
#define DEFAULT_THREAD_COUNT 4

static FILE *logfile;

// multithreading global variables
struct queue *q;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condvar = PTHREAD_COND_INITIALIZER;
pthread_mutex_t logfile_lock = PTHREAD_MUTEX_INITIALIZER;

#define LOG(...) fprintf(logfile, __VA_ARGS__);

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

static void handle_connection(int connfd) {
    // initialize variables
    char http_request[HEADER_MAX_SIZE + 1]
        = { 0 }; // string to contain up to 2048 bytes of http request
    char request_line[HEADER_MAX_SIZE + 1] = { 0 }; // string to contain the request line
    struct request *request_data = malloc(sizeof(struct request)); // holds data from request
    struct node *root = make_root(); // create a linked list to store header fields
    int status_code = 500; // start with internal server error
    bool end_found = false;

    // read client input until we find an end
    int bytes_read = 0;
    int total_bytes = 0;
    while (!end_found) {
        bytes_read = read(connfd, http_request + total_bytes, BLOCK_SIZE - total_bytes);
        if (http_request[0] == '\0') { // not sure why I have to do this but I have to do this
            return;
        }
        total_bytes += bytes_read;
        end_found = find_end(http_request);
    }
    request_data->first_read_size = total_bytes; // add total_bytes into request data

    int result = parse_header(
        http_request, request_data, request_line, &root); // parse the http request header

    // handle client input
    if (result != 200) { // handle internal server error
        dprintf(connfd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                        "22\r\n\r\nInternal Server Error\n");
        status_code = 500;

        pthread_mutex_lock(&logfile_lock);
        LOG("%s,/%s,%d,%s\n", request_data->method, request_data->uri, status_code, get_id(root));
        fflush(logfile);
        pthread_mutex_unlock(&logfile_lock);

        free_linked_list(root);
        free(request_data);
        return;
    }

    // call method function
    if ((strcmp(request_data->method, "GET") == 0) || (strcmp(request_data->method, "get") == 0)) {
        status_code = GET(request_data, connfd);
    } else if ((strcmp(request_data->method, "PUT") == 0)
               || (strcmp(request_data->method, "put") == 0)) {
        status_code = PUT(request_data, connfd, http_request);
    } else if ((strcmp(request_data->method, "APPEND") == 0)
               || (strcmp(request_data->method, "append") == 0)) {
        status_code = APPEND(request_data, connfd, http_request);
    }

    // update audit log
    pthread_mutex_lock(&logfile_lock);
    LOG("%s,/%s,%d,%s\n", request_data->method, request_data->uri, status_code, get_id(root));
    fflush(logfile);
    pthread_mutex_unlock(&logfile_lock);

    free_linked_list(root);
    free(request_data);
    return;
}

void *thread_func() {
    while (1) {
        pthread_mutex_lock(&lock);
        int connfd = dequeue(q); // dequeue a request
        if (connfd == -1) { // if there wasn't anything to dequeue
            pthread_cond_wait(
                &condvar, &lock); // pass lock back to main, try again when main signals.
            connfd = dequeue(q);
        }
        pthread_mutex_unlock(&lock);

        if (connfd != -1) {
            handle_connection(connfd);
            close(connfd);
        }
    }
    return NULL;
}

static void sigterm_handler(int sig) {
    if (sig == SIGTERM) {
        warnx("received SIGTERM");
        free_queue(q);
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
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
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sigterm_handler);

    // initialize queue
    q = malloc(sizeof(struct queue));
    queue_init(q);

    // create thread pool
    pthread_t thread_pool[threads];
    for (int i = 0; i < threads; i++) {
        if (pthread_create(&thread_pool[i], NULL, thread_func, NULL) != 0) {
            return EXIT_SUCCESS;
        }
    }

    int listenfd = create_listen_socket(port);

    for (;;) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }

        // put connection into work queue
        pthread_mutex_lock(&lock);
        enqueue(q, connfd);
        pthread_cond_signal(&condvar);
        pthread_mutex_unlock(&lock);
    }

    return EXIT_SUCCESS;
}
