#include "fox_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


enum {
    MATRIX_MAX_SIZE = 16384,
};

static int vector_new(Value *vret, Value *v, RefNode *node)
{
    Value *vv = v + 1;
    int size = (fg->stk_top - v) - 1;
    int i;
    RefVector *vec;

    if (size == 1) {
        if (fs->Value_type(v[1]) == fs->cls_list) {
            RefArray *ra = Value_vp(v[1]);
            vv = ra->p;
            size = ra->size;
        }
    }
    vec = fs->buf_new(cls_vector, sizeof(RefVector) + sizeof(double) * size);
    *vret = vp_Value(vec);
    vec->size = size;

    for (i = 0; i < size; i++) {
        const RefNode *v_type = fs->Value_type(vv[i]);
        if (v_type != fs->cls_float && v_type != fs->cls_int && v_type != fs->cls_frac) {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_number, v_type, i + 1);
            return FALSE;
        }
        vec->d[i] = fs->Value_float(vv[i]);
    }

    return TRUE;
}
static int vector_marshal_read(Value *vret, Value *v, RefNode *node)
{
    uint8_t *data;
    uint32_t size;
    int rd_size;
    int i;

    RefVector *vec;
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];

    if (!fs->stream_read_uint32(r, &size)) {
        return FALSE;
    }
    if (size > fs->max_alloc || size * sizeof(double) > fs->max_alloc) {
        fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }

    rd_size = size * sizeof(double);
    vec = fs->buf_new(cls_vector, sizeof(RefVector) + rd_size);
    *vret = vp_Value(vec);
    vec->size = size;

    data = malloc(rd_size);
    if (!fs->stream_read_data(r, NULL, (char*)data, &rd_size, FALSE, TRUE)) {
        free(data);
        return FALSE;
    }
    
    for (i = 0; i < vec->size; i++) {
        double val = bytes_to_double(data + i * 8);
        if (isnan(val)) {
            fs->throw_error_select(THROW_FLOAT_DOMAIN_ERROR);
            return FALSE;
        }
        vec->d[i] = val;
    }
    free(data);
    return TRUE;
}
static int vector_marshal_write(Value *vret, Value *v, RefNode *node)
{
    const RefVector *vec = Value_vp(*v);
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint8_t *data;
    int i;

    if (!fs->stream_write_uint32(w, vec->size)) {
        return FALSE;
    }
    data = malloc(8 * vec->size);
    for (i = 0; i < vec->size; i++) {
        double_to_bytes(data + i * 8, vec->d[i]);
    }
    if (!fs->stream_write_data(w, (const char*)data, 8 * vec->size)) {
        free(data);
        return FALSE;
    }
    free(data);

    return TRUE;
}
static int vector_dup(Value *vret, Value *v, RefNode *node)
{
    const RefVector *src = Value_vp(*v);
    int size = sizeof(RefVector) + sizeof(double) * src->size;
    RefVector *dst = fs->buf_new(cls_vector, size);
    *vret = vp_Value(dst);
    memcpy(dst->d, src->d, size);

    return TRUE;
}
static int vector_size(Value *vret, Value *v, RefNode *node)
{
    const RefVector *vec = Value_vp(*v);
    *vret = int32_Value(vec->size);
    return TRUE;
}
/**
 * 要素がすべて0.0ならtrue
 * 要素数が0ならtrue
 */
