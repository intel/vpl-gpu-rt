// Copyright (c) 2026 Intel Corporation
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

#ifndef __MFX_SCHEDULER_CORE_NEXT_FACTORY_H
#define __MFX_SCHEDULER_CORE_NEXT_FACTORY_H

#include <mfx_interface.h>

#include <vector>

namespace mfx_sched_next
{

// Returns true if the session should use mfxSchedulerCoreNext instead of the
// legacy mfxSchedulerCore. Evaluated once per process and cached.
bool IsSelected(void);

// Creates a mfxSchedulerCoreNext instance holding one reference
MFXIUnknown* CreateScheduler(void);

// Platform rule: at least one Intel GPU and SchedulerCaps::IsSchedulerNextSupported()
// for all Intel GPUs
bool IsNextPlatformSet(const std::vector<mfxU32>& intelDeviceIds);


} // namespace mfx_sched_next

#endif // __MFX_SCHEDULER_CORE_NEXT_FACTORY_H
