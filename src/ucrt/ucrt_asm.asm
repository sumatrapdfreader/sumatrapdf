   .386

_TEXT           segment use32 para public 'CODE'
				public  __ftol2_sse
				public  _stricmp

				extrn __imp___ftol:dword
				extrn __imp___stricmp:dword

; redirect _ftol2_sse => _ftol in msvc.dll
__ftol2_sse 	proc near
				jmp  __imp___ftol
__ftol2_sse 	endp

; redirect stricmp => _stricmp in msvc.dll

_stricmp		proc near
				jmp __imp___stricmp
_stricmp 		endp

_TEXT           ends
                end

