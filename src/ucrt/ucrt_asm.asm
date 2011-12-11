   .386

_TEXT           segment use32 para public 'CODE'
				public  __ftol2_sse

				extrn __imp___ftol:dword

; redirect _ftol2_sse --> __imp___ftol in msvc.dll
__ftol2_sse 	proc near
				jmp  __imp___ftol
__ftol2_sse 	endp

_TEXT           ends
                end
