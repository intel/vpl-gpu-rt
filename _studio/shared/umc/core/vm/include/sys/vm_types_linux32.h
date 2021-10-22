// Copyright (c) 2003-2021 Intel Corporation
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


#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>

#include <ippcore.h>
#include <ipps.h>
#include <sys/select.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* vm_event.h */
typedef struct vm_event
{
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    int32_t manual;
    int32_t state;
} vm_event;

/* vm_semaphore.h */
typedef struct vm_semaphore
{
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    int32_t count;
    int32_t max_count;
} vm_semaphore;

#ifdef __cplusplus
}
#endif /* __cplusplus */

