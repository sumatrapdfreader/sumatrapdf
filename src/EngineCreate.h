/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsSupportedFileType(Kind kind, bool enableEngineEbooks);

EngineBase* CreateEngine(const WCHAR* filePath, PasswordUI* pwdUI = nullptr, bool enableChmEngine = true,
                         bool enableEngineEbooks = true);

bool EngineSupportsAnnotations(EngineBase*);
bool EngineGetAnnotations(EngineBase*, Vec<Annotation*>*);
bool EngineHasUnsavedAnnotations(EngineBase*);
Annotation* EngineGetAnnotationAtPos(EngineBase*, int pageNo, PointF pos, AnnotationType* allowedAnnots);
