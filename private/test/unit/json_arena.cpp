// json_arena.cpp — unit tests for the arena-backed JSON library (Json.hpp).
//
// Covers: bump allocation + alignment, sticky failure + null propagation,
// copy-on-grow containers, arena-as-factory + string/key copying, and
// build/serialize/parse round-tripping.

#include "doctest.h"

#include <string>
#include <cstring>

#include "Json.hpp"

// ---------------------------------------------------------------------------
// 1. Bump allocator
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("json_arena: sequential allocations advance the offset, aligned")
{
    JsonArena arena(4096);
    DOCTEST_REQUIRE_FALSE(arena.Failed());

    void* a = arena.Alloc(8, 8);
    void* b = arena.Alloc(8, 8);
    DOCTEST_REQUIRE(a != nullptr);
    DOCTEST_REQUIRE(b != nullptr);

    // b is at or beyond a + its size, and both are 8-aligned.
    //
    DOCTEST_CHECK(reinterpret_cast<char*>(b) >= reinterpret_cast<char*>(a) + 8);
    DOCTEST_CHECK(reinterpret_cast<uintptr_t>(a) % 8 == 0);
    DOCTEST_CHECK(reinterpret_cast<uintptr_t>(b) % 8 == 0);
}

DOCTEST_TEST_CASE("json_arena: exhaustion sets failed, returns null, is sticky")
{
    JsonArena arena(64);
    void* big = arena.Alloc(128, 1);
    DOCTEST_CHECK(big == nullptr);
    DOCTEST_CHECK(arena.Failed());

    // A later, small allocation also fails until reset.
    //
    void* small = arena.Alloc(1, 1);
    DOCTEST_CHECK(small == nullptr);
    DOCTEST_CHECK(arena.Failed());

    arena.Reset();
    DOCTEST_CHECK_FALSE(arena.Failed());
    DOCTEST_CHECK(arena.Alloc(1, 1) != nullptr);
}

// ---------------------------------------------------------------------------
// 2. Null tolerance + propagation
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("json_arena: writes onto null nodes are no-ops, reads return null")
{
    JSON null = JSON::Null();
    DOCTEST_CHECK(null.IsNull());

    // No crash:
    //
    null.SetNew("k", JSON::Null());
    null.Append(JSON::Null());
    DOCTEST_CHECK(null.Get("k").IsNull());
    DOCTEST_CHECK(null.GetAt(0).IsNull());
    DOCTEST_CHECK(null.Size() == 0);
    DOCTEST_CHECK(null.StringValue() == nullptr);
    DOCTEST_CHECK(null.IntegerValue() == 0);
}

DOCTEST_TEST_CASE("json_arena: mid-build exhaustion is detectable via Failed()")
{
    // Tiny arena: enough for a couple of nodes, not a whole structure.
    //
    JsonArena arena(96);
    JSON root = arena.Object();
    for (int i = 0; i < 100; ++i)
    {
        root.SetNew("some_key_name", arena.Integer(i));
    }

    DOCTEST_CHECK(arena.Failed());
}

// ---------------------------------------------------------------------------
// 3. Copy-on-grow containers
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("json_arena: array growth preserves entries and indexed access")
{
    JsonArena arena(64 * 1024);
    JSON arr = arena.Array();
    const int n = 200;
    for (int i = 0; i < n; ++i)
    {
        arr.Append(arena.Integer(i * 7));
    }

    DOCTEST_REQUIRE_FALSE(arena.Failed());
    DOCTEST_CHECK(arr.Size() == static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        DOCTEST_CHECK(arr.GetAt(i).IntegerValue() == i * 7);
    }
}

// ---------------------------------------------------------------------------
// 4. Arena-as-factory + string/key copying
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("json_arena: strings and keys are copied, survive source mutation")
{
    JsonArena arena(64 * 1024);
    JSON obj = arena.Object();

    char key[16];
    strcpy(key, "tempkey");
    char val[16];
    strcpy(val, "tempval");

    obj.SetNew(key, arena.String(val));

    // Clobber the source buffers.
    //
    memset(key, 'X', sizeof(key));
    memset(val, 'X', sizeof(val));

    JSON got = obj.Get("tempkey");
    DOCTEST_REQUIRE_FALSE(got.IsNull());
    DOCTEST_CHECK(std::string(got.StringValue()) == "tempval");
}

DOCTEST_TEST_CASE("json_arena: integer and real are distinct, strict-by-type getters")
{
    JsonArena arena(4096);
    JSON i = arena.Integer(42);
    JSON r = arena.Real(1.5);

    DOCTEST_CHECK(i.IntegerValue() == 42);
    DOCTEST_CHECK(i.RealValue() == 0.0);  // strict: integer node, RealValue() -> 0
    DOCTEST_CHECK(r.RealValue() == doctest::Approx(1.5));
    DOCTEST_CHECK(r.IntegerValue() == 0); // strict: real node, IntegerValue() -> 0
}

