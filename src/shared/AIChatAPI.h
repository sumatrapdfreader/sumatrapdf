struct AIChatTabState;

AIChatProvider* GetCustomAIProvider();
bool AIChatStartAPI(MainWindow* win, Str input, Str model);
bool AIChatRunAPISync(Str message, Str& text, Str& error);
void AIChatCancelAPI(AIChatTabState* state);
void AIChatFinishAPI(AIChatTabState* state);
void AIChatResetAPI(AIChatTabState* state);
