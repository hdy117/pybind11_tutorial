#include "point.h"

#include <sstream>

std::string Point::describe() const {
    std::ostringstream out;
    out << "Point(x=" << x_ << ", y=" << y_ << ")";
    return out.str();
}
