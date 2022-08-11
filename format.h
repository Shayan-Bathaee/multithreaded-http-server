#ifndef FORMAT_HEADER
#define FORMAT_HEADER

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include "datastruct.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <pthread.h>

#define HEADER_MAX_SIZE 2048
#define BLOCK_SIZE      2048
#define CONTENT_LENGTH  "Content-Length"
#define VERSION         "HTTP/1.1"

// struct to hold request information
struct request {
    char *method;
    char *uri;
    struct node *headers_root;
    int header_len;
    int content_len;
    int first_read_size;
    int request_line_len;
};

bool find_end(char *);
int parse_header(char *, struct request *, char *, struct node **);
int parse_request_line(char *, struct request *);
int get_header_fields(char *, struct request *, struct node **);
int GET(struct request *, int);
int PUT(struct request *, int, char *);
int APPEND(struct request *, int, char *);
int ReadIn(int, uint8_t *);
int WriteOut(int, uint8_t *, int);

#endif
