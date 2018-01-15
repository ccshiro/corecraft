#ifndef SHARED__ESTD__ALGORITHM_H
#define SHARED__ESTD__ALGORITHM_H

#include <functional>

namespace estd
{
template <typename T>
T rangify(T lower, T upper, T val)
{
    if (std::less<T>()(val, lower))
        return lower;
    if (std::greater<T>()(val, upper))
        return upper;
    return val;
}

template <typename T>
bool in_range(T lower, T upper, T val)
{
    if (std::less<T>()(val, lower))
        return false;
    if (std::greater<T>()(val, upper))
        return false;
    return true;
}

} // namespace estd

#endif // SHARED__ESTD__ALGORITHM_H
