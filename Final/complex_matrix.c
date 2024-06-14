#include "complex_matrix.h"
#include <math.h>
#include <stdio.h>

complex_matrix_t *create_matrix(int rows, int cols) {
    complex_matrix_t *matrix = (complex_matrix_t *)malloc(sizeof(complex_matrix_t));
    matrix->rows = rows;
    matrix->cols = cols;
    matrix->data = (complex_t **)malloc(rows * sizeof(complex_t *));
    for (int i = 0; i < rows; i++) {
        matrix->data[i] = (complex_t *)malloc(cols * sizeof(complex_t));
    }
    return matrix;
}

void free_matrix(complex_matrix_t *matrix) {
    for (int i = 0; i < matrix->rows; i++) {
        free(matrix->data[i]);
    }
    free(matrix->data);
    free(matrix);
}

void generate_random_matrix(complex_matrix_t *matrix) {
    for (int i = 0; i < matrix->rows; i++) {
        for (int j = 0; j < matrix->cols; j++) {
            matrix->data[i][j].real = rand() % 1000;
            matrix->data[i][j].imag = rand() % 1000;
        }
    }
}

complex_matrix_t *transpose(complex_matrix_t *matrix) {
    complex_matrix_t *transposed = create_matrix(matrix->cols, matrix->rows);
    for (int i = 0; i < matrix->rows; i++) {
        for (int j = 0; j < matrix->cols; j++) {
            transposed->data[j][i] = matrix->data[i][j];
        }
    }
    return transposed;
}

complex_matrix_t *multiply(complex_matrix_t *a, complex_matrix_t *b) {
    if (a->cols != b->rows) return NULL;
    complex_matrix_t *result = create_matrix(a->rows, b->cols);
    for (int i = 0; i < result->rows; i++) {
        for (int j = 0; j < result->cols; j++) {
            result->data[i][j].real = 0;
            result->data[i][j].imag = 0;
            for (int k = 0; k < a->cols; k++) {
                long long int real_part = a->data[i][k].real * b->data[k][j].real - a->data[i][k].imag * b->data[k][j].imag;
                long long int imag_part = a->data[i][k].real * b->data[k][j].imag + a->data[i][k].imag * b->data[k][j].real;
                result->data[i][j].real += real_part;
                result->data[i][j].imag += imag_part;
            }
        }
    }
    return result;
}

complex_matrix_t *pseudo_inverse(complex_matrix_t *matrix) {
    // Transpose the matrix
    complex_matrix_t *transposed = transpose(matrix);

    // Multiply matrix and its transpose
    complex_matrix_t *product = multiply(transposed, matrix);

    // Simplified "inverse" by dividing each element (this is NOT a true matrix inverse)
    for (int i = 0; i < product->rows; i++) {
        for (int j = 0; j < product->cols; j++) {
            if (product->data[i][j].real != 0) {
                product->data[i][j].real = 1 / product->data[i][j].real;
            }
            if (product->data[i][j].imag != 0) {
                product->data[i][j].imag = 1 / product->data[i][j].imag;
            }
        }
    }

    free_matrix(transposed);
    return product;
}
