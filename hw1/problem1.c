/* circuitSatifiability.c solves the Circuit Satisfiability
 *  Problem using a brute-force sequential solution.
 *
 *   The particular circuit being tested is "wired" into the
 *   logic of function checkCircuit. All combinations of
 *   inputs that satisfy the circuit are printed.
 *
 *   16-bit version by Michael J. Quinn, Sept 2002.
 *   Extended to 32 bits by Joel C. Adams, Sept 2013.
 */

#include <stdio.h>     // printf()
#include <limits.h>    // UINT_MAX
#include <mpi.h>

int checkCircuit (int, int);

int main (int argc, char *argv[]) {
    /* MPI initialize */
    int comm_sz, my_rank;
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    double startTime = 0.0, totalTime = 0.0;
    startTime = MPI_Wtime();
    
    int count = 0;
    int i = my_rank;
    for (; i <= USHRT_MAX; i += comm_sz) {
        count += checkCircuit(my_rank, i);
    }

    int proc_num = comm_sz;
    while (proc_num > 1) {
        int half = proc_num / 2;

        /* odd process number */
        if (proc_num % 2) {
            if (my_rank < half) {
                int tmp;
                MPI_Recv(&tmp, 1, MPI_INT, my_rank + half + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                count += tmp;
                proc_num = half + 1;
            } else if (my_rank == half) {    
                proc_num = half + 1;
            } else {
                MPI_Send(&count, 1, MPI_INT, my_rank - half - 1, 0, MPI_COMM_WORLD);
                break;
            }
            
        }
        
        /* even process number */
        else {
            if (my_rank < half) {
                int tmp;
                MPI_Recv(&tmp, 1, MPI_INT, my_rank + half, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                count += tmp;
                proc_num = half;
            } else {
                MPI_Send(&count, 1, MPI_INT, my_rank - half, 0, MPI_COMM_WORLD);
                break;
            }
        }
    }

    totalTime = MPI_Wtime() - startTime;
    if (my_rank == 0) {
        printf("Process %d finished in time %f secs.\n", my_rank, totalTime);
        fflush(stdout);
        printf("\nA total of %d solutions were found.\n\n", count);
    }
    MPI_Finalize();

    return 0;
}

/* EXTRACT_BIT is a macro that extracts the ith bit of number n.
 *
 * parameters: n, a number;
 *             i, the position of the bit we want to know.
 *
 * return: 1 if ith bit of n is 1; 0 otherwise 
 */

#define EXTRACT_BIT(n,i) ( (n & (1<<i) ) ? 1 : 0)


/* checkCircuit() checks the circuit for a given input.
 * parameters: id, the id of the process checking;
 *             bits, the (long) rep. of the input being checked.
 *
 * output: the binary rep. of bits if the circuit outputs 1
 * return: 1 if the circuit outputs 1; 0 otherwise.
 */

#define SIZE 16

int checkCircuit (int id, int bits) {
   int v[SIZE];        /* Each element is a bit of bits */
   int i;

   for (i = 0; i < SIZE; i++)
     v[i] = EXTRACT_BIT(bits,i);

   if (  (v[0] || v[1]) && (!v[1] || !v[3]) && (v[2] || v[3])
       && (!v[3] || !v[4]) && (v[4] || !v[5])
       && (v[5] || !v[6]) && (v[5] || v[6])
       && (v[6] || !v[15]) && (v[7] || !v[8])
       && (!v[7] || !v[13]) && (v[8] || v[9])
       && (v[8] || !v[9]) && (!v[9] || !v[10])
       && (v[9] || v[11]) && (v[10] || v[11])
       && (v[12] || v[13]) && (v[13] || !v[14])
       && (v[14] || v[15])  )
   {
      printf ("%d) %d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d \n", id,
         v[15],v[14],v[13],v[12],
         v[11],v[10],v[9],v[8],v[7],v[6],v[5],v[4],v[3],v[2],v[1],v[0]);
      fflush (stdout);
      return 1;
   } else {
      return 0;
   }
}

