#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

typedef struct _queue {
    int size;
    int used;
    int first;
    void **data;
    pthread_mutex_t mutex;
    pthread_cond_t non_full;
    pthread_cond_t non_empty;
} _queue;

#include "new_queue.h"

queue q_create(int size) {
    queue q = malloc(sizeof(_queue));
    if (q == NULL) {
        perror("Error allocating memory for queue");
        exit(EXIT_FAILURE);
    }

    q->size = size;
    q->used = 0;
    q->first = 0;
    q->data = malloc(size * sizeof(void *));
    if (q->data == NULL) {
        perror("Error allocating memory for queue data");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        perror("Error initializing mutex");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&q->non_full, NULL) != 0) {
        perror("Error initializing non_full condition variable");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&q->non_empty, NULL) != 0) {
        perror("Error initializing non_empty condition variable");
        exit(EXIT_FAILURE);
    }

    return q;
}

int q_elements(queue q) {
    pthread_mutex_lock(&q->mutex);
    int elements = q->used;
    pthread_mutex_unlock(&q->mutex);
    return elements;
}

int q_insert(queue q, void *elem) {
    pthread_mutex_lock(&q->mutex);

    while (q->used == q->size) {
        pthread_cond_wait(&q->non_full, &q->mutex);
    }

    q->data[(q->first + q->used) % q->size] = elem;
    q->used++;

    pthread_cond_signal(&q->non_empty);
    pthread_mutex_unlock(&q->mutex);

    return 1;
}

void *q_remove(queue q) {
    void *res;
    pthread_mutex_lock(&q->mutex);

    while (q->used == 0) {
        pthread_cond_wait(&q->non_empty, &q->mutex);
    }

    res = q->data[q->first];
    q->first = (q->first + 1) % q->size;
    q->used--;

    pthread_cond_signal(&q->non_full);
    pthread_mutex_unlock(&q->mutex);

    return res;
}

void q_destroy(queue q) {
    pthread_mutex_lock(&q->mutex);
    free(q->data);
    pthread_mutex_unlock(&q->mutex);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->non_full);
    pthread_cond_destroy(&q->non_empty);
    free(q);
}

void q_print(queue q) {
    pthread_mutex_lock(&q->mutex);
    printf("Elementos actuales de la cola: ");
    if (q->used == 0) {
        printf("La cola está vacía\n");
    } else {
        int i;
        for (i = 0; i < q->used; i++) {
            printf("%d", *((int *)q->data[(q->first + i) % q->size]));
            if (i < q->used - 1) {
                printf(" ");
            }
        }
        printf("\n");
    }
    pthread_mutex_unlock(&q->mutex);
}
