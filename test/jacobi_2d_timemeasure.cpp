#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <iostream>

#define STEPS 10000
#define N 4000


void jacobi_2d(double A[N][N], int elementsA) {
  int t, i, j;
  double (*B)[N][N] = (double(*)[N][N])malloc(sizeof(double)*N*N);

  for (t = 0; t < STEPS; t++)
    {
      for (i = 1; i < N - 1; i++)
	for (j = 1; j < N - 1; j++)
	  (*B)[i][j] = 0.2 * (A[i][j] + A[i][j-1] + A[i][1+j] + A[1+i][j] + A[i-1][j]);
      for (i = 1; i < N-1; i++)
	for (j = 1; j < N-1; j++)
	  A[i][j] = (*B)[i][j];
    }
    
    free((void*)B);
    return;
}

int main(int argc, char *argv[]) {   
  std::cout << "S" << STEPS << "N" << N << std::endl;
  for (int r = 0; r < 15; r++) {
  int i, j;
  double (*A)[N][N] = (double(*)[N][N])malloc(sizeof(double)*N*N);

  for (i = 0; i < N; i++)
    for (j = 0; j < N; j++)
	(*A)[i][j] = ((double) i*(j+2) + 2) / N;


    auto StartTime = std::chrono::high_resolution_clock::now();
    jacobi_2d(*A, N*N);  
    auto EndTime = std::chrono::high_resolution_clock::now();
    const auto TimeDiff = EndTime - StartTime;
    std::cout <<  std::chrono::duration_cast<std::chrono::microseconds>(TimeDiff).count() << std::endl;

  free((void*)A);
  }
  return 0;
}
