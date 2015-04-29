#include <stdio.h>
#include <stdlib.h>

// #define N 4000
// #define STEPS 1000

#define N 4000
//#define STEPS 1000
#define STEPS 10

#define PAD64 0

#if PAD64
#define WIDTHP ((((N*sizeof(double))+63)/64)*(64/sizeof(double)))
#else
#define WIDTHP N
#endif

void seidel(double A[N][N], int elementsA) {
    int t,i,j;

    for (t=0; t<STEPS; t++)
        for (i=1; i < N-1; i++)
            for (j=1; j < N-1; j++)
                A[i][j] = (A[i-1][j-1] + A[i-1][j] + A[i-1][j+1]
                           + A[i][j-1] + A[i][j] + A[i][j+1]
                           + A[i+1][j-1] + A[i+1][j] + A[i+1][j+1])/9.0;

    return;
}

int main(int argc, char *argv[])
{
    int i, j;
    double (*A)[N][N] = (double (*)[N][N])malloc(sizeof(double)*N*N);
    double (*B)[N][N] = (double (*)[N][N])malloc(sizeof(double)*N*N);

    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++)
            (*A)[i][j] = (*B)[i][j] =  ((double) i*(j+2) + 2) / N;

//  getchar(); // use in ee-parallel-mode (default)

    printf("Running seidel kernel %d times\n", STEPS);

    seidel(*A, N*N);


    printf("Running seidel kernel %d times on the CPU\n", STEPS);

    int t;
    for (t=0; t<STEPS; t++)
        for (i=1; i < N-1; i++)
            for (j=1; j < N-1; j++)
                (*B)[i][j] = ((*B)[i-1][j-1] + (*B)[i-1][j] + (*B)[i-1][j+1]
                              + (*B)[i][j-1] + (*B)[i][j] + (*B)[i][j+1]
                              + (*B)[i+1][j-1] + (*B)[i+1][j] + (*B)[i+1][j+1])/9.0;


    int error=0;

//compare the results
    for (i = 0; i < N; i++) {
        if(error)
            break;
        for (j = 0; j < N; j++) {
//             printf(" %lf %lf; ",(*A)[i][j],(*B)[i][j]);
// 	    if(j%100==0){
// 	      printf("\n\n\n\n");
//
// 	    }
            if((*A)[i][j]!=(*B)[i][j]) {
                error=1;
                printf("\n Check Failed %d %d",i,j);
                break;
            }
        }
//         printf("\n\n\n\n");
    }

    if(!error) {
        printf("\n Check Passed");
    }


    return 0;
}
