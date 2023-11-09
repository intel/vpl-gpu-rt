// Copyright (c) 2021 Intel Corporation
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

#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <mutex>

#include "mfx_trace.h"

extern "C"
{

static const int MFX_PERF_TRACE_BUFFER_SIZE = 3072;
static char *perf_trace_var = nullptr;

static struct PerfTraceCtx {
    int32_t ftrace_fd = -1;
    std::mutex perf_mutex;
    uint32_t count = 0;
} perf_ctx;

mfxTraceU32 MFXTrace_EventInit()
{
    perf_trace_var = getenv("VPL_EVENT_TRACE");
    if (perf_trace_var == nullptr)
    {
        return 1;
    }

    std::lock_guard <std::mutex> lock(perf_ctx.perf_mutex);
    if (perf_ctx.ftrace_fd == -1) {
        perf_ctx.ftrace_fd = open("/sys/kernel/debug/tracing/trace_marker_raw", O_WRONLY);
        if (perf_ctx.ftrace_fd == -1) {
            return 1;
        }
    }
    ++perf_ctx.count;
    return 0;
}

// It dumps traces to binary format (internal representation) for future offline processing
mfxTraceU32 MFXTraceEvent(uint16_t task, uint8_t opcode, uint8_t level, uint64_t size, const void *ptr)
{
    if (perf_ctx.ftrace_fd == -1) {
        return 0;
    }
    uint64_t thread_id = syscall(SYS_gettid);
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    uint8_t trace_buf[MFX_PERF_TRACE_BUFFER_SIZE];
    memset(trace_buf,0,MFX_PERF_TRACE_BUFFER_SIZE);
    uint32_t *header = (uint32_t *)trace_buf;
    const char *tag = "FTMI"; // IMTE + 1 = IMTF (IntelMediaTraceEvent + 1) value is used as ftrace raw marker tag
    // It is reversed due to endianness
    memcpy(&header[0], tag, sizeof(header[0]));
    header[1] = (uint32_t)task << 16 | size;
    header[2] = opcode;
    size_t buf_len = sizeof(uint32_t) * 3;
    if (buf_len + size >= MFX_PERF_TRACE_BUFFER_SIZE) {
        return 1;
    }

    if (ptr && size > 0)
    {
        memcpy(trace_buf + buf_len, ptr, size);
        buf_len += size;
    }

    size_t written_bytes = write(perf_ctx.ftrace_fd, trace_buf, buf_len);
    if (written_bytes != buf_len) {
        return 1;
    }
    return 0;
}

mfxTraceU32 MFXTrace_EventClose()
{
    std::lock_guard <std::mutex> lock(perf_ctx.perf_mutex);
    --perf_ctx.count;
    if (!perf_ctx.count) {
        if (close(perf_ctx.ftrace_fd)) {
            return 1;
        }
        perf_ctx.ftrace_fd = -1;
    }
    return 0;
}

} // extern "C"

