/* ****************************************************************************** *\

Copyright (C) 2012-2022 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_trace_dump_common.cpp

\* ****************************************************************************** */

#include "mfx_trace_dump.h"

std::string DumpContext::dump(const std::string structName, const mfxBitstream& bitstream)
{
    std::string str = "mfxBitstream " + structName + " : addr[" + ToHexFormatString(&bitstream) + "]" + " size[" + ToString(sizeof(bitstream)) + "]" + "\n";
    str += structName + ".EncryptedData=" + ToString(bitstream.EncryptedData) + "\n";
    str += dump_mfxExtParams(structName, bitstream);
    str += structName + ".CodecId=" + ToString(bitstream.CodecId) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(bitstream.reserved) + "\n";
    str += structName + ".DecodeTimeStamp=" + ToString(bitstream.DecodeTimeStamp) + "\n";
    str += structName + ".TimeStamp=" + ToString(bitstream.TimeStamp) + "\n";
    str += structName + ".Data=" + ToHexFormatString(bitstream.Data) + "\n";
    str += structName + ".DataOffset=" + ToString(bitstream.DataOffset) + "\n";
    str += structName + ".DataLength=" + ToString(bitstream.DataLength) + "\n";
    str += structName + ".MaxLength=" + ToString(bitstream.MaxLength) + "\n";
    str += structName + ".PicStruct=" + ToString(bitstream.PicStruct) + "\n";
    str += structName + ".FrameType=" + ToString(bitstream.FrameType) + "\n";
    str += structName + ".DataFlag=" + ToString(bitstream.DataFlag) + "\n";
    str += structName + ".reserved2=" + ToString(bitstream.reserved2) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtBuffer& extBuffer)
{
    std::string str;
    std::string bufid_str = GetBufferIdInString(extBuffer.BufferId);
    if (!bufid_str.empty())
        str += structName + ".BufferId=" + bufid_str + "\n";
    else
        str += structName + ".BufferId=" + ToString(extBuffer.BufferId) + "\n";
    str += structName + ".BufferSz=" + ToString(extBuffer.BufferSz);
    return str;
}