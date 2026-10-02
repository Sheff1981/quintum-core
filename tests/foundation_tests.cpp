#include "core/types.hpp"

#include <algorithm>
#include <cassert>

int main()
{
    quintum::Hash256 hash{};
    static_assert(hash.size() == 32);
    assert(std::all_of(hash.begin(), hash.end(), [](quintum::Byte b) { return b == 0; }));
    return 0;
}