DOCTEST_TEST_CASE("json_arena: NumberValue reads whole-number and real tokens")
{
    JsonArena arena(4096);
    JSON root = arena.Loads("{\"intLike\":1,\"realLike\":1.0,\"expLike\":1e0}");
    DOCTEST_REQUIRE_FALSE(root.IsNull());

    DOCTEST_CHECK(root.Get("intLike").NumberValue() == doctest::Approx(1.0));
    DOCTEST_CHECK(root.Get("realLike").NumberValue() == doctest::Approx(1.0));
    DOCTEST_CHECK(root.Get("expLike").NumberValue() == doctest::Approx(1.0));

    DOCTEST_CHECK(root.Get("intLike").RealValue() == 0.0);
    DOCTEST_CHECK(root.Get("realLike").IntegerValue() == 0);
}

// ---------------------------------------------------------------------------
// 5. Parse / serialize round-trip
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("json_arena: build -> dump -> parse round-trips")
{
    JsonArena build(64 * 1024);
    JSON root = build.Object();
    root.SetNew("name", build.String("hello \"world\"\n\t/"));
    root.SetNew("count", build.Integer(-17));
    root.SetNew("ratio", build.Real(0.25));
    root.SetNew("flag", build.Boolean(true));

    JSON arr = build.Array();
    arr.Append(build.Integer(1));
    arr.Append(build.Integer(2));
    arr.Append(build.Integer(3));
    root.SetNew("list", arr);

    char* dumped = root.Dumps(0);
    DOCTEST_REQUIRE(dumped != nullptr);
    std::string text(dumped);
    free(dumped);

    JsonArena parseArena(64 * 1024);
    JSON parsed = parseArena.Loads(text.c_str());
    DOCTEST_REQUIRE_FALSE(parsed.IsNull());

    DOCTEST_CHECK(std::string(parsed.Get("name").StringValue()) == "hello \"world\"\n\t/");
    DOCTEST_CHECK(parsed.Get("count").IntegerValue() == -17);
    DOCTEST_CHECK(parsed.Get("ratio").RealValue() == doctest::Approx(0.25));
    DOCTEST_CHECK(parsed.Get("flag").BooleanValue() == true);
    DOCTEST_CHECK(parsed.Get("list").Size() == 3);
    DOCTEST_CHECK(parsed.Get("list").GetAt(1).IntegerValue() == 2);

    // Re-dump should be byte-stable.
    //
    char* dumped2 = parsed.Dumps(0);
    DOCTEST_REQUIRE(dumped2 != nullptr);
    DOCTEST_CHECK(std::string(dumped2) == text);
    free(dumped2);
}

DOCTEST_TEST_CASE("json_arena: parse rejects malformed input, accepts empty containers")
{
    JsonArena arena(4096);
    DOCTEST_CHECK(arena.Loads("").IsNull());
    DOCTEST_CHECK(arena.Loads("not json").IsNull());
    DOCTEST_CHECK(arena.Loads("{").IsNull());
    DOCTEST_CHECK(arena.Loads("{\"k\": }").IsNull());
    DOCTEST_CHECK(arena.Loads("null").IsNull());
    DOCTEST_CHECK(arena.Loads("[1,2,]").IsNull());

    JSON emptyArr = arena.Loads("[]");
    DOCTEST_CHECK_FALSE(emptyArr.IsNull());
    DOCTEST_CHECK(emptyArr.Size() == 0);

    JSON emptyObj = arena.Loads("{}");
    DOCTEST_CHECK_FALSE(emptyObj.IsNull());
    DOCTEST_CHECK(emptyObj.Size() == 0);
}

// ---------------------------------------------------------------------------
// 6. Grow-and-retry (message-thread side of the failure loop)
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("json_arena: GrowAndReset doubles capacity for retry")
{
    JsonArena arena(64);
    size_t cap0 = arena.Capacity();

    // Build something that won't fit.
    //
    JSON root = arena.Object();
    for (int i = 0; i < 50; ++i)
    {
        root.SetNew("key", arena.Integer(i));
    }
    DOCTEST_REQUIRE(arena.Failed());

    int attempts = 0;
    while (arena.Failed() && attempts < 20)
    {
        arena.GrowAndReset();
        JSON retry = arena.Object();
        for (int i = 0; i < 50; ++i)
        {
            retry.SetNew("key", arena.Integer(i));
        }
        ++attempts;
    }

    DOCTEST_CHECK_FALSE(arena.Failed());
    DOCTEST_CHECK(arena.Capacity() > cap0);
}
