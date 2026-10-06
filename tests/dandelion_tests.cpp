#include "net/dandelion.hpp"

#include <cassert>
#include <cstdint>

int main()
{
    using namespace quintum::net;

    assert(!dandelion_should_fluff(0U, 0U));
    assert(dandelion_should_fluff(0U, 10U));
    assert(!dandelion_should_fluff(10U, 10U));
    assert(dandelion_should_fluff(999U, 100U));

    assert(dandelion_embargo_delay(
               0U,
               10U,
               20U) == 10U);
    assert(dandelion_embargo_delay(
               20U,
               10U,
               20U) == 30U);
    assert(dandelion_embargo_delay(
               123U,
               7U,
               0U) == 7U);

    const auto random =
        secure_dandelion_random();
    if (random) {
        const auto delay =
            dandelion_embargo_delay(
                *random,
                10U,
                20U
            );
        assert(delay >= 10U);
        assert(delay <= 30U);
    }

    return 0;
}
