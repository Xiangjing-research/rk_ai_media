#include "scalar.h"

// 创建Scalar
Scalar Scalar_create(double v0, double v1, double v2, double v3) {
    Scalar scalar;
    scalar.val[0] = v0;
    scalar.val[1] = v1;
    scalar.val[2] = v2;
    scalar.val[3] = v3;
    return scalar;
}

// 访问元素
double Scalar_get(const Scalar* scalar, int idx) {
    if (idx < 0 || idx >= 4) {
        fprintf(stderr, "Index out of bounds\n");
        return -1;
    }
    return scalar->val[idx];
}

// 设置元素
void Scalar_set(Scalar* scalar, int idx, double value) {
    if (idx < 0 || idx >= 4) {
        fprintf(stderr, "Index out of bounds\n");
        return;
    }
    scalar->val[idx] = value;
}

// 打印Scalar
void Scalar_print(const Scalar* scalar) {
    printf("(%f, %f, %f, %f)\n", scalar->val[0], scalar->val[1], scalar->val[2], scalar->val[3]);
}