/*
 * Implements a queue of integers using a linked list.
 * 
 * Dan Whisman, 11/15/13
 */

/*
 * Inserts a node at the end of the queue.  Returns a pointer
 * to the node just created.
 */
qnode_t* enqueue( qnode_t** head, qnode_t** tail, int val )
{
  qnode_t* n = NULL;
  
  if( head != NULL && tail != NULL )
  {
    n = (qnode_t*)malloc( sizeof(qnode_t) );
    
    if( n != NULL )
    {
      n->value = val;
      n->next = NULL;
      n->prev = *tail;
      
      // If not empty list, insert value at end
      if( (*head) != NULL && (*tail) != NULL )
      {
        (*tail)->next = n;
      }
      else
      {
        // Start new list
        (*head) = n;
        (*tail) = n;
      }
    }
  }
  
  return n;
}

/*
 * Removes a node from the head of the queue and
 * returns its value.
 */
int dequeue( qnode_t** head, qnode_t** tail )
{
  int retval = 0;
  qnode_t* newhead;
  
  if( head != NULL && tail != NULL &&
     *head != NULL &&*tail != NULL )
  {
    retval = (*head)->value;
    
    if( (*tail) == (*head) )
    {
      // Only one node, remove it.
      free( *head );
      
      (*head) = NULL;
      (*tail) = NULL;
    }
    else
    {
      newhead = (*head)->next;
      newhead->prev = NULL;
      free(*head);
      (*head) = newhead;
    }
  }
  
  return retval;
}

/*
 * Returns the value a the head of the queue.
 */
int peek( qnode_t* head )
{
  int retval = 0;
  
  if( head != NULL )
    retval = head->value;
  
  return retval;  
}
