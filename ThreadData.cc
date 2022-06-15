/**
* @file
*
* PDF text extraction class and callback functions.
*/

#include "ThreadData.hh"
#include "xPDFInfo.hh"

/**
* Convert PDF string to UTF-16 wide string (wchar_t), change byte endianess.
* Filter out \\f and \\b delimiters.
*
* @param[in]        src     string to be converted
* @param[in]        cchSrc  number of chars in src
* @param[out]       dst     converted string
* @param[in,out]    cbDst   size of dst in bytes!!!
* @return number of wchars copied to dst
*/
static ptrdiff_t convertToUTF16(const char* src, const ptrdiff_t cchSrc, wchar_t* dst, ptrdiff_t *cbDst)
{
    const auto start{ dst };
    for (ptrdiff_t i{ 0 }; (i < cchSrc) && (*cbDst > sizeOfWchar); i += sizeOfWchar)
    {
        // swap bytes
        *dst = (*(src + i + 1) & 0xFF) | ((*(src + i) << 8U) & 0xFF00);
        // filter NUL, \b and \f
        if (*dst && (*dst != L'\b') && (*dst != L'\f'))
        {
            ++dst;
            *cbDst -= sizeOfWchar;  // decrease available buffer size in bytes
        }
    }
    *dst = 0;   // put NUL character at the end of the string

    return (dst - start);
}
/**
* Create event that triggers producer action. 
* Event has automatic reset, initialy unlocked.
*/
void ThreadData::createProducer()
{
    if (!hasProducer())
        handles[PRODUCER_HANDLE] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}
/**
* Create event that notifies consumer that data from producer is ready. 
* Event has automatic reset, initialy unlocked.
*/
void ThreadData::createConsumer()
{
    if (!hasConsumer())
        handles[CONSUMER_HANDLE] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}
/**
* Wait until producer event is raised or timeout expires.
* @param[in]    timeout     timeout in miliseconds
* @return 1 if producer event is not created; for other return values check MSDN WaitForSingleObject
*/
DWORD ThreadData::waitForProducer(DWORD timeout)
{
    if (hasProducer())
        return WaitForSingleObject(handles[PRODUCER_HANDLE], timeout);

    return 1UL;
}

/**
* Wait until consumer event is raised or timeout expires.
* @param[in]    timeout     timeout in miliseconds
* @return 1 if consumer event is not created; for other return values check MSDN WaitForSingleObject
*/
DWORD ThreadData::waitForConsumer(DWORD timeout)
{
    if (hasConsumer())
        return WaitForSingleObject(handles[CONSUMER_HANDLE], timeout);

    return 1UL;
}

/**
* Raise producer event and waits for thread to exit or timeout expires.
* Event is raised if producer thread is running.
*
* @param[in]    timeout     timeout in miliseconds
* @return 1 if producer event is not created; for other return values check MSDN SignalObjectAndWait
*/
DWORD ThreadData::notifyProducerAndWait(DWORD timeout)
{
    if (hasProducer() && isStarted())
        return SignalObjectAndWait(handles[PRODUCER_HANDLE], handles[THREAD_HANDLE], timeout, FALSE);

    return 1UL;
}
/**
* Raise producer event and waits for consumer event or timeout expires.
* Event is raised if producer thread is marked as active (#abort() has not been called).
* If timeout expires, producer event is reset.
*
* @param[in]    timeout     timeout in miliseconds
* @return 1 if producer event is not created; for other return values check MSDN SignalObjectAndWait
*/
DWORD ThreadData::notifyProducerWaitForConsumer(DWORD timeout)
{
    auto dwRet{ WAIT_FAILED };
    if (isActive() && hasProducer() && hasConsumer())
    {
        TRACE(L"%hs\n", __FUNCTION__);
        dwRet = SignalObjectAndWait(handles[PRODUCER_HANDLE], handles[CONSUMER_HANDLE], timeout, FALSE);
        if (dwRet != WAIT_OBJECT_0)
        {
            // producer should wait for next call
            resetProducer();
        }
    }
    return dwRet;
}


