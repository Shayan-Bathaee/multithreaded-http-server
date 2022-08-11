
#include "datastruct.h"

// creates a root
struct node *make_root() {
    struct node *root = malloc(sizeof(struct node));
    root->next = NULL;
    return root;
}

// takes in the root, a header key, and the value
struct node *insert_node(struct node **root_ptr) {
    struct node *new = malloc(sizeof(struct node));
    new->next = *root_ptr;
    *root_ptr = new; // set new as the root
    return new; // everything went fine
}

// prints a linked list
void print_linked_list(struct node *root) {
    struct node *curr = root;
    while (curr != NULL) {
        printf("Key: %s, Val: %s\n", curr->key, curr->val);
        curr = curr->next;
    }
    return;
}

// frees all allocated nodes
void free_linked_list(struct node *root) {
    struct node *curr = root;
    while (root != NULL) {
        curr = root;
        root = curr->next;
        free(curr);
    }
    return;
}

// finds the key and returns the value. If the key was not found, it returns null.
char *get_id(struct node *root) {
    struct node *curr = root;
    while (curr != NULL) {
        if (!strcmp(curr->key, "Request-Id")) {
            return curr->val;
        }
        curr = curr->next;
    }
    return "0";
}

void queue_init(struct queue *q) {
    q->length = 0;
    q->head = NULL;
    q->tail = NULL;
}

// inserts node to the back of the queue, returns that node
struct qnode *enqueue(struct queue *q, int connfd) {
    struct qnode *new = malloc(sizeof(struct qnode)); // allocate space for new node
    new->connfd = connfd;
    if (q->head == NULL) { // empty queue
        q->head = new;
        q->tail = new;
        q->length = 1;
        return new;
    }
    q->tail->next = new; // set current tail node to point to new
    q->tail = new; // set new as the tail
    q->length += 1;
    return new;
}

// removes the head node of the queue, returns that node's connfd
int dequeue(struct queue *q) {
    if (q->length == 0) { // if queue is empty, return -1
        return -1;
    }
    struct qnode *to_remove = malloc(sizeof(struct qnode *));
    to_remove = q->head;
    q->head = to_remove->next; // set head to next node
    int connfd = to_remove->connfd; // get the result
    free(to_remove); // free the removed node
    q->length -= 1;
    return connfd;
}

// prints the queue
void print_queue(struct queue *q) {
    struct qnode *curr = q->head;
    printf("[");
    while (curr != NULL) {
        printf(" %d ", curr->connfd);
        curr = curr->next;
    }
    printf("]\n");
    return;
}

// frees all qnodes
void free_queue(struct queue *q) {
    if (q) {
        struct qnode *curr = q->head;
        while (curr != NULL) {
            struct qnode *next = curr->next;
            free(curr);
            curr = next;
        }
        free(q);
    }
    return;
}
