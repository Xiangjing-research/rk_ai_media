#ifndef RECT_H
#define RECT_H

#include <iostream>

template <typename T>
class Rect {
public:
    T x, y, width, height;

    // 默认构造函数
    Rect() : x(0), y(0), width(0), height(0) {}

    // 参数化构造函数
    Rect(T _x, T _y, T _width, T _height) : x(_x), y(_y), width(_width), height(_height) {}

    // 面积计算
    T area() const {
        return width * height;
    }

    // 判断是否为空
    bool empty() const {
        return width <= 0 || height <= 0;
    }

    // 判断是否包含点
    bool contains(T _x, T _y) const {
        return (_x >= x) && (_x < x + width) && (_y >= y) && (_y < y + height);
    }

    // 打印Rect
    void print() const {
        std::cout << "Rect(" << x << ", " << y << ", " << width << ", " << height << ")" << std::endl;
    }
};

#endif // RECT_H