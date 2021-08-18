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

static const int MFX_PERF_TRACE_BUFFER_SIZE = 256;
static char *perf_trace_var = getenv("MFX_PERF_TRACE");

static struct PerfTraceCtx {
    int32_t ftrace_fd = -1;
    std::mutex perf_mutex;
    uint32_t count = 0;
} perf_ctx;

mfxTraceU32 MFXTrace_PerfInit()
{
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
mfxTraceU32 MFXTrace_PerfEvent(uint16_t task, uint8_t opcode, uint8_t level, uint64_t size, void *ptr)
{
    if ((!(perf_trace_var && !strcmp(perf_trace_var, "1"))) || (perf_ctx.ftrace_fd == -1)) {
        return 0;
    }
    uint64_t thread_id = syscall(SYS_gettid);
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    size_t total_size = sizeof(task) + sizeof(opcode) + sizeof(timestamp) + sizeof(thread_id) + sizeof(size) + sizeof(char) * size;
    uint8_t trace_buf[MFX_PERF_TRACE_BUFFER_SIZE];
    uint32_t *header = (uint32_t *)trace_buf;
    const char *tag = "FTMI"; // IMTE + 1 = IMTF (IntelMediaTraceEvent + 1) value is used as ftrace raw marker tag
    // It is reversed due to endianness
    memcpy(&header[0], tag, sizeof(header[0]));
    header[1] = task << 16 | (total_size);
    header[2] = opcode;
    size_t buf_len = sizeof(uint32_t) * 3;
    if (buf_len + total_size >= MFX_PERF_TRACE_BUFFER_SIZE) {
        return 1;
    }

    memcpy(trace_buf + buf_len, &task, sizeof(task));
    buf_len += sizeof(task);
    memcpy(trace_buf + buf_len, &opcode, sizeof(opcode));
    buf_len += sizeof(opcode);
    memcpy(trace_buf + buf_len, &timestamp, sizeof(timestamp));
    buf_len += sizeof(timestamp);
    memcpy(trace_buf + buf_len, &thread_id, sizeof(thread_id));
    buf_len += sizeof(thread_id);
    memcpy(trace_buf + buf_len, &size, sizeof(size));
    buf_len += sizeof(size);
    memcpy(trace_buf + buf_len, ptr, sizeof(char) * size);
    buf_len += sizeof(char) * size;

    size_t written_bytes = write(perf_ctx.ftrace_fd, trace_buf, buf_len);
    if (written_bytes != buf_len) {
        return 1;
    }
    return 0;
}

mfxTraceU32 MFXTrace_PerfClose()
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

