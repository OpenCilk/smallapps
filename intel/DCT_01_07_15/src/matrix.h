#define ALIGNMENT                                                              \
  32 // Set to 16 bytes for SSE architectures and 32 bytes for AVX architectures
#include <cstring>
#include <iostream>

using namespace std;
class matrix_serial {
public:
  float *ptr;
  int row_size;
  matrix_serial(int);
  ~matrix_serial();
  matrix_serial(matrix_serial &);
  void create_identity();
  matrix_serial operator*(matrix_serial &);
  matrix_serial &operator=(const matrix_serial &);
  matrix_serial operator-(int);
  void transpose(matrix_serial &);
  // friend ostream& operator<<(ostream &, matrix_serial &);
};
