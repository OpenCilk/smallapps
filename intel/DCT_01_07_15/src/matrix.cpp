#include "matrix.h"
#include <cstdlib>
#include <cstring>


matrix_serial::matrix_serial(int size) {
  ptr = (float *)aligned_alloc(ALIGNMENT, sizeof(float) * size * size);
  memset(ptr, 0, sizeof(float) * size * size);
  row_size = size;
}
matrix_serial::~matrix_serial() { free(ptr); }

void matrix_serial::create_identity() {
  int size = row_size * row_size;
  for (int i = 0; i < row_size; i++) {
    *(ptr + (i * row_size) + i) = 1;
  }
  return;
}

matrix_serial::matrix_serial(matrix_serial &m) {
  ptr = (float *)aligned_alloc(ALIGNMENT,
                               sizeof(float) * m.row_size * m.row_size);
  memcpy(ptr, m.ptr, sizeof(float) * m.row_size * m.row_size);
  row_size = m.row_size;
}

matrix_serial matrix_serial::operator*(matrix_serial &y) {
  int size = y.row_size;
  matrix_serial temp(size);
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      temp.ptr[(i * size) + j] = 0;
      for (int k = 0; k < size; k++)
        temp.ptr[(i * size) + j] +=
            (ptr[(i * size) + k] * y.ptr[(k * size) + j]);
    }
  }
  return temp;
}
matrix_serial &matrix_serial::operator=(const matrix_serial &temp) {
  int row_stride = temp.row_size;
  int size = (row_stride * row_stride);
  for (int i = 0; i < row_stride; i++) {
    for (int j = 0; j < row_stride; j++)
      ptr[(i * row_stride) + j] = temp.ptr[(i * row_stride) + j];
  }
  return *this;
}
matrix_serial matrix_serial::operator-(int num) {
  matrix_serial temp(row_size);
  int size = (row_size * row_size);
  for (int i = 0; i < size; i++)
    temp.ptr[i] -= num;
  return temp;
}
void matrix_serial::transpose(matrix_serial &output) {
  int size = row_size;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++)
      output.ptr[(j * size) + i] = ptr[(i * size) + j];
  }
  return;
}

/*	 ostream& operator<<(ostream &out, matrix_serial &x){
                for(int i = 0; i < x.row_size; i++)
                {
                        for(int j = 0; j < x.row_size; j++)
                                out<<x.ptr[(i * x.row_size) + j]<<"\t";
                        out<<"\n";
                }
        return out;
}*/