static int vector_empty(Value *vret, Value *v, RefNode *node)
{
    const RefVector *vec = Value_vp(*v);
    int i;
    for (i = 0; i < vec->size; i++) {
        if (vec->d[0] != 0.0) {
            *vret = VALUE_FALSE;
            return TRUE;
        }
    }
    *vret = VALUE_TRUE;
    return TRUE;
}
static int vector_norm(Value *vret, Value *v, RefNode *node)
{
    double norm = 0.0;
    int i;
    const RefVector *vec = Value_vp(*v);

    for (i = 0; i < vec->size; i++) {
        double d = vec->d[i];
        norm += d * d;
    }
    norm = sqrt(norm);
    *vret = fs->float_Value(norm);

    return TRUE;
}
static int vector_to_matrix(Value *vret, Value *v, RefNode *node)
{
    int i;
    const RefVector *vec = Value_vp(*v);
    RefMatrix *mat = fs->buf_new(cls_matrix, sizeof(RefMatrix) + sizeof(double) * vec->size);
    *vret = vp_Value(mat);

    if (FUNC_INT(node)) {
        // 縦
        mat->cols = 1;
        mat->rows = vec->size;
    } else {
        // 横
        mat->cols = vec->size;
        mat->rows = 1;
    }
    for (i = 0; i < vec->size; i++) {
        mat->d[i] = vec->d[i];
    }

    return TRUE;
}
static int vector_to_list(Value *vret, Value *v, RefNode *node)
{
    int i;
    const RefVector *vec = Value_vp(*v);
    RefArray *ra = fs->refarray_new(vec->size);
    *vret = vp_Value(ra);

    for (i = 0; i < vec->size; i++) {
        ra->p[i] = fs->float_Value(vec->d[i]);
    }
    return TRUE;
}
static int vector_tostr(Value *vret, Value *v, RefNode *node)
{
    StrBuf sb;
    const RefVector *vec = Value_vp(*v);
    int i;

    fs->StrBuf_init_refstr(&sb, 0);
    fs->StrBuf_add(&sb, "Vector(", 7);

    for (i = 0; i < vec->size; i++) {
        char cbuf[64];
        if (i > 0) {
            fs->StrBuf_add(&sb, ", ", 2);
        }
        sprintf(cbuf, "%g", vec->d[i]);
        fs->StrBuf_add(&sb, cbuf, -1);
    }
    fs->StrBuf_add_c(&sb, ')');
    *vret = fs->StrBuf_str_Value(&sb, fs->cls_str);

    return TRUE;
}
static int vector_hash(Value *vret, Value *v, RefNode *node)
{
    const RefVector *vec = Value_vp(*v);
    uint32_t *i32 = (uint32_t*)vec->d;
    int i, size;
    uint32_t hash = 0;

    size = vec->size * 2;   // sizeof(double) / sizeof(uint32_t)
    for (i = 0; i < size; i++) {
        hash = hash * 31 + i32[i];
    }
    *vret = int32_Value(hash & INT32_MAX);

    return TRUE;
}
static int vector_eq(Value *vret, Value *v, RefNode *node)
{
    const RefVector *v1 = Value_vp(v[0]);
    const RefVector *v2 = Value_vp(v[1]);

    if (v1->size == v2->size) {
        int i;
        for (i = 0; i < v1->size; i++) {
            if (v1->d[i] != v2->d[i]) {
                break;
            }
        }
        if (i < v1->size) {
            *vret = VALUE_FALSE;
        } else {
            *vret = VALUE_TRUE;
        }
    } else {
        *vret = VALUE_FALSE;
    }
    return TRUE;
}
static int vector_index(Value *vret, Value *v, RefNode *node)
{
    const RefVector *vec = Value_vp(*v);
    int err = FALSE;
    int64_t i = fs->Value_int64(v[1], &err);

    if (i < 0) {
        i += vec->size;
    }
    if (!err && i >= 0 && i < vec->size) {
        *vret = fs->float_Value(vec->d[i]);
    } else {
        fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], vec->size);
        return FALSE;
    }
    return TRUE;
}
static int vector_index_set(Value *vret, Value *v, RefNode *node)
{
    RefVector *vec = Value_vp(*v);
    int err = FALSE;
    int64_t i = fs->Value_int64(v[1], &err);

    if (i < 0) {
        i += vec->size;
    }
    if (i >= 0 && i < vec->size) {
        vec->d[i] = fs->Value_float(v[i]);
    } else {
        fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], vec->size);
        return FALSE;
    }
    return TRUE;
}
static int vector_minus(Value *vret, Value *v, RefNode *node)
{
    const RefVector *v1 = Value_vp(v[0]);
    int size, i;
    RefVector *vec;

    size = v1->size;
    vec = fs->buf_new(cls_vector, sizeof(RefVector) + sizeof(double) * size);
    *vret = vp_Value(vec);
    vec->size = size;

    for (i = 0; i < size; i++) {
        vec->d[i] = -v1->d[i];
    }

    return TRUE;
}
static int vector_addsub(Value *vret, Value *v, RefNode *node)
{
    double factor = (FUNC_INT(node) ? -1.0 : 1.0);
    const RefVector *v1 = Value_vp(v[0]);
    const RefVector *v2 = Value_vp(v[1]);
    int size, i;
    RefVector *vec;

    if (v1->size != v2->size) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Vector size mismatch");
        return FALSE;
    }
    size = v1->size;
    vec = fs->buf_new(cls_vector, sizeof(RefVector) + sizeof(double) * size);
    *vret = vp_Value(vec);
    vec->size = size;

    for (i = 0; i < size; i++) {
        vec->d[i] = v1->d[i] + v2->d[i] * factor;
    }

    return TRUE;
}
/**
 * 各要素をx倍する
 */
