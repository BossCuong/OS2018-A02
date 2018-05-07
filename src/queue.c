#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
	return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
	/* TODO: put a new process to queue [q] */
	int size = q->size;

	q->proc[size] = proc;

	q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
	/* TODO: return a pcb whose prioprity is the highest
	 * in the queue [q] and remember to remove it from q
	 * */
	int q_size = q->size;

	if (q_size == 0)
		return NULL;

	uint32_t highest_prioprity = q->proc[0]->priority;
	uint32_t prioprity;

	int hp_index = 0;

	//Get the index of highest prioprity process
	for (int i = 0; i < q_size; i++)
	{
		prioprity = q->proc[i]->priority;

		if (prioprity > highest_prioprity)
		{
			highest_prioprity = prioprity;

			hp_index = i;
		}
	}

	//Return valie
	struct pcb_t *r_proc = q->proc[hp_index];

	//Delete highest prioprity process in q
	for (int i = hp_index; i < q_size - 1; i++)
		q->proc[i] = q->proc[i + 1];

	q->proc[q_size - 1] = NULL;

	q->size--;

	return r_proc;
}
