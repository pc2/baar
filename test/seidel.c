#include <stdio.h>
#include <stdlib.h>

#define N 4000
#define STEPS 1000

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

  for (i = 0; i < N; i++)
    for (j = 0; j < N; j++)
      (*A)[i][j] = ((double) i*(j+2) + 2) / N;
    
  //  getchar(); // use in ee-parallel-mode (default)
  
  printf("Running seidel kernel %d times\n", STEPS);

  seidel(*A, N*N);

  return 0;
}