static int vector_muldiv(Value *vret, Value *v, RefNode *node)
{
    const RefVector *v1 = Value_vp(v[0]);
    double d = fs->Value_float(v[1]);
    int i;
    RefVector *vec;

    if (FUNC_INT(node)) {
        d = 1.0 / d;
    }

    vec = fs->buf_new(cls_vector, sizeof(RefVector) + sizeof(double) * v1->size);
    *vret = vp_Value(vec);
    vec->size = v1->size;

    for (i = 0; i < v1->size; i++) {
        vec->d[i] = v1->d[i] * d;
    }

    return TRUE;
}
static int matrix_new(Value *vret, Value *v, RefNode *node)
{
    const Value *vv = v + 1;
    int size = (fg->stk_top - v) - 1;
    int cols = -1;
    int i;
    RefMatrix *mat;

    // 引数が配列1つで、その配列の最初の要素も配列の場合
    if (size == 1) {
        if (fs->Value_type(v[1]) == fs->cls_list) {
            RefArray *ra = Value_vp(v[1]);
            if (ra->size > 0 && fs->Value_type(ra->p[0]) == fs->cls_list) {
                vv = ra->p;
                size = ra->size;
            }
        }
    }
    for (i = 0; i < size; i++) {
        const RefNode *v_type = fs->Value_type(vv[i]);
        RefArray *ra;

        if (v_type != fs->cls_list) {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_list, v_type, i + 1);
            return FALSE;
        }
        ra = Value_vp(vv[i]);
        if (cols == -1) {
            cols = ra->size;
        } else {
            if (cols != ra->size) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "List length mismatch");
                return FALSE;
            }
        }
    }
    mat = fs->buf_new(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size * cols);
    *vret = vp_Value(mat);
    mat->rows = size;
    mat->cols = cols;

    for (i = 0; i < size; i++) {
        RefArray *ra = Value_vp(vv[i]);
        int j;
        
        for (j = 0; j < cols; j++) {
            const RefNode *v_type = fs->Value_type(ra->p[j]);
            if (v_type != fs->cls_float && v_type != fs->cls_int && v_type != fs->cls_frac) {
                fs->throw_errorf(fs->mod_lang, "TypeError", "Number required but %n", v_type);
                return FALSE;
            }
            mat->d[i * cols + j] = fs->Value_float(ra->p[j]);
        }
    }

    return TRUE;
}
static int matrix_marshal_read(Value *vret, Value *v, RefNode *node)
{
    uint8_t *data;
    uint32_t rows, cols;
    int size;
    int rd_size;
    int i;

    RefMatrix *mat;
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];

    if (!fs->stream_read_uint32(r, &cols)) {
        return FALSE;
    }
    if (!fs->stream_read_uint32(r, &rows)) {
        return FALSE;
    }
    if (cols > MATRIX_MAX_SIZE || rows > MATRIX_MAX_SIZE) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal Matrix size (0 - %d)", MATRIX_MAX_SIZE);
        return FALSE;
    }
    size = cols * rows;
    if (size > fs->max_alloc || size * sizeof(double) > fs->max_alloc) {
        fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }

    rd_size = size * sizeof(double);
    mat = fs->buf_new(cls_matrix, sizeof(RefMatrix) + rd_size);
    *vret = vp_Value(mat);
    mat->cols = cols;
    mat->rows = rows;

    data = malloc(rd_size);
    if (!fs->stream_read_data(r, NULL, (char*)data, &rd_size, FALSE, TRUE)) {
        free(data);
        return FALSE;
    }
    
    for (i = 0; i < size; i++) {
        double val = bytes_to_double(data + i * 8);
        if (isnan(val)) {
            fs->throw_error_select(THROW_FLOAT_DOMAIN_ERROR);
            return FALSE;
        }
        mat->d[i] = val;
    }
    free(data);
    return TRUE;
}
static int matrix_marshal_write(Value *vret, Value *v, RefNode *node)
{
    const RefMatrix *mat = Value_vp(*v);
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint8_t *data;
    int size;
    int i;

    if (!fs->stream_write_uint32(w, mat->cols)) {
        return FALSE;
    }
    if (!fs->stream_write_uint32(w, mat->rows)) {
        return FALSE;
    }
    size = mat->cols * mat->rows;
    data = malloc(8 * size);
    for (i = 0; i < size; i++) {
        double_to_bytes(data + i * 8, mat->d[i]);
    }
    if (!fs->stream_write_data(w, (const char*)data, 8 * size)) {
        free(data);
        return FALSE;
    }
    free(data);

    return TRUE;
}
/*
 * 単位行列
 */
