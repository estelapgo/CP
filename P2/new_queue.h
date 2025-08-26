#ifndef __QUEUE_H__
#define __QUEUE_H__

typedef struct _queue *queue;

queue q_create(int size);            // Create a new queue
int   q_elements(queue q);           // Number of elements in a queue
int   q_insert(queue q, void *elem); // Insert an element into a queue
void *q_remove(queue q);             // Remove an element from the queue
void  q_destroy(queue q);            // Destroy a queue
void  q_print(queue q);              // Print the current elements in the queue

#endif
