# SumatraPDF show

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `SumatraPDF show [options] file.pdf ( trailer | xref | pages | grep | outline | js | form | <path> ) *`

`show` is advanced tool for inspecting internal structure of a PDF file.

Example information.

## trailer

```
<<
  /ID [ <FD33BEA90DDE04BE16E7189D77594BCA> <614CE953883E263E0CE7B36E3DAD0D99> ]
  /Root 145 0 R
  /Info 144 0 R
  /Type /XRef
  /Size 161
  /W [ 1 4 2 ]
  /DecodeParms <<
    /Columns 7
    /Predictor 12
  >>
  /Filter /FlateDecode
  /Length 288
>>
```

**trailer.Root**

```
145 0 obj
<<
  /PageLayout /TwoPageRight
  /Type /Catalog
  /PageLabels <<
    /Kids [ 131 0 R 132 0 R 133 0 R 134 0 R 135 0 R 136 0 R 137 0 R
        138 0 R 139 0 R 140 0 R 141 0 R 142 0 R 143 0 R ]
  >>
  /ViewerPreferences <<
    /Direction /L2R
  >>
  /Pages 43 0 R
  /Version /1.6
  /AcroForm 148 0 R
  /Outlines 149 0 R
  /Metadata 160 0 R
>>
endobj
```

**trailer.Root.Outlines.First**

```
PS C:\Users\kjk\Downloads> 150 0 obj
<<
  /Parent 149 0 R
  /Title (de DAME.)
  /Dest 151 0 R
  /C [ 0 0 0 ]
  /Next 152 0 R
>>
endobj
```

## xref

```
00000: 0000000000 65535 f
00001: 0000000017 00000 n
00002: 0000610334 00000 n
00003: 0000000147 00000 o
```

## pages

```
page 1 = 3 0 R
page 2 = 8 0 R
page 3 = 13 0 R
```

## outline

```
|    "de DAME."      #page=1&zoom=75,0,0
|       "DATHIS."       #page=3&zoom=75,0,0
|       "DAVID D'ANGERS."       #page=5&zoom=75,0,0
```

## All options

```
Usage: SumatraPDF show [options] file.pdf ( trailer | xref | pages | grep | outline | js | form | <path> ) *
  -p -    password
  -o -    output file
  -e      leave stream contents in their original form
  -b      print only stream contents, as raw binary data
  -g      print only object, one line per object, suitable for grep
  -r      force repair before showing any objects
  -L      show object labels
  path: path to an object, starting with either an object number,
          'pages', 'trailer', or a property in the trailer;
          path elements separated by '.' or '/'. Path elements must be
          array index numbers, dictionary property names, or '*'.
```