static int matrix_i(Value *vret, Value *v, RefNode *node)
{
    int64_t size = fs->Value_int64(v[1], NULL);
    RefMatrix *mat;
    double *p;

    // 0x0の行列も可能
    if (size < 0 || size > MATRIX_MAX_SIZE) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal Matrix size (0 - %d)", MATRIX_MAX_SIZE);
        return FALSE;
    }

    mat = fs->buf_new(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size * size);
    *vret = vp_Value(mat);
    mat->rows = size;
    mat->cols = size;
    p = mat->d;

    {
        int i, j;
        for (i = 0; i < size; i++) {
            for (j = 0; j < size; j++) {
                *p = (i == j ? 1.0 : 0.0);
                p++;
            }
        }
    }

    return TRUE;
}
/*
 * 0行列
 */
static int matrix_zero(Value *vret, Value *v, RefNode *node)
{
    int64_t size1 = fs->Value_int64(v[1], NULL);
    int64_t size2 = fs->Value_int64(v[2], NULL);
    RefMatrix *mat;
    double *p;

    // 0x0の行列も可能
    if (size1 < 0 || size1 > 16384 || size2 < 0 || size2 > 16384) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal Matrix size (0 - 16384)");
        return FALSE;
    }

    mat = fs->buf_new(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size1 * size2);
    *vret = vp_Value(mat);
    mat->cols = size1;
    mat->rows = size2;
    p = mat->d;

    {
        int size = size1 * size2;
        int i;
        for (i = 0; i < size; i++) {
            *p = 0.0;
            p++;
        }
    }

    return TRUE;
}
static int matrix_dup(Value *vret, Value *v, RefNode *node)
{
    const RefMatrix *src = Value_vp(*v);
    int size = sizeof(RefMatrix) + sizeof(double) * src->cols * src->rows;
    RefMatrix *dst = fs->buf_new(cls_matrix, size);
    *vret = vp_Value(dst);
    memcpy(dst->d, src->d, size);

    return TRUE;
}
static int matrix_size(Value *vret, Value *v, RefNode *node)
{
    const RefMatrix *mat = Value_vp(*v);
    int ret;
    if (FUNC_INT(node)) {
        ret = mat->cols;
    } else {
        ret = mat->rows;
    }
    *vret = int32_Value(ret);
    return TRUE;
}
static int matrix_to_vector(Value *vret, Value *v, RefNode *node)
{
    const RefMatrix *mat = Value_vp(*v);
    RefVector *vec;
    int idx = fs->Value_int64(v[1], NULL);
    int i;

    if (FUNC_INT(node)) {
        if (idx < 0) {
            idx += mat->cols;
        }
        if (idx < 0 || idx >= mat->cols) {
            fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, &v[1], mat->cols);
            return FALSE;
        }
        vec = fs->buf_new(cls_vector, sizeof(RefVector) + sizeof(double) * mat->rows);
        *vret = vp_Value(vec);
        vec->size = mat->rows;
        for (i = 0; i < mat->rows; i++) {
            vec->d[i] = mat->d[i * mat->cols + idx];
        }
    } else {
        if (idx < 0) {
            idx += mat->rows;
        }
        if (idx < 0 || idx >= mat->rows) {
            fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, &v[1], mat->rows);
            return FALSE;
        }
        vec = fs->buf_new(cls_vector, sizeof(RefVector) + sizeof(double) * mat->cols);
        *vret = vp_Value(vec);
        vec->size = mat->cols;
        for (i = 0; i < mat->cols; i++) {
            vec->d[i] = mat->d[idx * mat->cols + i];
        }
    }
    return TRUE;
}
static int matrix_to_list(Value *vret, Value *v, RefNode *node)
{
    const RefMatrix *mat = Value_vp(*v);
    RefArray *ra = fs->refarray_new(mat->rows);
    int i, j;

    *vret = vp_Value(ra);
    for (i = 0; i < mat->rows; i++) {
        RefArray *ra2 = fs->refarray_new(mat->cols);
        const double *src = &mat->d[i * mat->cols];
        ra->p[i] = vp_Value(ra2);

        for (j = 0; j < mat->cols; j++) {
            ra2->p[j] = fs->float_Value(src[j]);
        }
    }
    return TRUE;
}
static int matrix_empty(Value *vret, Value *v, RefNode *node)
{
    const RefMatrix *mat = Value_vp(*v);
    int i;
    int size = mat->cols * mat->rows;

    for (i = 0; i < size; i++) {
        if (mat->d[i] != 0.0) {
            *vret = VALUE_FALSE;
            return TRUE;
        }
    }
    *vret = VALUE_TRUE;
    return TRUE;
}
/*
 * 転置行列
 */
