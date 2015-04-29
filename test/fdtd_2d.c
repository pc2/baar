#include <stdio.h>
#include <stdlib.h>

#define STEPS 10000
#define N 2000

void fdtd_2d(double ex[N][N], int elements_ex, double ey[N][N], int elements_ey, double hz[N][N], int elements_hz, double _fict_[STEPS], int elements__fict_) {
  for(int t = 0; t < STEPS; t++) {
      for (int j = 0; j < N; j++)
	ey[0][j] = _fict_[t];
      for (int i = 1; i < N; i++)
	for (int j = 0; j < N; j++)
	  ey[i][j] = ey[i][j] - 0.5*(hz[i][j]-hz[i-1][j]);
      for (int i = 0; i < N; i++)
	for (int j = 1; j < N; j++)
	  ex[i][j] = ex[i][j] - 0.5*(hz[i][j]-hz[i][j-1]);
      for (int i = 0; i < N - 1; i++)
	for (int j = 0; j < N - 1; j++)
	  hz[i][j] = hz[i][j] - 0.7 *  (ex[i][j+1] - ex[i][j] +
				       ey[i+1][j] - ey[i][j]);
   }
}

int main(int argc, char *argv[]) {
  double (*ex)[N][N] = (double(*)[N][N])malloc(sizeof(double)*N*N);
  double (*ey)[N][N] = (double(*)[N][N])malloc(sizeof(double)*N*N);
  double (*hz)[N][N] = (double(*)[N][N])malloc(sizeof(double)*N*N);
  double (*_fict_)[STEPS] = (double(*)[STEPS])malloc(sizeof(double)*STEPS);

  for (int i = 0; i < STEPS; i++)
    (*_fict_)[i] = (double) i;
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++) {
	(*ex)[i][j] = ((double) i*(j+1)) / N;
	(*ey)[i][j] = ((double) i*(j+2)) / N;
	(*hz)[i][j] = ((double) i*(j+3)) / N;
     }

  // getchar(); // use in ee-parallel-mode (default)
  
  printf("Running fdtd_2d kernel %d times\n", STEPS);

  fdtd_2d(*ex, N*N, *ey, N*N, *hz, N*N, *_fict_, STEPS);

  free((void*)ex);
  free((void*)ey);
  free((void*)hz);
  free((void*)_fict_);
  return 0;
}
