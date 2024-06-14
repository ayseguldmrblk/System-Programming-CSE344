#ifndef COMPLEX_MATRIX_H
#define COMPLEX_MATRIX_H

#include <complex.h>
#include <stdlib.h>

typedef struct {
    long long int real;
    long long int imag;
} complex_t;

typedef struct {
    int rows;
    int cols;
    complex_t **data;
} complex_matrix_t;

complex_matrix_t *create_matrix(int rows, int cols);
void free_matrix(complex_matrix_t *matrix);
void generate_random_matrix(complex_matrix_t *matrix);
complex_matrix_t *pseudo_inverse(complex_matrix_t *matrix);

#endif
