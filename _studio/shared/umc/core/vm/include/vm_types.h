// Copyright (c) 2003-2019 Intel Corporation
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

#ifndef __VM_TYPES_H__
#define __VM_TYPES_H__

#include <stdint.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <ippdefs.h>
#include <ippcore.h>

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

# include "sys/vm_types_linux32.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef enum e_vm_Status
{
    VM_OK                           = 0,    /* no error */
    VM_OPERATION_FAILED             =-999,
    VM_NOT_INITIALIZED              =-998,
    VM_TIMEOUT                      =-987,
    VM_NOT_ENOUGH_DATA              =-996,  /* not enough input data */

    VM_NULL_PTR                     =-995,
    VM_SO_CANT_LOAD                 =-994,
    VM_SO_INVALID_HANDLE            =-993,
    VM_SO_CANT_GET_ADDR             =-992

} vm_status;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VM_TYPES_H__ */
