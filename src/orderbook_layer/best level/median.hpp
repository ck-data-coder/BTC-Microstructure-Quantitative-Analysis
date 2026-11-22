#pragma once
#include <vector>
#include <algorithm>

class RollingMedian100 {
public:

    RollingMedian100()
    {
        values.reserve(100);
    }

    void add(double v);

    double median() const;

private:

    std::vector<double> values;
};