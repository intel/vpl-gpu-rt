// Copyright (c) 2012-2018 Intel Corporation
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

#ifndef __MFX_PLATFORM_DEFS_H__
#define __MFX_PLATFORM_DEFS_H__


static const GUID GUID_NULL =
{ 0x00000000, 0x0000, 0x0000,{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

typedef int D3DFORMAT;

#ifndef D3DDDIFORMAT
#define D3DDDIFORMAT        D3DFORMAT
#endif

typedef int             BOOL;
typedef char            CHAR;
typedef unsigned char   BYTE;
typedef short           SHORT;
typedef int             INT;


typedef unsigned char  UCHAR;
typedef unsigned short USHORT;
typedef unsigned int   UINT;

typedef int            LONG;
typedef unsigned int   ULONG;
typedef unsigned int   DWORD;

#endif // __MFX_PLATFORM_DEFS_H__
