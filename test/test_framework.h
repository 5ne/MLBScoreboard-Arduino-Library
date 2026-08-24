#ifndef MLB_TEST_FRAMEWORK_H
#define MLB_TEST_FRAMEWORK_H

// A deliberately tiny, dependency-free test harness -- no GoogleTest/Catch2
// install required, just a standard C++ compiler. Good enough for the
// pure-logic functions under test/ (see README.md in this directory for
// why only those are covered, and how to run this).

#include <cstdio>
#include <cstring>
#include <vector>

namespace testing
{

struct TestCase
{
    const char *name;
    void (*fn)();
};

inline std::vector<TestCase> &registry()
{
    static std::vector<TestCase> r;
    return r;
}

struct Registrar
{
    Registrar(const char *name, void (*fn)())
    {
        registry().push_back({name, fn});
    }
};

inline int &failureCount()
{
    static int n = 0;
    return n;
}

inline int runAll()
{
    int failedTests = 0;
    for (auto &tc : registry())
    {
        failureCount() = 0;
        tc.fn();
        if (failureCount() == 0)
        {
            printf("  [PASS] %s\n", tc.name);
        }
        else
        {
            printf("  [FAIL] %s (%d assertion%s failed)\n", tc.name, failureCount(), failureCount() == 1 ? "" : "s");
            failedTests++;
        }
    }
    printf("\n%d test case%s, %d failed.\n", (int)registry().size(), registry().size() == 1 ? "" : "s", failedTests);
    return failedTests;
}

} // namespace testing

#define MLB_TEST_CONCAT_INNER(a, b) a##b
#define MLB_TEST_CONCAT(a, b) MLB_TEST_CONCAT_INNER(a, b)

#define TEST(name)                                                                                                   \
    static void MLB_TEST_CONCAT(mlbtest_, name)();                                                                   \
    static ::testing::Registrar MLB_TEST_CONCAT(mlbtest_reg_, name)(#name, MLB_TEST_CONCAT(mlbtest_, name));          \
    static void MLB_TEST_CONCAT(mlbtest_, name)()

#define EXPECT_TRUE(cond)                                                                                             \
    do                                                                                                                \
    {                                                                                                                 \
        if (!(cond))                                                                                                  \
        {                                                                                                             \
            printf("    FAILED: %s:%d: EXPECT_TRUE(%s)\n", __FILE__, __LINE__, #cond);                                \
            ::testing::failureCount()++;                                                                              \
        }                                                                                                             \
    } while (0)

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

#define EXPECT_EQ(a, b)                                                                                                \
    do                                                                                                                \
    {                                                                                                                 \
        if (!((a) == (b)))                                                                                             \
        {                                                                                                             \
            printf("    FAILED: %s:%d: EXPECT_EQ(%s, %s)\n", __FILE__, __LINE__, #a, #b);                             \
            ::testing::failureCount()++;                                                                              \
        }                                                                                                             \
    } while (0)

#define EXPECT_STREQ(a, b)                                                                                             \
    do                                                                                                                \
    {                                                                                                                 \
        const char *mlb_a_ = (a);                                                                                    \
        const char *mlb_b_ = (b);                                                                                    \
        if (strcmp(mlb_a_, mlb_b_) != 0)                                                                              \
        {                                                                                                             \
            printf("    FAILED: %s:%d: EXPECT_STREQ(%s=\"%s\", %s=\"%s\")\n", __FILE__, __LINE__, #a, mlb_a_, #b,     \
                   mlb_b_);                                                                                           \
            ::testing::failureCount()++;                                                                              \
        }                                                                                                             \
    } while (0)

#endif // MLB_TEST_FRAMEWORK_H
