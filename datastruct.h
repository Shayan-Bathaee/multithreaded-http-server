#ifndef DATA_STRUCTS
#define DATA_STRUCTS

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

#define HEADER_MAX_SIZE 2048

// Linked List node to contain header field
struct node {
    char key[HEADER_MAX_SIZE + 1];
    char val[HEADER_MAX_SIZE + 1];
    struct node *next;
};

struct node *make_root();
struct node *insert_node(struct node **);
void print_linked_list(struct node *);
void free_linked_list(struct node *);
char *get_id(struct node *);

// queue node to contain the thread pool
struct qnode {
    int connfd;
    struct qnode *next;
};

// queue struct to contain length, head, and tail
struct queue {
    int length;
    struct qnode *head;
    struct qnode *tail;
};

void queue_init(struct queue *);

struct qnode *enqueue(struct queue *, int);
int dequeue(struct queue *);
void print_queue(struct queue *);
void free_queue(struct queue *);

#endif
