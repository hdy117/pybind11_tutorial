#pragma once

#include <string>

class Point {
public:
    Point(double x, double y) : x_(x), y_(y) {}

    double x() const { return x_; }
    double y() const { return y_; }

    void translate(double dx, double dy) {
        x_ += dx;
        y_ += dy;
    }

    std::string describe() const;

private:
    double x_;
    double y_;
};
