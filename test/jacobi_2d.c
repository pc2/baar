#include <stdio.h>
#include <stdlib.h>

#define STEPS 10000
#define N 4000

void jacobi_2d(double A[N][N], int elementsA) {
  double (*B)[N][N] = (double(*)[N][N])malloc(sizeof(double)*N*N);

  for (int t = 0; t < STEPS; t++)
    {
      for (int i = 1; i < N - 1; i++)
	for (int j = 1; j < N - 1; j++)
	  (*B)[i][j] = 0.2 * (A[i][j] + A[i][j-1] + A[i][1+j] + A[1+i][j] + A[i-1][j]);
      for (int i = 1; i < N-1; i++)
	for (int j = 1; j < N-1; j++)
	  A[i][j] = (*B)[i][j];
    }
    
    free((void*)B);
}

int main(int argc, char *argv[]) {
  double (*A)[N][N] = (double(*)[N][N])malloc(sizeof(double)*N*N);

  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
	(*A)[i][j] = ((double) i*(j+2) + 2) / N;

    getchar(); // use in ee-parallel-mode (default)
  
  printf("Running jacobi_2d kernel %d times\n", STEPS);

  jacobi_2d(*A, N*N);

  free((void*)A);
  return 0;
}