/**
* Used in comparision.
* Wait for both consumers to get data form its producers.
*
* @param[in]    searcher    second instance of #ThreadData class
* @param[in]    timeout     timeout in miliseconds
* @return compared results of extractions
*/
int ThreadData::compareWaitForConsumers(ThreadData* searcher, DWORD timeout)
{
    auto result{ ft_fileerror };

    if (isActive() && hasProducer() && hasConsumer()
        && searcher && searcher->isActive()
        && searcher->hasProducer() && searcher->hasConsumer())
    {
        notifyProducer();
        searcher->notifyProducer();

        const HANDLE consumers[] { handles[CONSUMER_HANDLE] , searcher->handles[CONSUMER_HANDLE] };
        // wait unitl both threads signal that they completed extraction
        const auto dwRet{ WaitForMultipleObjects(ARRAYSIZE(consumers), consumers, TRUE, timeout) };
        switch (dwRet)
        {
        case WAIT_OBJECT_0:
        {
            auto result1{ ft_fileerror };
            auto result2{ ft_fileerror };
            {
                std::lock_guard lock(mutex);
                result1 = request.result;
            }
            {
                std::lock_guard lock(searcher->mutex);
                result2 = searcher->request.result;
            }
            // compare results
            result = (result1 == result2) ? result1 : ft_compare_not_eq;
            break;
        }
        case WAIT_TIMEOUT:
            result = ft_compare_abort;
            break;
        default:
            result = ft_compare_abort;
            break;
        }
        TRACE(L"%hs!consumers!dw=%lx result=%d\n", __FUNCTION__, dwRet, result);
    }
    return result;
}

/**
* Create extraction (producer) thread.
* Create producer and consumer events if they don't already exist.
*
* @param[in]    func    pointer to thread function
* @param[in]    args    parameter for thread function
* @return thread ID if thread has been successfully created or already running, 0 if error.
*/
unsigned int ThreadData::start(_beginthreadex_proc_type func, void* args)
{
    static auto threadID{ 0U };
    createConsumer();
    createProducer();

    if (hasConsumer() && hasProducer())
    {
        // if thread has not been started...
        if (!isStarted())
        {
            // start new thread
            handles[THREAD_HANDLE] = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, func, args, 0, &threadID));
            if (handles[THREAD_HANDLE])
            {
                // wait a little bit for thread to start...
                const auto dwRet{ WaitForSingleObject(handles[THREAD_HANDLE], 10UL) };
                if (dwRet != WAIT_TIMEOUT)
                    threadID = 0;
            }
        }
#if 0
        else
        {
            // get running thread ID
            threadID = GetThreadId(handles[THREAD_HANDLE]);
        }
#endif
        return threadID;
    }
    return 0;
}
/**
* Stop data extraction and closes PDF.
* Raise producer event to wake producer up, and waits until producer sends signal that PDF has been closed.
* Doesn't exit extraction thread.
*/
void ThreadData::stop()
{
    const auto status{ setStatusCond(request_status::cancelled, request_status::active) };
    if ((status == request_status::active) || (status == request_status::complete))
    {
        // reset consumer event that producer might have set
        resetConsumer();
        // wake up producer and close document
        notifyProducerWaitForConsumer(CONSUMER_TIMEOUT);
    }
}

/**
* Stop data extraction without closing PDF.
* Raise producer event to wake producer up, and waits until producer sends consumer event that extraction has been completed.
*/
void ThreadData::done()
{
    const auto status{ setStatusCond(request_status::complete, request_status::active) };
    if (status == request_status::active)
    {
        // reset consumer event that producer might have set
        resetConsumer();
        // wake up producer and close document
        notifyProducerWaitForConsumer(CONSUMER_TIMEOUT);
    }
}

/**
* Stop data extraction, close PDF and exits thread.
* Raise producer event to wake producer up, and wait until thread exists or timeout.
*/
void ThreadData::abort()
{
    // set thread as inactive
    if (setActive(false))
    {
        // mark request as cancelled
        setStatus(request_status::cancelled);
        {
            std::lock_guard lock(mutex);
            request.fileName = nullptr;
        }
        // raise producer event to wake thread up, and wait until thread exits
        notifyProducerAndWait(PRODUCER_TIMEOUT);
        // reset consumer event
        resetConsumer();
    }
    if (handles[THREAD_HANDLE])
    {
        CloseHandle(handles[THREAD_HANDLE]);
        handles[THREAD_HANDLE] = nullptr;
    }
}

