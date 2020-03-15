#ifndef _NSEEL_FUNC_REF_H_
#define _NSEEL_FUNC_REF_H_

#include "ns-eel.h"
#define TMP_MKSTR2(x) #x
#define TMP_MKSTR(x) TMP_MKSTR2(x)

const char *nseel_builtin_function_reference=
    "while\texpression\tExecutes expression until expression evaluates to zero" 
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN
                ", or until " TMP_MKSTR(NSEEL_LOOPFUNC_SUPPORT_MAXLEN) "iterations occur"
#endif
                ". An alternate and more useful syntax is while (expression) ( statements ), which evaluates statements after "
                "every non-zero evaluation of expression.\0"
    "loop\tcount,expression\tEvaluates count once, and then executes expression count"
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN
    ", but not more than " TMP_MKSTR(NSEEL_LOOPFUNC_SUPPORT_MAXLEN) ","
#endif
    " times.\0"
    "sin\tangle\tReturns the sine of the angle specified (specified in radians -- to convert from degrees to radians, multiply by $pi/180, or 0.017453).\0"
    "cos\tangle\tReturns the cosine of the angle specified (specified in radians).\0"
    "tan\tangle\tReturns the tangent of the angle specified (specified in radians).\0"
    "sqrt\tvalue\tReturns the square root of the parameter. If the parameter is negative, the return value is undefined.\0"
    "log\tvalue\tReturns the natural logarithm (base e) of the parameter. If the value is not greater than 0, the return value is undefined.\0"
    "log10\tvalue\tReturns the base-10 logarithm of the parameter. If the value is not greater than 0, the return value is undefined.\0"
    "asin\tvalue\tReturns the arc sine of the value specified (return value is in radians). If the parameter is not between -1.0 and 1.0 inclusive, the return value is undefined.\0"
    "acos\tvalue\tReturns the arc cosine of the value specified (return value is in radians). If the parameter is not between -1.0 and 1.0 inclusive, the return value is undefined.\0"
    "atan\tvalue\tReturns the arc tangent of the value specified (return value is in radians). If the parameter is not between -1.0 and 1.0 inclusive, the return value is undefined.\0"
    "atan2\tnumerator,denominator\tReturns the arc tangent of the numerator divided by the denominator, allowing the denominator to be 0, and using their signs to produce a more meaningful result.\0"
    "exp\texponent\tReturns the number e ($e, approximately 2.718) raised to the parameter-th power. This function is significantly faster than pow() or the ^ operator.\0"
    "abs\tvalue\tReturns the absolute value of the parameter.\0"
    "sqr\tvalue\tReturns the square of the parameter (similar to value*value, but only evaluating value once).\0"
    "min\t&value,&value\tReturns (by reference) the minimum value of the two parameters. Since min() returns by reference, expressions such as min(x,y) = 5 are possible.\0"
    "max\t&value,&value\tReturns (by reference) the maximum value of the two parameters. Since max() returns by reference, expressions such as max(x,y) = 5 are possible.\0"
    "sign\tvalue\tReturns 1.0 if the parameter is greater than 0, -1.0 if the parameter is less than 0, or 0 if the parameter is 0.\0"
    "floor\tvalue\tReturns the value rounded to the next lowest integer (floor(3.9)==3, floor(-3.1)==-4).\0"
    "ceil\tvalue\tReturns the value rounded to the next highest integer (ceil(3.1)==4, ceil(-3.9)==-3).\0"
    "invsqrt\tvalue\tReturns a fast inverse square root (1/sqrt(x)) approximation of the parameter.\0"
    "freembuf\taddress\tHints the runtime that memory above the address specified may no longer be used. The runtime may, at its leisure, choose to lose the contents of memory above the address specified.\0"
    "memcpy\tdest,src,length\tCopies length items of memory from src to dest. Regions are permitted to overlap.\0"
    "memset\toffset,value,length\tSets length items of memory at offset to value.\0"
    "mem_get_values\toffset, ...\tReads values from memory starting at offset into variables specified. Slower than regular memory reads for less than a few variables, faster for more than a few. Undefined behavior if used with more than 32767 variables.\0"
    "mem_set_values\toffset, ...\tWrites values to memory starting at offset from variables specified. Slower than regular memory writes for less than a few variables, faster for more than a few. Undefined behavior if used with more than 32767 variables.\0"
    "stack_push\t&value\tPushes value onto the user stack, returns a reference to the parameter.\0"
    "stack_pop\t&value\tPops a value from the user stack into value, or into a temporary buffer if value is not specified, and returns a reference to where the stack was popped. Note that no checking is done to determine if the stack is empty, and as such stack_pop() will never fail.\0"
    "stack_peek\tindex\tReturns a reference to the item on the top of the stack (if index is 0), or to the Nth item on the stack if index is greater than 0. \0"
    "stack_exch\t&value\tExchanges a value with the top of the stack, and returns a reference to the parameter (with the new value).\0"
#ifdef NSEEL_EEL1_COMPAT_MODE
    "rand\tmax\tReturns a psuedorandom non-negative integer number less than the parameter.\0"
    "sigmoid\tvalue,constraint\tReturns 1.0/(1+exp(-x * (constraint))), or 0 if a divide by 0 would occur.\0"
    "band\tx,y\tReturns 1 if both x and y evaluate to nonzero, 0 if otherwise. Both parameters are always evaluated.\0"
    "bor\tx,y\tReturns 1 if either x or y evaluate to nonzero, 0 if otherwise. Both parameters are always evaluated.\0"
    "exec2\tx,y\tEvaluates x, then evaluates and returns y.\0"
    "exec3\tx,y,z\tEvaluates x, evaluates y, then evaluates and returns z.\0"
#else
    "rand\t[max]\tReturns a psuedorandom real number between 0 and the parameter, inclusive. If the parameter is omitted or less than 1.0, 1.0 is used as a maximum instead.\0"

#endif
;
#undef TMP_MKSTR

#endif
