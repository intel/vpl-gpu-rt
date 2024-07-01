// Copyright (c) 2007-2021 Intel Corporation
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

#include <mfxvideo.h>
#include <mfx_session.h>

mfxStatus MFXQueryIMPL(mfxSession session, mfxIMPL *impl)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);
    mfxIMPL currentImpl;

    // check error(s)
    if (0 == session)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
    if (0 == impl)
    {
        return MFX_ERR_NULL_PTR;
    }

    // set the library's type
    if (0 == session->m_adapterNum || session->m_adapterNum >= 4)
    {
        currentImpl = MFX_IMPL_HARDWARE;
    }
    else
    {
        currentImpl = (mfxIMPL) (MFX_IMPL_HARDWARE2 + (session->m_adapterNum - 1));
    }
    currentImpl |= session->m_implInterface;

    // save the current implementation type
    *impl = currentImpl;
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "Out:  impl = ", MFX_TRACE_FORMAT_I, *impl);
    return MFX_ERR_NONE;

} // mfxStatus MFXQueryIMPL(mfxSession session, mfxIMPL *impl)

mfxStatus MFXQueryVersion(mfxSession session, mfxVersion *pVersion)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);
    if (0 == session)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
    if (0 == pVersion)
    {
        return MFX_ERR_NULL_PTR;
    }

    // set the library's version
    *pVersion = session->m_versionToReport;
    MFX_TRACE_2("Out:  MFX_API version = ", "%d.%d", pVersion->Major, pVersion->Minor);

    return MFX_ERR_NONE;

} // mfxStatus MFXQueryVersion(mfxSession session, mfxVersion *pVersion)
