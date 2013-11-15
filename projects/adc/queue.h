/*
 * Implements a queue of integers using a linked list.
 * 
 * Dan Whisman, 11/15/13
 */

#ifndef __QUEUE_H_
#define __QUEUE_H_

typedef struct qnode_s
{
  int value;
  struct qnode_s* prev;
  struct qnode_s* next;
}qnode_t;

qnode_t* enqueue( qnode_t** head, qnode_t** tail, int val );
int dequeue( qnode_t** head, qnode_t** tail );
int peek( qnode_t* head );

#endif /* __QUEUE_H_ */
