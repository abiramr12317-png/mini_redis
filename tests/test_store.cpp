// Automated correctness tests for Store (Milestone 1).
//
// This replaces manually retyping SET/GET/DEL into the CLI every time you
// want to check nothing broke. Run this after every change to store.cpp —
// it takes under a second and catches regressions immediately.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "store.hpp"

using miniredis::Store;

TEST_CASE("SET then GET returns the value") {
    Store s;
    s.set("foo", "bar");
    auto result = s.get("foo");
    REQUIRE(result.has_value());
    CHECK(*result == "bar");
}

TEST_CASE("GET on a missing key returns nullopt") {
    Store s;
    auto result = s.get("does_not_exist");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SET overwrites an existing key") {
    Store s;
    s.set("foo", "bar");
    s.set("foo", "baz");
    auto result = s.get("foo");
    REQUIRE(result.has_value());
    CHECK(*result == "baz");
}

TEST_CASE("DEL on an existing key returns true and removes it") {
    Store s;
    s.set("foo", "bar");
    CHECK(s.del("foo") == true);
    CHECK_FALSE(s.get("foo").has_value());
}

TEST_CASE("DEL on a missing key returns false") {
    Store s;
    CHECK(s.del("never_existed") == false);
}

TEST_CASE("DEL twice: second call returns false") {
    Store s;
    s.set("foo", "bar");
    CHECK(s.del("foo") == true);
    CHECK(s.del("foo") == false); // already gone
}

TEST_CASE("empty string is a valid value, distinct from missing") {
    Store s;
    s.set("foo", "");
    auto result = s.get("foo");
    REQUIRE(result.has_value());   // key exists...
    CHECK(*result == "");          // ...with an empty value
    // ...which must be distinguishable from a key that was never set:
    CHECK_FALSE(s.get("bar").has_value());
}

TEST_CASE("many distinct keys don't collide or clobber each other") {
    Store s;
    const int N = 10000;
    for (int i = 0; i < N; i++) {
        s.set("key" + std::to_string(i), "val" + std::to_string(i));
    }
    // spot-check a sample rather than all 10000 (fast, still catches bugs)
    for (int i = 0; i < N; i += 137) {
        auto result = s.get("key" + std::to_string(i));
        REQUIRE(result.has_value());
        CHECK(*result == "val" + std::to_string(i));
    }
    CHECK(s.get("key" + std::to_string(N)).has_value() == false); // one past the end
}

TEST_CASE("re-inserting after delete works correctly") {
    Store s;
    s.set("foo", "v1");
    s.del("foo");
    s.set("foo", "v2");
    auto result = s.get("foo");
    REQUIRE(result.has_value());
    CHECK(*result == "v2");
}

TEST_CASE("keys and values are case-sensitive") {
    Store s;
    s.set("Foo", "bar");
    CHECK_FALSE(s.get("foo").has_value());
    CHECK(s.get("Foo").has_value());
}