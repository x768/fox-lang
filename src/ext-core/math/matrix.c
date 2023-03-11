#include "fox_math.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double *factors;
    double *tmp;
    int *pivots;
    int n;
} Solver;

static void solver_init(Solver *sol, const RefMatrix *mat)
{
    int i;
    int mat_size = sizeof(double) * mat->rows * mat->cols;
    sol->factors = malloc(mat_size);
    memcpy(sol->factors, mat->d, mat_size);

    sol->n = mat->rows;
    sol->pivots = malloc(sizeof(double) * sol->n);

    for (i = 0; i < sol->n; i++) {
        sol->pivots[i] = i;
    }

    sol->tmp = malloc(sizeof(double) * sol->n);
}

static void solver_step(Solver *sol)
{
    int n = sol->n;
    int i, j, k;

    for (j = 0; j < n; j++) {
        int p;
        for (i = 0; i < n; i++) {
            sol->tmp[i] = sol->factors[i * n + j];
        }

        for (i = 0; i < n; i++) {
            int kmax = i < j ? i : j;
            double s = 0.0;
            for (k = 0; k < kmax; k++) {
                s += sol->factors[i * n + k] * sol->tmp[k];
            }
            sol->tmp[i] -= s;
            sol->factors[i * n + j] = sol->tmp[i];
        }

        p = j;
        for (i = j + 1; i < n; i++) {
            if (abs(sol->tmp[i]) > abs(sol->tmp[p])) {
                p = i;
            }
        }

        if (p != j) {
            for (k = 0; k < n; k++) {
                double temp = sol->factors[p * n + k];
                sol->factors[p * n + k] = sol->factors[j * n + k];
                sol->factors[j * n + k] = temp;
            }
            sol->pivots[j] = p;
        }

        if (j < n && sol->factors[j * n + j] != 0.0) {
            for (i = j + 1; i < n; i++) {
                sol->factors[i * n + j] = sol->factors[i * n + j] / sol->factors[j * n + j];
            }
        }
    }
}
static void solver_close(Solver *sol)
{
    free(sol->factors);
    free(sol->pivots);
    free(sol->tmp);
}

static double get_determinant(Solver *sol)
{
    int n = sol->n;
    int det = 1.0;
    for (int j = 0; j < n; j++) {
        double d = sol->factors[j * n + j];
        det *= (sol->pivots[j] != j) ? -d : d;
    }
    return det;
}

static void set_identity(RefMatrix *mat)
{
    int i, j;
    for (i = 0; i < mat->rows; i++) {
        int k = i * mat->cols;
        for (j = 0; j < mat->cols; j++) {
            mat->d[k + j] = i == j ? 1.0 : 0.0;
        }
    }
}

// result->rows == sol->n
static void solver_solve(Solver *sol, RefMatrix *result)
{
    int n = sol->n;
    int cols = result->cols;
    int i, j, k, p;
    for (i = 0; i < n; i++) {
        if (sol->pivots[i] == i) {
            continue;
        }

        p = sol->pivots[i];
        for (j = 0; j < cols; j++) {
            double temp = result->d[p * cols + j];
            result->d[p * cols + j] = result->d[i * cols + j];
            result->d[i * cols + j] = temp;
        }
    }

    for (k = 0; k < n; k++) {
        for (i = k + 1; i < n; i++) {
            for (j = 0; j < cols; j++) {
                double temp = result->d[k * cols + j] * sol->factors[i * n + k];
                result->d[i * cols + j] = result->d[i * cols + j] - temp;
            }
        }
    }

    for (k = n - 1; k >= 0; k--) {
        for (j = 0; j < cols; j++) {
            result->d[k * cols + j] = result->d[k * cols + j] / sol->factors[k * n + k];
        }

        for (i = 0; i < k; i++) {
            for (j = 0; j < cols; j++) {
                double temp = result->d[k * cols + j] * sol->factors[i * n + k];
                result->d[i * cols + j] = result->d[i * cols + j] - temp;
            }
        }
    }
}

// mat->rows == mat->cols
double matrix_determinant(const RefMatrix *mat)
{
    Solver sol;
    double det;
    solver_init(&sol, mat);
    solver_step(&sol);
    det = get_determinant(&sol);
    solver_close(&sol);
    return det;
}

// result->cols == result->rows == mat->cols == mat->rows
void matrix_inverse(RefMatrix *result, const RefMatrix *mat)
{
    Solver sol;
    solver_init(&sol, mat);
    solver_step(&sol);
    set_identity(result);
    solver_solve(&sol, result);
    solver_close(&sol);
}

int matrix_is_number(const RefMatrix *mat)
{
    int i;
    int size = mat->rows * mat->cols;
    for (i = 0; i < size; i++) {
        double d = mat->d[i];
        if (isnan(d)) {
            return FALSE;
        }
    }
    return TRUE;
}
