EngineBase* CreateEngineForPath(Str path); // implemented by each app

bool RenderPath(Str path);
bool ListLinks(Str path);
bool SelectAllText(Str path);
bool FindText(Str path, Str term);
int PrintTocItems(TocItem* item, int depth);
bool ListToc(Str path);
bool ListProperties(Str path);
bool BenchMediabox(Str path);
bool CloneAfterDeleteTest(Str srcPath);
bool BenchIsOnNetworkDrive(Str path);
#if OS_WIN
bool CheckIsOnNetworkDrive();
#endif
bool BenchReadN(Str dir);
