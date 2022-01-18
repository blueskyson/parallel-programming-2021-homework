#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

#define MAX 100000000    // max value of all elements
int N;              // array size
int *arr;           // array
int *tmp;           // temporary array

/* print array */
void parr(int a[], int n)
{
    int i;
    for (i = 0; i < n; i++) {
        printf("%d ", a[i]);
    }
    printf("\n");
}

/* pthread variables */
struct child_args {
    int *arr, *tmp;
    int start, end;
};

void *count_sort(void *args)
{
    struct child_args *arg = (struct child_args*)args;
    int start = arg->start;
    int end = arg->end;
    int *a = arg->arr;
    int *temp = arg->tmp;
    
    int i, j, count;
    for (i = start; i < end; i++) {
        count = 0;
        for (j = 0; j < N; j++) {
            if (a[j] < a[i])
                count++;
            else if (a[j] == a[i] && j < i)
                count++;
        }
        temp[count] = a[i]; 
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    struct timespec begin, end;
    clock_gettime(CLOCK_REALTIME, &begin);
    N = 10;
    
    // Initialize pthread variables
    long n_thread = 4;
    if (argc >= 2) {
        n_thread = strtol(argv[1], NULL, 10);
        if (n_thread < 1) {
            printf("n_thread must > 0! abort.\n");
            return 0;
        }
    }

    if (argc >= 3) {
        N = strtol(argv[2], NULL, 10);
        if (N < 1) {
            printf("N must > 0! abort.\n");
            return 0;
        }
    }

    // initialize array
    srand(time(NULL));
    arr = (int*)malloc(N * sizeof(int));
    tmp = (int*)malloc(N * sizeof(int));
    int i;
    for (i = 0; i < N; i++) {
        arr[i] = rand() % MAX;
    }
    //parr(arr, N);
    
    // initialize threads
    pthread_t child[n_thread];
    struct child_args args[n_thread];
    int step = N / n_thread;
    int stepsum = 0;
    for (i = 0; i < n_thread; i++) {
        args[i].arr = arr;
        args[i].tmp = tmp;
        args[i].start = stepsum;
        stepsum += step;
        args[i].end = stepsum;
    }
    args[n_thread - 1].end = N;
    
    // parallel count sort
    for (int i = 0; i < n_thread; i++) {
        pthread_create(&child[i], NULL, count_sort, (void*) &args[i]);
    }
    for (i = 0; i < n_thread; i++) {
        pthread_join(child[i], NULL);
    }
    memcpy(arr, tmp, N * sizeof(int));

    //parr(arr, N);
    free(arr);
    free(tmp);
    clock_gettime(CLOCK_REALTIME, &end);
    long seconds = end.tv_sec - begin.tv_sec;
    long nanoseconds = end.tv_nsec - begin.tv_nsec;
    double elapsed = seconds + nanoseconds*1e-9;
    printf("time in sec: %.10f\n", elapsed);
    return 0;
}