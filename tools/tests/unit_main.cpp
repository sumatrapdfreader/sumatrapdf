#include "BaseUtil.h"
#include "UtAssert.h"

// in UnitTests.cpp
extern void BaseUtils_UnitTests();

// TODO: move this into UnitTests.cpp ?
int main(int argc, char **argv)
{
    int total, failed;
    BaseUtils_UnitTests();
    utassert_get_stats(&total, &failed);
    if (failed > 0) {
        fprintf(stderr, "Failed %d (of %d) tests\n", failed, total);
        return failed;
    }
    printf("Passed all %d tests\n", total);
    return 0;
}
