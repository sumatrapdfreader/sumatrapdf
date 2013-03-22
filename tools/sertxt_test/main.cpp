#include "BaseUtil.h"
#include "FileUtil.h"
#include "SerializeTxtParser.h"
#include "SettingsSumatra.h"

int main(int argc, char **argv)
{
    size_t fileSize;
    TxtParser parser;
    char *p1 = path::JoinUtf("..", "tools", NULL);
    char *p2 = path::JoinUtf(p1, "sertxt_test", NULL);
    char *p3 = path::JoinUtf(p2, "data.txt", NULL);
    char *s = file::ReadAllUtf(p3, &fileSize);
    if (!s) {
        printf("failed to load '%s'", p3);
        goto Exit;
    }

    parser.s.Init(s, fileSize);
    bool ok = ParseTxt(parser);
    if (!ok)
        printf("%s", "failed to parse");
Exit:
    free(p3);
    free(p2);
    free(p1);
}
