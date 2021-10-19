// Copyright (c) 2003-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __VM_STRINGS_H__
#define __VM_STRINGS_H__

#include <stdint.h>
#include "ippdefs.h"


# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <stdarg.h>
# include <ctype.h>
# include <dirent.h>
# include <errno.h>

typedef char vm_char;

#define VM_STRING(x) x

#define vm_string_printf    printf
#define vm_string_sprintf   sprintf
#define vm_string_snprintf  snprintf
#define vm_string_vsprintf  vsprintf
#define vm_string_vsnprintf vsnprintf

#define vm_string_strcat    strcat
#define vm_string_strcpy    strcpy
#define vm_string_strncpy   strncpy

#define vm_string_strlen    strlen
#define vm_string_strcmp    strcmp
#define vm_string_strncmp   strncmp
#define vm_string_stricmp   strcmp


#define vm_string_atol      atol
#define vm_string_atoi      atoi

#define vm_string_strtod    strtod
#define vm_string_strtol    strtol

#define vm_string_sscanf    sscanf
#define vm_string_vsscanf   vsscanf
#define vm_string_strchr    strchr

typedef DIR* vm_findptr;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

int32_t vm_string_vprintf(const vm_char *format, va_list argptr);

#ifdef __cplusplus
}
#endif /* __cplusplus */



typedef int error_t;


#endif /* __VM_STRINGS_H__ */
