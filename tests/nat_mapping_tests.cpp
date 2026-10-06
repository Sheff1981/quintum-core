#include "net/nat_mapping.hpp"

#include <cassert>

int main()
{
    using namespace quintum::net;

    NatPortMapper mapper;

    assert(!mapper.active());
    assert(mapper.method() ==
           NatMappingMethod::none);
    assert(mapper.external_port() == 0U);

    const auto invalid =
        mapper.map_tcp(0U);

    assert(!invalid.ok());
    assert(!mapper.active());

    mapper.unmap();
    assert(!mapper.active());
    return 0;
}
