// Automated correctness tests for Store (Milestone 1).
//
// This replaces manually retyping SET/GET/DEL into the CLI every time you
// want to check nothing broke. Run this after every change to store.cpp —
// it takes under a second and catches regressions immediately.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "store.hpp"
#include <thread>

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

// ---- Milestone 2: TTL and expiration ----

TEST_CASE("key with TTL is readable before it expires") {
    Store s;
    s.set("foo", "bar", std::chrono::seconds(2));
    auto result = s.get("foo");
    REQUIRE(result.has_value());
    CHECK(*result == "bar");
}

TEST_CASE("key with TTL expires after the duration passes (passive expiration)") {
    Store s;
    s.set("foo", "bar", std::chrono::seconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    auto result = s.get("foo"); // get() itself should notice it's expired
    CHECK_FALSE(result.has_value());
}

TEST_CASE("key with no TTL never expires") {
    Store s;
    s.set("permanent", "val"); // no ttl argument == no expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    auto result = s.get("permanent");
    REQUIRE(result.has_value());
    CHECK(*result == "val");
}

TEST_CASE("active expiry removes an expired key even without a GET call") {
    Store s;
    s.set("foo", "bar", std::chrono::seconds(1));
    s.start_active_expiry(std::chrono::milliseconds(200)); // sweep every 200ms

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    // Note: no s.get("foo") call anywhere above this line.
    // If len() == 0 here, the background sweep did its job on its own.
    CHECK(s.len() == 0);

    s.stop_active_expiry();
}

TEST_CASE("SET with TTL overwrites a previous non-expiring value") {
    Store s;
    s.set("foo", "permanent_value");
    s.set("foo", "temporary_value", std::chrono::seconds(1));
    auto result = s.get("foo");
    REQUIRE(result.has_value());
    CHECK(*result == "temporary_value"); // overwrite worked, and TTL applies now

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    CHECK_FALSE(s.get("foo").has_value()); // and it expires as expected
}

// ---- Milestone 3: LRU eviction ----

TEST_CASE("no eviction happens when max_keys is 0 (unlimited)") {
    Store s(0);
    for (int i = 0; i < 100; i++) s.set("k" + std::to_string(i), "v");
    CHECK(s.len() == 100);
}

TEST_CASE("inserting past capacity evicts the least recently used key") {
    Store s(3);
    s.set("a", "1");
    s.set("b", "2");
    s.set("c", "3");
    s.set("d", "4"); // over capacity -> "a" should be evicted

    CHECK_FALSE(s.get("a").has_value());
    REQUIRE(s.get("d").has_value());
    CHECK(*s.get("d") == "4");
    CHECK(s.len() == 3);
}

TEST_CASE("GET refreshes recency, changing future eviction order") {
    Store s(3);
    s.set("a", "1");
    s.set("b", "2");
    s.set("c", "3");

    s.get("a"); // touch "a" -> now most recently used; "b" becomes LRU

    s.set("d", "4"); // should evict "b", not "a"

    CHECK(s.get("a").has_value());       // survived
    CHECK_FALSE(s.get("b").has_value()); // evicted
    CHECK(s.get("c").has_value());
    CHECK(s.get("d").has_value());
}

TEST_CASE("overwriting an existing key never triggers eviction") {
    Store s(3);
    s.set("a", "1");
    s.set("b", "2");
    s.set("c", "3");
    s.set("a", "1-updated"); // overwrite, not a new key

    CHECK(s.len() == 3); // still exactly 3, nothing evicted
    REQUIRE(s.get("a").has_value());
    CHECK(*s.get("a") == "1-updated");
    CHECK(s.get("b").has_value());
    CHECK(s.get("c").has_value());
}

TEST_CASE("eviction prefers reclaiming an expired key over evicting a live one") {
    Store s(3);
    s.set("a", "1");                              // permanent
    s.set("b", "2", std::chrono::seconds(1));      // will expire soon
    s.set("c", "3");                               // permanent

    std::this_thread::sleep_for(std::chrono::milliseconds(1200)); // "b" now expired

    s.set("d", "4"); // over capacity: should reclaim expired "b", NOT evict "a"

    CHECK(s.get("a").has_value());       // still alive: was NOT wrongly evicted
    CHECK_FALSE(s.get("b").has_value()); // gone: expired, reclaimed
    CHECK(s.get("c").has_value());
    CHECK(s.get("d").has_value());
    CHECK(s.len() == 3);
}

TEST_CASE("SET on existing key refreshes recency too, not just GET") {
    Store s(3);
    s.set("a", "1");
    s.set("b", "2");
    s.set("c", "3");

    s.set("a", "1-updated"); // touch via SET, not GET

    s.set("d", "4"); // should evict "b" (LRU), not "a"

    CHECK(s.get("a").has_value());
    CHECK_FALSE(s.get("b").has_value());
}

// ---- Customizable LRU cache size ----

TEST_CASE("default constructor uses kDefaultMaxKeys") {
    Store s;
    CHECK(s.max_keys() == Store::kDefaultMaxKeys);
}

TEST_CASE("shrinking max_keys evicts immediately down to the new limit") {
    Store s(10);
    for (int i = 0; i < 10; i++) s.set("k" + std::to_string(i), "v");
    CHECK(s.len() == 10);

    s.set_max_keys(3); // shrink while already full

    CHECK(s.max_keys() == 3);
    CHECK(s.len() == 3); // trimmed immediately, no new SET needed to trigger it
}

TEST_CASE("shrinking keeps the most recently used keys, evicts the rest") {
    Store s(5);
    s.set("a", "1");
    s.set("b", "2");
    s.set("c", "3");
    s.set("d", "4");
    s.set("e", "5"); // recency order (MRU->LRU): e d c b a

    s.set_max_keys(2); // should keep "e" and "d", evict a/b/c

    CHECK(s.len() == 2);
    CHECK(s.get("e").has_value());
    CHECK(s.get("d").has_value());
    CHECK_FALSE(s.get("c").has_value());
    CHECK_FALSE(s.get("b").has_value());
    CHECK_FALSE(s.get("a").has_value());
}

TEST_CASE("growing max_keys does not evict or restore anything") {
    Store s(2);
    s.set("a", "1");
    s.set("b", "2");
    s.set("c", "3"); // evicts "a" immediately, since capacity is 2

    CHECK_FALSE(s.get("a").has_value());
    CHECK(s.len() == 2);

    s.set_max_keys(10); // grow

    CHECK(s.max_keys() == 10);
    CHECK(s.len() == 2);              // still just 2 -- "a" is NOT restored
    CHECK_FALSE(s.get("a").has_value());

    s.set("d", "4"); // now room for more, nothing should be evicted
    CHECK(s.len() == 3);
}

TEST_CASE("setting max_keys to 0 disables eviction going forward") {
    Store s(3);
    s.set("a", "1");
    s.set("b", "2");
    s.set("c", "3");

    s.set_max_keys(0); // unlimited from now on

    for (int i = 0; i < 50; i++) s.set("extra" + std::to_string(i), "v");
    CHECK(s.len() == 53); // 3 original + 50 new, nothing evicted
}