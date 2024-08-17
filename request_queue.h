//
// Created by Max on 26/07/2024.
//

#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#include <sys/time.h>

// Define the node structure
typedef struct node {
    int connfd;
    struct timeval arrival_time;
    struct node *next;
} node_t;

// Define the queue structure
typedef struct {
    node_t *front;
    node_t *rear;
    int size;
} request_queue_t;

// Function declarations
void init(request_queue_t *queue);
void enqueue(request_queue_t *queue, int connfd, struct timeval arrival_time);
int dequeue(request_queue_t *queue, struct timeval *arrival_time);
int remove_node_at_index(request_queue_t *queue, int index);
int remove_by_connfd(request_queue_t *queue, int connfd);
int dequeue_last(request_queue_t *queue, struct timeval *arrival_time);

#endif // REQUEST_QUEUE_H