static int matrix_transpose(Value *vret, Value *v, RefNode *node)
{
    const RefMatrix *mat = Value_vp(*v);
    RefMatrix *mat2;
    int i, j;
    int rows = mat->rows;
    int cols = mat->cols;

    mat2 = fs->buf_new(cls_matrix, sizeof(RefMatrix) + sizeof(double) * rows * cols);
    *vret = vp_Value(mat2);
    mat2->rows = cols;
    mat2->cols = rows;

    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            mat2->d[j * rows + i] = mat->d[i * cols + j];
        }
    }

    return TRUE;
}
static int matrix_eq(Value *vret, Value *v, RefNode *node)
{
    const RefMatrix *m1 = Value_vp(v[0]);
    const RefMatrix *m2 = Value_vp(v[1]);
    int rows = m1->rows;
    int cols = m1->cols;
    int i, size;

    if (rows != m2->rows || cols != m2->cols) {
        *vret = VALUE_FALSE;
        return TRUE;
    }
    size = rows * cols;

    for (i = 0; i < size; i++) {
        if (m1->d[i] != m2->d[i]) {
            *vret = VALUE_FALSE;
            return TRUE;
        }
    }
    *vret = VALUE_TRUE;

    return TRUE;
}
static int matrix_minus(Value *vret, Value *v, RefNode *node)
{
    const RefMatrix *m1 = Value_vp(v[0]);
    int size, i;
    RefMatrix *mat;

    size = m1->rows * m1->cols;
    mat = fs->buf_new(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size);
    *vret = vp_Value(mat);
    mat->rows = m1->rows;
    mat->cols = m1->cols;

    for (i = 0; i < size; i++) {
        mat->d[i] = -m1->d[i];
    }

    return TRUE;
}
static int matrix_addsub(Value *vret, Value *v, RefNode *node)
{
    double factor = (FUNC_INT(node) ? -1.0 : 1.0);
    const RefMatrix *m1 = Value_vp(v[0]);
    const RefMatrix *m2 = Value_vp(v[1]);
    int size, i;
    RefMatrix *mat;

    if (m1->rows != m2->rows || m1->cols != m2->cols) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Matrix size mismatch");
        return FALSE;
    }
    size = m1->rows * m1->cols;
    mat = fs->buf_new(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size);
    *vret = vp_Value(mat);
    mat->rows = m1->rows;
    mat->cols = m1->cols;

    for (i = 0; i < size; i++) {
        mat->d[i] = m1->d[i] + m2->d[i] * factor;
    }

    return TRUE;
}
/**
 * 右辺が数値の場合、各要素をx倍する
 * 右辺がRefMatrixの場合、行列の積を求める
 */
