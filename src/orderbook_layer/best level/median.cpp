#include "median.hpp"

void RollingMedian100::add(double v)
{
    if(values.size() == 100)
    {
        values.erase(values.begin());   // remove oldest
    }

    values.push_back(v);
}


double RollingMedian100::median() const
{
    if(values.empty())
        return 1;

    std::vector<double> temp = values;

    size_t n = temp.size() / 2;

    std::nth_element(temp.begin(), temp.begin() + n, temp.end());

    double med = temp[n];

    if(temp.size() % 2 == 0)
    {
        std::nth_element(temp.begin(), temp.begin() + n - 1, temp.end());
        med = (med + temp[n - 1]) * 0.5;
    }

    return med;
}