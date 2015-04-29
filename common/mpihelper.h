#ifndef MPIHELPER_H
#define MPIHELPER_H
 
#define MAX_NUMBER_OF_ARGUMENTS 200
#define MPI_SERVER_TAG 123
#define MPI_SERVER_RANK 1

#define MPI_CLIENT_RANK 0
#define MPI_CLIENT_TAG 123

#define MPI_MAX_RECV_BUFFER_SIZE 2100000000

enum MPI_DATATYPE_ENUM {
	ENUM_MPI_DATATYPE_NULL       = 0,
	ENUM_MPI_CHAR                = 1,
	ENUM_MPI_SHORT               = 2,
	ENUM_MPI_INT                 = 3,
	ENUM_MPI_LONG                = 4,
	ENUM_MPI_UNSIGNED_CHAR       = 5,
	ENUM_MPI_UNSIGNED_SHORT      = 6,
	ENUM_MPI_UNSIGNED            = 7,
	ENUM_MPI_UNSIGNED_LONG       = 8,
	ENUM_MPI_FLOAT               = 9,
	ENUM_MPI_DOUBLE              = 10,
	ENUM_MPI_LONG_DOUBLE         = 11,
	ENUM_MPI_LONG_LONG           = 12,

	ENUM_MPI_INTEGER             = 13,
	ENUM_MPI_REAL                = 14,
	ENUM_MPI_DOUBLE_PRECISION    = 15,
	ENUM_MPI_COMPLEX             = 16,
	ENUM_MPI_DOUBLE_COMPLEX      = 17,
	ENUM_MPI_LOGICAL             = 18,
	ENUM_MPI_CHARACTER           = 19,
	ENUM_MPI_INTEGER1            = 20,
	ENUM_MPI_INTEGER2            = 21,
	ENUM_MPI_INTEGER4            = 22,
	ENUM_MPI_INTEGER8            = 23,
	ENUM_MPI_REAL4               = 24,
	ENUM_MPI_REAL8               = 25,
	ENUM_MPI_REAL16              = 26,

	ENUM_MPI_BYTE                = 27,
	ENUM_MPI_PACKED              = 28,
	ENUM_MPI_UB                  = 29,
	ENUM_MPI_LB                  = 30,

	ENUM_MPI_FLOAT_INT           = 31,
	ENUM_MPI_DOUBLE_INT          = 32,
	ENUM_MPI_LONG_INT            = 33,
	ENUM_MPI_2INT                = 34,
	ENUM_MPI_SHORT_INT           = 35,
	ENUM_MPI_LONG_DOUBLE_INT     = 36,

	ENUM_MPI_2REAL               = 37,
	ENUM_MPI_2DOUBLE_PRECISION   = 38,
	ENUM_MPI_2INTEGER            = 39
};

struct ArgumentList{
  int sizeOfArg;
  int typeofArg;
};

#endif // MPIHELPER_H