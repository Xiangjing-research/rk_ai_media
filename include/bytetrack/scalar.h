#ifndef SCALAR_H
#define SCALAR_H

#include <stdio.h>
#include <stdarg.h>

typedef struct {
    double val[4];
} Scalar;

// 创建Scalar
Scalar Scalar_create(double v0, double v1, double v2, double v3);

// 访问元素
double Scalar_get(const Scalar* scalar, int idx);

// 设置元素
void Scalar_set(Scalar* scalar, int idx, double value);

// 打印Scalar
void Scalar_print(const Scalar* scalar);

#endif // SCALAR_H