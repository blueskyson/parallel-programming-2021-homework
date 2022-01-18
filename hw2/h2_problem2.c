#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

void my_sort(int *arr, int front, int end);

void printarr(int *arr, int size);

int main(int argc, char *argv[]) {
    int my_rank, comm_sz;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    // Broadcast n to all processes
    int n = 100;
    if (my_rank == 0) {
        if (argc == 2) {
            char *ptr;
            n = (int)strtol(argv[1], &ptr, 10);
        }
        printf("n = %d\n", n);
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // Generate local list
    int local_n = n / comm_sz;
    int *local_list = (int*)malloc(sizeof(int) * local_n);
    srand(time(NULL) + my_rank);
    int i;
    for (i = 0; i < local_n; i++) {
        local_list[i] = rand() % 1000;
    }
    my_sort(local_list, 0, local_n - 1);

    // Gather and print local lists
    int total_n = local_n * comm_sz;
    int *list = NULL;
    if (my_rank == 0) {
        list = (int*)malloc(sizeof(int) * total_n);
    }
    MPI_Gather(local_list, local_n, MPI_INT, list, local_n, MPI_INT, 0, MPI_COMM_WORLD);
    if (my_rank == 0) {
        printf("\nlocal lists:\n");
        for (i = 0; i < comm_sz; i++) {
            printf("rank %d:", i);
            int offset = i * local_n;
            int j;
            for (j = 0; j < local_n; j++) {
                printf(" %d", list[offset + j]);
            }
            printf("\n");
        }
    }

    double startwtime = 0.0, endwtime = 0;
    MPI_Barrier(MPI_COMM_WORLD);
    startwtime = MPI_Wtime();

    // Find peer processes of odd and even phases
    int even_phase_peer, odd_phase_peer;
    int my_rank_is_odd = my_rank % 2;
    if (!my_rank_is_odd) {
        even_phase_peer = my_rank + 1;
        odd_phase_peer = my_rank - 1;
    } else {
        even_phase_peer = my_rank - 1;
        odd_phase_peer = my_rank + 1;
    }

    // deal with border processes' peers 
    if (my_rank == 0) {
        odd_phase_peer = MPI_PROC_NULL;
    } else if (my_rank == comm_sz - 1) {
        if (!my_rank_is_odd) {
            even_phase_peer = MPI_PROC_NULL;
        } else {
            odd_phase_peer = MPI_PROC_NULL;
        }
    }

    // Begin odd even sort
    MPI_Status status;
    int *peer_list = (int*)malloc(sizeof(int) * local_n);
    int *tmp_list = (int*)malloc(sizeof(int) * local_n);
    for (i = 0; i < comm_sz; i++) {
        int phase_is_odd = i % 2;
        if (!phase_is_odd) {
            MPI_Sendrecv(local_list, local_n, MPI_INT, even_phase_peer, 0, peer_list,
                         local_n, MPI_INT, even_phase_peer, 0, MPI_COMM_WORLD, &status);
        } else {
            MPI_Sendrecv(local_list, local_n, MPI_INT, odd_phase_peer, 0, peer_list,
                         local_n, MPI_INT, odd_phase_peer, 0, MPI_COMM_WORLD, &status);
        }
        
        if (status.MPI_SOURCE == MPI_PROC_NULL)
            continue;

        int cursor1, cursor2, cursor3;
        if (my_rank > status.MPI_SOURCE) {  // Right peer
            cursor1 = cursor2 = cursor3 = local_n - 1;
            while (cursor3 >= 0) {
                if (local_list[cursor1] > peer_list[cursor2]) {
                    tmp_list[cursor3] = local_list[cursor1];
                    cursor1--;
                } else {
                    tmp_list[cursor3] = peer_list[cursor2];
                    cursor2--;
                }
                cursor3--;
            }
        } else {                            // Left peer 
            cursor1 = cursor2 = cursor3 = 0;
            while (cursor3 < local_n) {
                if (local_list[cursor1] < peer_list[cursor2]) {
                    tmp_list[cursor3] = local_list[cursor1];
                    cursor1++;
                } else {
                    tmp_list[cursor3] = peer_list[cursor2];
                    cursor2++;
                }
                cursor3++;
            }
        }
        memcpy(local_list, tmp_list, sizeof(int) * local_n);
    }

    MPI_Gather(local_list, local_n, MPI_INT, list, local_n, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
    endwtime = MPI_Wtime();

    if (my_rank == 0) {
        printf("\nResult:\n");
        for (i = 0; i < total_n; i++) {
            printf("%d ", list[i]);
        }
        printf("\n");
    }
    MPI_Finalize();
    return 0;
}

void printarr(int *arr, int size) {
    int i;
    for (i = 0; i < size; i++) {
        printf("%d ", arr[i]);
    }
}

void my_sort(int *arr, int front, int end) {
    if(front < end) {
        int i, index = front, pivot = arr[end], tmp;
        for(i = front; i < end; i++) {
            if(arr[i] < pivot) {
                tmp = arr[i];         //swap
                arr[i] = arr[index];  //
                arr[index] = tmp;     //
                index++;
            }
        }
        tmp = arr[i];         //swap pivot
        arr[i] = arr[index];  //
        arr[index] = tmp;     //
        my_sort(arr, front, index - 1);
        my_sort(arr, index + 1, end);
    }
}
