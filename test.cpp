

// C++ file that only does include headers and enable the static_assert checks

#define NCT_TESTS
#include "ct_list.hpp"

#include "hash/fnv1a.hpp"

static_assert(std::is_same_v<decltype(neam::ct::hash::fnv1a<32>("coucou")), uint32_t>, "fnv1a: BAD return type");
static_assert(std::is_same_v<decltype(neam::ct::hash::fnv1a<64>("coucou")), uint64_t>, "fnv1a: BAD return type");
static_assert(neam::ct::hash::fnv1a<32>("coucou") == 0x0e3318cf, "fnv1a: BAD");
static_assert(neam::ct::hash::fnv1a<64>("coucou") == 0xa9a87dbe4c4ab80f, "fnv1a: BAD");
static_assert(neam::ct::hash::fnv1a<32>("") == 0x811c9dc5, "fnv1a: BAD");
static_assert(neam::ct::hash::fnv1a<64>("") == 0xcbf29ce484222325, "fnv1a: BAD");

