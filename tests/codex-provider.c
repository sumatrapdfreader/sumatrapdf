#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "app-server") == 0) {
        return 1;
    }
    const char* prompt = argc > 1 ? argv[argc - 1] : "";
    if (strstr(prompt, "codex-turn-failed")) {
        puts("{\"type\":\"turn.failed\",\"error\":{\"message\":\"Selected model requires a newer CLI\"}}");
        return 1;
    }
    if (strstr(prompt, "codex-error")) {
        // like codex 0.x: a retry, then the error twice, the second time in turn.failed
        const char* api = "{\\\"type\\\":\\\"error\\\",\\\"status\\\":400,\\\"error\\\":{\\\"message\\\":"
                          "\\\"Selected model is unavailable\\\"}}";
        puts("{\"type\":\"error\",\"message\":\"Reconnecting... 1/5\"}");
        printf("{\"type\":\"error\",\"message\":\"%s\"}\n", api);
        printf("{\"type\":\"turn.failed\",\"error\":{\"message\":\"%s\"}}\n", api);
        return 1;
    }
    if (strstr(prompt, "codex-plain-error")) {
        fputs("error: unexpected argument '--json'\n", stderr);
        return 1;
    }
    const char* model = "missing";
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "-m") == 0) model = argv[i + 1];
    }
    printf("{\"type\":\"item.completed\",\"item\":{\"type\":\"agent_message\",\"text\":\"%s\"}}\n", model);
    return 0;
}
