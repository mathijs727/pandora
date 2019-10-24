#pragma once
#include "stream/templates.h"
#include <cstdint>

namespace stream {

namespace detail {
    template <typename T>
    struct DummyT {
    };
}

struct Handle {
    uint32_t index;
};

template <typename T>
using CacheHandle = StronglyTypedAlias<Handle, detail::DummyT<T>>;

}