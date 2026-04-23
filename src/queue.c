#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include "common.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        if (q->size < MAX_QUEUE_SIZE){
                q->proc[q->size] = proc;
                q->size++;
        }
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
	if (empty(q)) return NULL;

        #ifdef MLQ_SCHED
        struct pcb_t *proc = q->proc[0];
        for ( int i =0; i < q->size-1; i ++){
                q->proc[i] = q->proc[i+1];
        }
        q->size--;

        #else
        int highestPriorityIdx = 0;
        for ( int i =1; i < q->size; i++){
                if(q->proc[i]->priority < q->proc[highestPriorityIdx]->priority){
                        highestPriorityIdx = i;
                }
        }
        struct pcb_t * proc = q->proc[highestPriorityIdx];
        for ( int i =0; i < q->size-1; i ++){
                q->proc[i] = q->proc[i+1];
        }
        q->size--;
        #endif
        return proc;
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: remove a specific item from queue
         * */
         
        if (empty(q) || proc == NULL) return NULL;

        int i=0;
        int found=0;
        struct pcb_t * removedProc = NULL;

        for (i =0; i < q->size; i++){
                if(q->proc[i] == proc){
                        found = 1;
                        removedProc = q->proc[i];
                        break;
                }
        }

        if(found){
                for( int j =i; j < q->size-1 ; j++){
                        q->proc[j] = q->proc[j+1];
                }
                q->size--;
                return removedProc;
        }
        return NULL;
}