#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

#define RAND() ((double)rand() / RAND_MAX)

int main (int argc, char *argv[]) {
    /* MPI initialize */
    int comm_sz, my_rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    double startTime = 0.0, totalTime = 0.0;
    startTime = MPI_Wtime();
    
    srand(time(NULL) + my_rank);

    long long int toss = 100000000;
    if (my_rank == 0 && argc == 2) {
        char *ptr;
        toss = strtoll(argv[1], &ptr, 10);
    }

    MPI_Bcast(&toss, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD); 
    
    long long int in_circle = 0;
    long long int i = my_rank;
    for (; i < toss; i += comm_sz) {
        double x = RAND();
        double y = RAND();
        if (((x * x) + (y * y)) <= (double)1.0) {
            in_circle++;
        }
    }
    
    printf("process: %2d in_circle: %lld\n", my_rank, in_circle, i);
    int proc_num = comm_sz;
    while (proc_num > 1) {
        int half = proc_num / 2;
        
        /* odd process number */
        if (proc_num % 2) {
            if (my_rank < half) {
                long long int tmp;
                MPI_Recv(&tmp, 1, MPI_LONG_LONG, my_rank + half + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                in_circle += tmp;
            } else if (my_rank > half) {
                MPI_Send(&in_circle, 1, MPI_LONG_LONG, my_rank - half - 1, 0, MPI_COMM_WORLD);
                break;
            }
            proc_num = half + 1;
        }

        /* even process number */
        else {
            if (my_rank < half) {
                long long int tmp;
                MPI_Recv(&tmp, 1, MPI_LONG_LONG, my_rank + half, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                in_circle += tmp;
            } else {
                MPI_Send(&in_circle, 1, MPI_LONG_LONG, my_rank - half, 0, MPI_COMM_WORLD);
                break;
            }
            proc_num = half;
        }
    }

    if (my_rank == 0) {
        double totalTime = MPI_Wtime() - startTime;
        printf("\nFinished in time %f secs.\n\n", totalTime);
        printf("toss: %lld\tin_circle: %lld\n\n", toss, in_circle);
        double pi = 4.0 * ((double)in_circle / (double)toss);
        printf("pi = %.16lf\n", pi);
    }
    MPI_Finalize();
    return 0;
}