/**
* Destructor
* Close handles and delete #Request::buffer
*/
ThreadData::~ThreadData()
{
    if (handles[CONSUMER_HANDLE])
    {
        CloseHandle(handles[CONSUMER_HANDLE]);
        handles[CONSUMER_HANDLE] = nullptr;
    }
    if (handles[PRODUCER_HANDLE])
    {
        CloseHandle(handles[PRODUCER_HANDLE]);
        handles[PRODUCER_HANDLE] = nullptr;
    }
    if (request.buffer)
    {
        delete[] static_cast<char*>(request.buffer);
        request.buffer = nullptr;
    }
}

/**
* Request initialization.
* 
* @param[in]    fileName        full path to PDF document
* @param[in]    field           index of the field
* @param[in]    unit            index of the unit
* @param[in]    flags           TC flags
* @param[in]    timeout         producer timeout (in text extraction)
* @return       ft_setsuccess if there is no available data to read, ft_timeout if consumer should get data
*/
int ThreadData::initRequest(const wchar_t* fileName, int field, int unit, int flags, DWORD timeout)
{
    int result{ ft_setsuccess };
    std::lock_guard lock(mutex);
    request.fileName = fileName;
    request.field = field;
    request.unit = unit;
    request.flags = flags;
    request.timeout = timeout;
    // for continuous full text search, don't move ptr to the beginning, it may point to extracted data
    if (!((field == fiText) && (unit > 0)))
        request.ptr = request.buffer;   // set string end to begining of buffer
    // if buffer is empty set result to ft_fieldempty
    if (request.ptr == request.buffer)
    {
        request.result = ft_fieldempty;
        *(static_cast<int64_t*>(request.ptr)) = 0;
    }
    else
    {
        request.result = ft_fulltextw;
        result = ft_timeout;
    }

    return result;
}
/**
* Convert data from PDF text extraction to TC output buffer.
* Used for #fiFirstRow, #fiDocStart and #fiText.
* Data is collected until output buffer is full or end-of-line (EOL) is found (for #fiFirstRow).
* For #fiFirstRow and #fiDocStart request is marked as complete (#Request::status=#complete).
* For #fiText, thread raises consumer event and continues extraction.
* If consumer thread doesn't fetch data, this thread goes to wait state until consumer raises producer event.
* If consumer doesn't raise producer event in #Request::timeout, 
* request is marked as cancelled (#Request::status=#cancelled) and extraction aborts.
*
* @param[in]    text    pointer to text extracted from PDF
* @param[in]    len     number of chars in text
* @return 0 - continue extraction, 1 abort extraction
*/
int ThreadData::output(const char *text, int len)
{
    int field{ 0 };
    ptrdiff_t remaining{ 0 };
    {
        std::unique_lock lock(mutex);
        // get data from request structure for later use outside of lock
        const auto timeout{ request.timeout };
        remaining = request.remaining();
        while (remaining <= 2)
        {
            lock.unlock();
            // wait for TC to get data
            if (waitForProducer(timeout) != WAIT_OBJECT_0)
            {
                setStatusCond(request_status::cancelled, request_status::active);
                return 1;
            }
            if (getStatus() != request_status::active)
                return 1;

            lock.lock();
            remaining = request.remaining();
        }
        field = request.field;

        // get end of current string
        auto wdst{ static_cast<wchar_t*>(request.ptr) };
        // convert data from TextOutputDev to wchar_t
        const auto cchWdst{ convertToUTF16(text, len, wdst, &remaining) };
        if (cchWdst)
        {
            if (field == fiFirstRow)
            {
                request.result = ft_stringw;
                // search for EOL
                auto pos{ wcspbrk(wdst, L"\r\n") };
                if (pos)
                {
                    // EOL found!
                    *pos = 0;       // remove EOL
                    remaining = 0;  // flag to exit extraction
                }
            }
            else if (field == fiDocStart)
                request.result = ft_stringw;
            else
                request.result = ft_fulltextw;

            wdst += cchWdst;
            // update end of string pointer
            request.ptr = wdst;
        }
    }

    // if no bytes left in dest buffer
    if (remaining <= 2)
    {
        if (field == fiText)
        {
            if (getStatus() == request_status::active)
            {
                notifyConsumer();
                TRACE(L"%hs!TC notified!%d b\n", __FUNCTION__, REQUEST_BUFFER_SIZE - remaining);
            }
            else
                return 1;
        }
        else
        {
            // extraction is complete
            setStatusCond(request_status::complete, request_status::active);
            return 1;
        }
    }
    return 0;
}
