#include "segel.h"
#include "request.h"
#include "request_queue.h"

typedef enum {
    SCHED_BLOCK,
    SCHED_DT,
    SCHED_DH,
    SCHED_BF,
    SCHED_RD
} schedalg_t;


schedalg_t parse_schedalg(char *schedalg) {
    if (strcmp(schedalg, "block") == 0) return SCHED_BLOCK;
    if (strcmp(schedalg, "dt") == 0) return SCHED_DT;
    if (strcmp(schedalg, "dh") == 0) return SCHED_DH;
    if (strcmp(schedalg, "bf") == 0) return SCHED_BF;
    if (strcmp(schedalg, "random") == 0) return SCHED_RD;
    fprintf(stderr, "Unknown scheduling algorithm: %s\n", schedalg);
    exit(1);
}

void getargs(int *port, int *threads, int *queue_size, char **schedalg, int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <port> <threads> <queue_size> <schedalg>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    *schedalg = argv[4];
}

pthread_cond_t queue_not_full;
pthread_cond_t queue_not_empty;
pthread_cond_t queue_empty;
pthread_mutex_t queue_mutex;

request_queue_t incoming_queue;
request_queue_t processing_queue;

int total_capacity;

// Function to write a message to a log file
void write_log(const char *message) {
    FILE *log_file = fopen("server.log", "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return;
    }

    fprintf(log_file, "%s\n", message);
    fclose(log_file);
}

void *worker_thread(void *arg) {
    thread_stats_t *t_stats = (thread_stats_t *)arg;
    while (1) {
        struct timeval arrival_time, dispatch_time, current_time;

        pthread_mutex_lock(&queue_mutex);
        while (incoming_queue.size == 0) {
            pthread_cond_wait(&queue_not_empty, &queue_mutex);
        }
        int connfd = dequeue(&incoming_queue, &arrival_time);
        enqueue(&processing_queue, connfd, arrival_time);
        pthread_mutex_unlock(&queue_mutex);

        // Calculate dispatch interval
        gettimeofday(&current_time, NULL);
        timersub(&current_time, &arrival_time, &dispatch_time);

        // Update thread statistics
        t_stats->total_req++;

        requestHandle(connfd, arrival_time, dispatch_time, t_stats);

        pthread_cond_signal(&queue_not_full); // Signal that the queue is not full

        pthread_mutex_lock(&queue_mutex);
        remove_by_connfd(&processing_queue, connfd);
        if (incoming_queue.size == 0 && processing_queue.size == 0) {
            pthread_cond_signal(&queue_empty);
        }
        pthread_mutex_unlock(&queue_mutex);
    }
    return NULL;
}



int main(int argc, char *argv[]) {
    int listenfd, connfd, port, clientlen, threads, queue_size;
    struct sockaddr_in clientaddr;
    char *schedalg_str;
    schedalg_t schedalg;

    getargs(&port, &threads, &queue_size, &schedalg_str, argc, argv);
    schedalg = parse_schedalg(schedalg_str);

    total_capacity = queue_size;

    init(&incoming_queue);
    init(&processing_queue);

    pthread_mutex_init(&queue_mutex, NULL);
    pthread_cond_init(&queue_not_full, NULL);
    pthread_cond_init(&queue_not_empty, NULL);
    pthread_cond_init(&queue_empty, NULL);


    pthread_t *thread_pool = malloc(threads * sizeof(pthread_t));
    thread_stats_t *thread_stats = malloc(threads * sizeof(thread_stats_t));
    for (int i = 0; i < threads; i++) {
        thread_stats[i].id = i;
        thread_stats[i].total_req = 0;
        thread_stats[i].stat_req = 0;
        thread_stats[i].dynm_req = 0;
        pthread_create(&thread_pool[i], NULL, worker_thread, &thread_stats[i]);
    }

    listenfd = Open_listenfd(port);
    srand(time(NULL)); // Initialize random seed for drop_random

    while (1) {
        struct timeval arrival_time;
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t *) &clientlen);

        gettimeofday(&arrival_time, NULL);

        pthread_mutex_lock(&queue_mutex);
        if (incoming_queue.size + processing_queue.size >= total_capacity) {
            if (schedalg == SCHED_BF) {
                while (incoming_queue.size > 0 || processing_queue.size > 0) {
                    pthread_cond_wait(&queue_empty, &queue_mutex);
                }
                pthread_mutex_unlock(&queue_mutex);
                Close(connfd);
                continue;
            } else if (schedalg == SCHED_BLOCK) {
                while (incoming_queue.size + processing_queue.size >= total_capacity) {
                    pthread_cond_wait(&queue_not_full, &queue_mutex);
                }
            } else if (schedalg == SCHED_DT) {
                pthread_mutex_unlock(&queue_mutex);
                Close(connfd);
                continue;
            } else if (schedalg == SCHED_DH) {
                if (incoming_queue.size > 0) {
                    struct timeval ignored_time;
                    int ignored_fd = dequeue(&incoming_queue, &ignored_time);
                    Close(ignored_fd);
                } else {
                    pthread_mutex_unlock(&queue_mutex);
                    Close(connfd);
                    continue;
                }
            } else {
                if (incoming_queue.size > 0) {
                    int drop_count = (incoming_queue.size + 1) / 2;
                    for (int i = 0; i < drop_count; i++) {
                        int index = rand() % incoming_queue.size;
                        int ignored_fd = remove_node_at_index(&incoming_queue, index);
                        Close(ignored_fd);
                    }
                } else {
                    pthread_mutex_unlock(&queue_mutex);
                    Close(connfd);
                    continue;
                }
            }
        }
        enqueue(&incoming_queue, connfd, arrival_time);
        pthread_cond_signal(&queue_not_empty);
        pthread_mutex_unlock(&queue_mutex);
    }
}