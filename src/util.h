#pragma once

#include <vector>

using namespace std;

template<typename T> vector<T> slice(vector<T> const& v, int m, int n)
{
    auto first = v.cbegin() + m;
    auto last = v.cbegin() + n + 1;
    vector<T> vec(first, last);
    return vec;
}
