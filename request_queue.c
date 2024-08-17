//
// Created by Max on 26/07/2024.
//

#include "request_queue.h"
#include <stdlib.h>

// Define the queue structure
//struct request_queue_t {
//    node_t *front;
//    node_t *rear;
//    int size;
//};

// Initialize the queue
void init(request_queue_t *queue) {
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}

// Enqueue operation
void enqueue(request_queue_t *queue, int connfd, struct timeval arrival_time) {
    node_t *new_node = (node_t *)malloc(sizeof(node_t));
    new_node->connfd = connfd;
    new_node->arrival_time = arrival_time;
    new_node->next = NULL;

    if (queue->rear == NULL) {
        queue->front = new_node;
        queue->rear = new_node;
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }
    queue->size++;
}

// Dequeue operation
int dequeue(request_queue_t *queue, struct timeval *arrival_time) {
    if (queue->front == NULL) {
        return -1; // Queue is empty
    }

    node_t *temp = queue->front;
    int connfd = temp->connfd;
    *arrival_time = temp->arrival_time;
    queue->front = queue->front->next;

    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    free(temp);
    queue->size--;
    return connfd;
}

int remove_node_at_index(request_queue_t *queue, int index) {
    if (index < 0 || index >= queue->size) {
        return -1; // Index out of bounds
    }

    node_t *current = queue->front;
    node_t *previous = NULL;

    for (int i = 0; i < index; i++) {
        previous = current;
        current = current->next;
    }

    if (previous == NULL) {
        // Removing the front node
        queue->front = current->next;
    } else {
        previous->next = current->next;
    }

    if (current == queue->rear) {
        // Removing the rear node
        queue->rear = previous;
    }

    int connfd = current->connfd;
    free(current);
    queue->size--;

    return connfd;
}

int remove_by_connfd(request_queue_t *queue, int connfd) {
    node_t *current = queue->front;
    node_t *previous = NULL;

    while (current != NULL) {
        if (current->connfd == connfd) {
            if (previous == NULL) {
                // Removing the front node
                queue->front = current->next;
            } else {
                previous->next = current->next;
            }

            if (current == queue->rear) {
                // Removing the rear node
                queue->rear = previous;
            }

            free(current);
            queue->size--;
            return 0; // Success
        }
        previous = current;
        current = current->next;
    }
    return -1; // connfd not found
}

int dequeue_last(request_queue_t *queue, struct timeval *arrival_time) {
    if (queue->front == NULL) {
        return -1; // Queue is empty
    }

    if (queue->front == queue->rear) {
        // Only one element in the queue
        int connfd = queue->front->connfd;
        *arrival_time = queue->front->arrival_time;
        free(queue->front);
        queue->front = NULL;
        queue->rear = NULL;
        queue->size--;
        return connfd;
    }

    // More than one element in the queue
    node_t *current = queue->front;
    while (current->next != queue->rear) {
        current = current->next;
    }

    int connfd = queue->rear->connfd;
    *arrival_time = queue->rear->arrival_time;
    free(queue->rear);
    queue->rear = current;
    queue->rear->next = NULL;
    queue->size--;

    return connfd;
}
