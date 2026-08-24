#include "test_framework.h"

int main()
{
    printf("MLBScoreboard logic tests\n");
    printf("==========================\n");
    int failed = testing::runAll();
    return failed == 0 ? 0 : 1;
}
