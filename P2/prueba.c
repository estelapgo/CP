#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "new_queue.h"

#define NUM_THREADS 4
#define QUEUE_SIZE 5
#define NUM_ITEMS 10

queue q;
pthread_mutex_t mutex_producer, mutex_consumer;

void *producer(void *arg) {
    int i;
    for (i = 0; i < NUM_ITEMS; i++) {
        int *item = malloc(sizeof(int));
        *item = i;
        
        pthread_mutex_lock(&mutex_producer);
        while (q_elements(q) == QUEUE_SIZE) {
            pthread_mutex_unlock(&mutex_producer);
            sched_yield();                          // permite que el hilo consumidor obtenga el mutex
            pthread_mutex_lock(&mutex_producer);
        }
        
        q_insert(q, item);
        q_print(q);
        printf("Producido: %d\n\n", i);
        
        
        
        pthread_mutex_unlock(&mutex_producer);
        sched_yield();
    }
    pthread_exit(NULL);
}

void *consumer(void *arg) {
    int i;
    for (i = 0; i < NUM_ITEMS; i++) {
        pthread_mutex_lock(&mutex_consumer);
        while (q_elements(q) == 0) {
            pthread_mutex_unlock(&mutex_consumer);
            sched_yield();                          // permite que el hilo productor obtenga el mutex
            pthread_mutex_lock(&mutex_consumer);
        }
        
        int *item = q_remove(q);
        if (item != NULL) {
            q_print(q);
            printf("Consumido: %d\n", *item);
            
            free(item);
        }
        
        pthread_mutex_unlock(&mutex_consumer);
        sched_yield();
    }
    pthread_exit(NULL);
}

int main() {
    q = q_create(QUEUE_SIZE);
    pthread_mutex_init(&mutex_producer, NULL);
    pthread_mutex_init(&mutex_consumer, NULL);

    pthread_t threads[NUM_THREADS];
    int i;

    for (i = 0; i < NUM_THREADS; i++) {
        if (i < NUM_THREADS/2) {
            if (pthread_create(&threads[i], NULL, producer, NULL) != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
        } else {
            if (pthread_create(&threads[i], NULL, consumer, NULL) != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
        }
    }

      

    for (i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    pthread_mutex_destroy(&mutex_producer);
    pthread_mutex_destroy(&mutex_consumer);
    
    q_destroy(q);

    return 0;
}