static int matrix_multiple(Value *vret, Value *v, RefNode *node)
{
    RefMatrix *m1 = Value_vp(v[0]);
    const RefNode *v1_type = fs->Value_type(v[1]);
    int cols1 = m1->cols;
    int rows1 = m1->rows;

    if (v1_type == cls_matrix) {
        RefMatrix *m2 = Value_vp(v[1]);
        RefMatrix *mat;
        int cols2 = m2->cols;
        int rows2 = m2->rows;
        int i, j, k;

        if (cols1 != rows2) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "RefMatrix size mismatch");
            return FALSE;
        }
        mat = fs->buf_new(cls_matrix, sizeof(RefMatrix) + sizeof(double) * rows1 * cols2);
        *vret = vp_Value(mat);
        mat->rows = rows1;
        mat->cols = cols2;
        
        for (i = 0; i < rows1; i++) {
            for (j = 0; j < cols2; j++) {
                double sum = 0.0;
                for (k = 0; k < cols1; k++) {
                    sum += m1->d[i * cols1 + k] * m2->d[k * cols2 + j];
                }
                mat->d[i * cols2 + j] = sum;
            }
        }
    } else if (v1_type == fs->cls_float || v1_type == fs->cls_int || v1_type == fs->cls_frac) {
        int i;
        double d2 = fs->Value_float(v[1]);
        int size = cols1 * rows1;
        RefMatrix *mat = fs->buf_new(cls_matrix, sizeof(RefMatrix) + sizeof(double) * size);
        *vret = vp_Value(mat);

        mat->cols = cols1;
        mat->rows = rows1;
        for (i = 0; i < size; i++) {
            mat->d[i] = m1->d[i] * d2;
        }
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, cls_matrix, fs->cls_number, v1_type, 1);
        return FALSE;
    }

    return TRUE;
}
static double get_matrix_norm(RefMatrix *mat)
{
    double norm = 0.0;
    int i;
    int size = mat->cols * mat->rows;

    for (i = 0; i < size; i++) {
        double d = mat->d[i];
        norm += d * d;
    }
    return sqrt(norm);
}
static int matrix_norm(Value *vret, Value *v, RefNode *node)
{
    double norm = get_matrix_norm(Value_vp(*v));
    *vret = fs->float_Value(norm);
    return TRUE;
}

static int matrix_index(Value *vret, Value *v, RefNode *node)
{
    const RefMatrix *mat = Value_vp(*v);
    int64_t i = fs->Value_int64(v[1], NULL);  // 行
    int64_t j = fs->Value_int64(v[2], NULL);  // 列
    if (i < 0) {
        i += mat->rows;
    }
    if (j < 0) {
        j += mat->cols;
    }
    if (i >= 0 && i < mat->rows && j >= 0 && j < mat->cols) {
        *vret = fs->float_Value(mat->d[i * mat->cols + j]);
    } else {
        fs->throw_errorf(fs->mod_lang, "IndexError", "Invalid matrix index ((%v, %v) of (%d, %d))", v[1], v[2], mat->rows, mat->cols);
        return FALSE;
    }
    return TRUE;
}
static int matrix_index_set(Value *vret, Value *v, RefNode *node)
{
    RefMatrix *mat = Value_vp(*v);
    int64_t i = fs->Value_int64(v[1], NULL);  // 行
    int64_t j = fs->Value_int64(v[2], NULL);  // 列
    if (i < 0) {
        i += mat->rows;
    }
    if (j < 0) {
        j += mat->cols;
    }
    if (i >= 0 && i < mat->rows && j >= 0 && j < mat->cols) {
        mat->d[i * mat->cols + j] = fs->Value_float(v[3]);
    } else {
        fs->throw_errorf(fs->mod_lang, "IndexError", "Invalid matrix index ((%v, %v) of (%d, %d))", v[1], v[2], mat->rows, mat->cols);
        return FALSE;
    }
    return TRUE;
}

