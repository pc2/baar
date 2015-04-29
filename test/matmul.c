#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define N 512


void matmul(double A[N][N], int elementsA, double B[N][N], int elementsB, double C[N][N], int elementsC) {  
    for(int i = 0; i < N; i++)
      for(int j = 0; j < N; j++)  {
	C[i][j] = 0;
	for(int k = 0; k < N; k++)
	  C[i][j] = C[i][j] + A[i][k] * B[k][j];
	}
    
    return;
}

int main(int argc, char *argv[]) {
  double (*const A)[N][N] = (double(*)[N][N])malloc(sizeof(double)*N*N);
  double (*const B)[N][N] = (double(*)[N][N])malloc(sizeof(double)*N*N);
  double (*const C)[N][N] = (double(*)[N][N])malloc(sizeof(double)*N*N);


  // getchar(); // use in ee-parallel-mode (default)

  printf("matmul\n");
  matmul(*A, N*N, *B, N*N, *C, N*N);

  free((void*)A);
  free((void*)B);
  free((void*)C);
  return 0;
}
