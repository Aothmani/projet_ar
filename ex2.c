#include <stdlib.h>
#include <mpi.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "utils.h"
#include "election.h"

#define PROC_COUNT 6
#define M 6
#define K (1 << M)

#define MSWAP(a, b) {typeof(a) swap_#a#b;	\
		swap_#a#b = a;			\
		a = b;				\
		b = swap_#a#b;}


void swap(int *a, int *b)
{
	int c;
	c = *b;
	*b = *a;
	*a = c;
}

void sequence_array(int *arr, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++) {
		arr[i] = i;
	}
}

void init_node(struct node *node)
{
	MPI_Status status;
	MPI_Recv(&node->rank, 1, MPI_INT, 0, TAGINIT, MPI_COMM_WORLD, &status);
	receive_addr(TAGINIT, &node->next_addr);
	MPI_Recv(&node->leader, 1, MPI_INT, 0, TAGINIT, MPI_COMM_WORLD,
		 &status);
}

void print_fingers(struct node_addr* fingers, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		printf("%d ", fingers[i].chord);
	}
	printf("\n");
}

void print_array(struct array* arr) {
        print_fingers(arr->data, arr->size);
}

void node(int rank)
{
	struct node node;
	int next;
	MPI_Status status;
	int elect_state, leader = -1;
	int reception = 0;
	struct node_addr addr;

	printf("P%d> Starting node\n", rank);
	
	node.mpi_rank = rank;
	node.leader = 0;
	node.fingers = malloc(sizeof(struct node_addr) * M);
	node.fingers->size = M;
	
	memset(node.fingers->data, 0, sizeof(struct node_addr) * M);
	
	init_node(&node);

	printf("P%d> Chord rank = %d, next = (%d, %d)\n",
	       rank, node.rank, node.next_addr.mpi, node.next_addr.chord);
	
	addr.chord = node.rank;
	addr.mpi = node.mpi_rank;
	
	if (node.leader) {
		printf("P%d> Is leader\n", rank);
		election(rank, node.next_addr.mpi);
		elect_state = ELECT_CANDIDATE;
	}

	while (!reception) {
		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		printf("P%d> Received message from %d, tag = ",
		       rank, status.MPI_SOURCE);
		switch (status.MPI_TAG) {
		case TAGELECT:
			printf("TAGELECT\n");
			receive_elect(&node, &elect_state,
				      &leader);
			break;
		case TAGTAB:
			printf("TAGTAB\n");
			receive_tab(&addr, &node.next_addr,
				    elect_state == ELECT_LEADER ? 1 : 0);
			break;
		case TAGTABANN:
			printf("TAGTABANN\n");
			receive_tabann(&node, elect_state, &reception);
			break;
		default:
			printf("Undefined message tag\n");
			break;
		}
	}
	printf("P%d> finger table: ", rank);
	print_array(node.fingers);
	
	free(node.fingers);
}

void shuffle(int *arr, size_t len)
{
	size_t i;
	int target;
	for (i = 0; i < len; i++) {
		target = rand() % len;
		swap(arr + i, arr + target);
	}
}


void simulator(void)
{
	int mpi_ranks[M] = {1, 2, 3, 4, 5, 6};
	int chord_ids[M];
        int i, next;
	int leader;
	int one = 1, zero = 0;
	struct node_addr next_addr;

	printf("Starting simulator process\n");
	
	/* Randomize chords ids */
	sequence_array(chord_ids, M);
	shuffle(chord_ids, M);

	printf("Simulator, shuffle => ");
	for (i = 0; i < M; i++) {
		printf("(%d, %d), ", mpi_ranks[i], chord_ids[i]);
	}
	printf("\n");

	leader = rand() % M;
	
	/* Send chords ids and mpi ids for next nodes */
	for (i = 0; i < M; i++) {
		SEND_INT(mpi_ranks[i], TAGINIT, chord_ids[i]);
		next = (i + 1) % M;
		next_addr.mpi = next + FIRST_NODE;
		next_addr.chord = chord_ids[next];
		send_addr(mpi_ranks[i], TAGINIT, &next_addr);
		if (i == leader)
			SEND_INT(mpi_ranks[i], TAGINIT, one);
		else
			SEND_INT(mpi_ranks[i], TAGINIT, zero);
	}	
}


int main(int argc, char *argv[])
{
	int proc_count, rank;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &proc_count);
	
	if (proc_count != PROC_COUNT + 1) {
		printf("Nombre de processus incorrect !\n");
		MPI_Finalize();
		exit(2);
	}
  
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	srand(time(NULL)^rank);
	
	if (rank == 0)
		simulator();
	else 
		node(rank);
	  
	MPI_Finalize();
	return 0;
}