static int matrix_tostr(Value *vret, Value *v, RefNode *node)
{
    StrBuf sb;
    const RefMatrix *mat = Value_vp(*v);
    int cols = mat->cols;
    int size = cols * mat->rows;
    Str fmt;
    int i;

    if (fg->stk_top > v + 1) {
        fmt = fs->Value_str(v[1]);
    } else {
        fmt.p = NULL;
        fmt.size = 0;
    }

    if (Str_eq_p(fmt, "p") || Str_eq_p(fmt, "pretty")) {
        // 各列の横幅の最大値を求める
        uint8_t *max_w = malloc(cols);
        int rows = mat->rows;

        fs->StrBuf_init_refstr(&sb, 0);

        for (i = 0; i < cols; i++) {
            // i : 列
            int j;
            int max = 0;
            for (j = 0; j < rows; j++) {
                // j : 行
                char cbuf[64];
                int len;
                sprintf(cbuf, "%g", mat->d[j * cols + i]);
                len = strlen(cbuf);
                if (max < len) {
                    max = len;
                }
            }
            max_w[i] = max;
        }

        for (i = 0; i < rows; i++) {
            // i : 行
            int j;
            fs->StrBuf_add_c(&sb, '|');
            for (j = 0; j < cols; j++) {
                // j : 列
                char cbuf[64];
                sprintf(cbuf, " %*g ", max_w[j], mat->d[i * cols + j]);
                fs->StrBuf_add(&sb, cbuf, -1);
            }
            fs->StrBuf_add(&sb, "|\n", 2);
        }

        free(max_w);

        *vret = fs->StrBuf_str_Value(&sb, fs->cls_str);
    } else if (fmt.size == 0) {
        fs->StrBuf_init_refstr(&sb, 0);
        fs->StrBuf_add(&sb, "Matrix([", 8);

        for (i = 0; i < size; i++) {
            char cbuf[64];
            if (i > 0) {
                if (i % cols == 0) {
                    fs->StrBuf_add(&sb, "], [", 4);
                } else {
                    fs->StrBuf_add(&sb, ", ", 2);
                }
            }
            sprintf(cbuf, "%g", mat->d[i]);
            fs->StrBuf_add(&sb, cbuf, -1);
        }
        fs->StrBuf_add(&sb, "])", 2);

        *vret = fs->StrBuf_str_Value(&sb, fs->cls_str);
    } else {
        fs->throw_errorf(fs->mod_lang, "FormatError", "Unknown format %Q", fmt);
        return FALSE;
    }

    return TRUE;
}
static int matrix_hash(Value *vret, Value *v, RefNode *node)
{
    int i, size;
    RefMatrix *mat = Value_vp(*v);
    uint32_t *i32 = (uint32_t*)mat->d;
    uint32_t hash = 0;

    size = mat->rows * mat->cols * 2;   // sizeof(double) / sizeof(uint32_t)

    for (i = 0; i < size; i++) {
        hash = hash * 31 + i32[i];
    }
    *vret = int32_Value(hash & INT32_MAX);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int math_dot(Value *vret, Value *v, RefNode *node)
{
    const RefVector *v1 = Value_vp(v[1]);
    const RefVector *v2 = Value_vp(v[2]);
    double d = 0.0;
    int i, n;

    if (v1->size != v2->size) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Vector size mismatch");
        return FALSE;
    }
    n = v1->size;
    for (i = 0; i < n; i++) {
        d += v1->d[i] * v2->d[i];
    }
    *vret = fs->float_Value(d);

    return TRUE;
}
static int math_cross(Value *vret, Value *v, RefNode *node)
{
    const RefVector *v1 = Value_vp(v[1]);
    const RefVector *v2 = Value_vp(v[2]);
    RefVector *vr;

    if (v1->size != 3 || v2->size != 3) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Vector size must be 3");
        return FALSE;
    }
    vr = fs->buf_new(cls_vector, sizeof(RefVector) + sizeof(double) * 3);
    *vret = vp_Value(vr);
    vr->size = 3;

    vr->d[0] = v1->d[1] * v2->d[2] - v1->d[2] * v2->d[1];
    vr->d[1] = v1->d[2] * v2->d[0] - v1->d[0] * v2->d[2];
    vr->d[2] = v1->d[0] * v2->d[1] - v1->d[1] * v2->d[0];

    return TRUE;
}
static int math_outer(Value *vret, Value *v, RefNode *node)
{
    int x, y;
    const RefVector *v1 = Value_vp(v[1]);
    const RefVector *v2 = Value_vp(v[2]);
    int rows = v1->size;
    int cols = v2->size;

    RefMatrix *mat = fs->buf_new(cls_matrix, sizeof(RefMatrix) + sizeof(double) * v1->size * v2->size);
    *vret = vp_Value(mat);

    mat->rows = rows;
    mat->cols = cols;

    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            mat->d[x + y * cols] = v1->d[y] + v2->d[x];
        }
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void define_vector_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "dot", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_dot, 2, 2, NULL, cls_vector, cls_vector);

    n = fs->define_identifier(m, m, "cross", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_cross, 2, 2, NULL, cls_vector, cls_vector);

    n = fs->define_identifier(m, m, "outer", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_outer, 2, 2, NULL, cls_vector, cls_vector);
}
void define_vector_class(RefNode *m)
{
    RefNode *n;
    RefNode *cls;

    cls = fs->define_identifier(m, m, "Vector", NODE_CLASS, 0);
    cls_vector = cls;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, vector_new, 0, -1, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    fs->define_native_func_a(n, vector_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, vector_hash, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);

    n = fs->define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_dup, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, vector_size, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, vector_empty, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "norm", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, vector_norm, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "to_row_matrix", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_to_matrix, 0, 0, (void*)FALSE);
    n = fs->define_identifier(m, cls, "to_col_matrix", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_to_matrix, 0, 0, (void*)TRUE);
    n = fs->define_identifier(m, cls, "to_list", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_to_list, 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_index, 1, 1, NULL, fs->cls_int);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_index_set, 2, 2, NULL, fs->cls_int, fs->cls_number);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_eq, 1, 1, NULL, cls_vector);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_PLUS], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_native_return_this, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_MINUS], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_minus, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_addsub, 1, 1, (void*) FALSE, cls_vector);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_addsub, 1, 1, (void*) TRUE, cls_vector);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_muldiv, 1, 1, (void*) FALSE, fs->cls_float);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_DIV], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, vector_muldiv, 1, 1, (void*) TRUE, fs->cls_float);

    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "Matrix", NODE_CLASS, 0);
    cls_matrix = cls;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, matrix_new, 0, -1, NULL);
    n = fs->define_identifier(m, cls, "unit", NODE_NEW_N, 0);
    fs->define_native_func_a(n, matrix_i, 1, 1, NULL, fs->cls_int);
    n = fs->define_identifier(m, cls, "zero", NODE_NEW_N, 0);
    fs->define_native_func_a(n, matrix_zero, 2, 2, NULL, fs->cls_int, fs->cls_int);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    fs->define_native_func_a(n, matrix_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, matrix_hash, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = fs->define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_dup, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, matrix_empty, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "rows", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, matrix_size, 0, 0, (void*)FALSE);
    n = fs->define_identifier(m, cls, "cols", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, matrix_size, 0, 0, (void*)TRUE);

    n = fs->define_identifier(m, cls, "get_row_vector", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_to_vector, 1, 1, (void*)FALSE, fs->cls_int);
    n = fs->define_identifier(m, cls, "get_col_vector", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_to_vector, 1, 1, (void*)TRUE, fs->cls_int);
    n = fs->define_identifier(m, cls, "to_list", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_to_list, 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_index, 2, 2, NULL, fs->cls_int, fs->cls_int);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_index_set, 3, 3, NULL, fs->cls_int, fs->cls_int, fs->cls_number);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_eq, 1, 1, NULL, cls_matrix);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_PLUS], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, math_native_return_this, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_MINUS], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_minus, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_ADD], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_addsub, 1, 1, (void*) FALSE, cls_matrix);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_SUB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_addsub, 1, 1, (void*) TRUE, cls_matrix);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_multiple, 1, 1, NULL, NULL);
    n = fs->define_identifier(m, cls, "transpose", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_transpose, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "norm", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, matrix_norm, 0, 0, NULL);

    fs->extends_method(cls, fs->cls_obj);
}
