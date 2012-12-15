REBOL [
    Purpose: {helper script to address https://code.google.com/p/sumatrapdf/issues/detail?id=2053
It copies VisualStudio solution and project files from top directory to vs
directory, fixing as many paths as possible. It can be run using r3 binary
from http://www.rebol.com/r3/downloads.html as:
r3 ./scripts/move-vs-solutions.r}
]

; we are called as r3 ./scripts/move-vs-solutions.r and rebol sets
; current directory to where the script lives, so change where it should be
change-dir %..
if not exists? %vs/ [make-dir %vs/]

fix-vs2008-line: func [l] [
    l: replace l {RelativePath="} {RelativePath=".}
    l: replace l "obj-" "..\obj-"
    l: replace l {".\scripts} {"..\scripts}
    l: replace l {IncludeSearchPath="} {IncludeSearchPath="..\}
    l
]

fix-vs2008-vcproj: func [srcfile [file!] /local lines] [
    lines: read/lines srcfile
    forall lines [change lines fix-vs2008-line first lines]
    write/lines join %vs/ srcfile lines
]

fix-vs2010-line: func [l] [
    l: replace l {ClCompile Include="} {ClCompile Include="..\}
    l: replace l {ClInclude Include="} {ClInclude Include="..\}
    l: replace l {None Include="} {None Include="..\}
    l: replace l {ResourceCompile Include="} {ResourceCompile Include="..\}
    l: replace l {Manifest Include="} {Manifest Include="..\}
    l: replace l "obj-" "..\obj-"
    l: replace/all l {$(ProjectDir)} {$(ProjectDir)..}
    l
]

fix-vs2010-vcproj: func [srcfile [file!] /local lines] [
    lines: read/lines srcfile
    forall lines [change lines fix-vs2010-line first lines]
    write/lines join %vs/ srcfile lines
]

vs2008-files: [
    %sumatrapdf-vc2008.vcproj
    %installer-vc2008.vcproj
]

foreach file vs2008-files [fix-vs2008-vcproj file]

vs2010-files: [
    %installer-vc2010.vcxproj          %sumatrapdf-vc2010.vcxproj.filters
    %installer-vc2010.vcxproj.filters  %sumatrapdf-vc2012.vcxproj
    %sumatrapdf-vc2010.vcxproj         %sumatrapdf-vc2012.vcxproj.filters
]

cp-file: func [srcfile] [
    write join %vs/ srcfile read srcfile
]

foreach file vs2010-files [fix-vs2010-vcproj file]

sln-files: []
files: (read %./)
forall files [if find first files ".sln" [append sln-files first files]]

foreach file sln-files [cp-file file]
