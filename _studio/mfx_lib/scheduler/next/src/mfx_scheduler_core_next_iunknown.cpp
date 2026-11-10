// Copyright (c) 2009-2026 Intel Corporation
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

#include <mfx_scheduler_core_next.h>

#include <vm_interlocked.h>

namespace mfx_sched_next
{

void *mfxSchedulerCoreNext::QueryInterface(const MFX_GUID &guid)
{
    // Specific interface is required
    if (MFXIScheduler_GUID == guid)
    {
        // increment reference counter
        vm_interlocked_inc32(&m_refCounter);

        return (MFXIScheduler *) this;
    }

    if (MFXIScheduler2_GUID == guid)
    {
        // increment reference counter
        vm_interlocked_inc32(&m_refCounter);

        return (MFXIScheduler2 *) this;
    }

    // it is unsupported interface
    return NULL;

} // void *mfxSchedulerCoreNext::QueryInterface(const MFX_GUID &guid)

void mfxSchedulerCoreNext::AddRef(void)
{
    // increment reference counter
    vm_interlocked_inc32(&m_refCounter);

} // void mfxSchedulerCoreNext::AddRef(void)

void mfxSchedulerCoreNext::Release(void)
{
    // decrement reference counter
    vm_interlocked_dec32(&m_refCounter);

    if (0 == m_refCounter)
    {
        delete this;
    }

} // void mfxSchedulerCoreNext::Release(void)

mfxU32 mfxSchedulerCoreNext::GetNumRef(void) const
{
    return m_refCounter;

} // mfxU32 mfxSchedulerCoreNext::GetNumRef(void) const

MFXIUnknown* CreateScheduler(void)
{
    return new mfxSchedulerCoreNext;

} // MFXIUnknown* CreateScheduler(void)

} // namespace mfx_sched_next
