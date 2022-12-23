/**
* @file
*
* PDF text extraction class and callback functions.
*/

#include "ThreadData.hh"
#include "xPDFInfo.hh"
#include <GlobalParams.h>

/**
* Convert PDF string to UTF-16 wide string (wchar_t), change byte endianess.
* Filter out \\f and \\b delimiters.
*
* @param[in]        src     string to be converted
* @param[in]        cchSrc  number of chars in src in bytes
* @param[out]       dst     converted string
* @param[in,out]    cbDst   [in] size of dst in bytes, [out] remaining dst size in bytes
* @return number of src chars converted to dst
*/
static ptrdiff_t PdfTxtToUTF16(const char* src, const ptrdiff_t cchSrc, wchar_t* dst, ptrdiff_t *cbDst)
{
    ptrdiff_t i{ 0 };
    for (; (i < cchSrc) && (*cbDst > sizeOfWchar + 1); i += sizeOfWchar)
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

    return i;
}

/**
* Convert string from PDF Unicode to wchar_t.
*
* @param[in]        src     string to convert
* @param[in]        cchSrc  number of unicode characters
* @param[out]       dst     converted string
* @param[in,out]    cbDst   [in] size of dst in bytes, [out] remaining dst size in bytes
* @return number of src Unicode characters converted to dst
*/
ptrdiff_t ThreadData::UnicodeToUTF16(const Unicode* src, ptrdiff_t cchSrc, wchar_t* dst, ptrdiff_t *cbDst)
{
    ptrdiff_t i{ 0 };
    if (src && dst && cbDst)
    {
        for (; (i < cchSrc) && (*cbDst > sizeOfWchar + 1); i++)
        {
            *dst++ = *src++ & 0xFFFF;
            *cbDst -= sizeOfWchar;
        }
        *dst = 0; // put NUL character at the end of the string
    }
    return i;
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
* Create worker (producer) thread.
* @param[in]    func    pointer to thread function
* @param[in]    args    parameter for thread function
* @return thread ID if worker thread has been successfully created or already running, 0 if error.
*/
uint32_t ThreadData::createWorker(_beginthreadex_proc_type func, void* args)
{
    static auto threadID{ 0U };
    // if worker thread has not been created...
    if (!hasWorker())
    {
        // start new worker thread
        handles[WORKER_HANDLE] = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, func, args, 0, &threadID));
        if (handles[WORKER_HANDLE])
        {
            // wait a little bit for thread to start...
            const auto dwRet{ WaitForSingleObject(handles[WORKER_HANDLE], 10UL) };
            if (dwRet != WAIT_TIMEOUT)
            {
                TRACE(L"%hs!new thread %x ended prematurely\n", __FUNCTION__, handles[WORKER_HANDLE]);
                threadID = 0;
            }
        }
        else
        {
            TRACE(L"%hs!unable to start new thread\n", __FUNCTION__);
        }
    }
#if 0
    else
    {
        // get running thread ID
        threadID = GetThreadId(handles[WORKER_HANDLE]);
    }
#endif
    return threadID;
}

/**
* Close consumer event handle.
*/
void ThreadData::closeConsumer()
{
    if (hasConsumer())
    {
        CloseHandle(handles[CONSUMER_HANDLE]);
        handles[CONSUMER_HANDLE] = nullptr;
    }
}

/**
* Close producer event handle.
*/
void ThreadData::closeProducer()
{
    if (hasProducer())
    {
        CloseHandle(handles[PRODUCER_HANDLE]);
        handles[PRODUCER_HANDLE] = nullptr;
    }
}

/**
* Close worker thread handle.
*/
void ThreadData::closeWorker()
{
    if (hasWorker())
    {
        CloseHandle(handles[WORKER_HANDLE]);
        handles[WORKER_HANDLE] = nullptr;
    }
}

/**
* Wait until producer event is raised or timeout expires.
*
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
*
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
* Raise producer event and waits for worker thread to exit or timeout expires.
* Event is raised if producer thread is running.
*
* @param[in]    timeout     timeout in miliseconds
* @return 1 if producer event is not created; for other return values check MSDN SignalObjectAndWait
*/
DWORD ThreadData::notifyProducerAndWait(DWORD timeout)
{
    if (hasProducer() && hasWorker())
        return SignalObjectAndWait(handles[PRODUCER_HANDLE], handles[WORKER_HANDLE], timeout, FALSE);

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
* @return thread ID if worker thread has been successfully created or already running, 0 if error.
*/
uint32_t ThreadData::start(_beginthreadex_proc_type func, void* args)
{
    createConsumer();
    createProducer();

    if (hasConsumer() && hasProducer())
    {
        return createWorker(func, args);
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
    const auto status{ setStatusCond(requestStatus::cancelled, requestStatus::active) };
    if ((status == requestStatus::active) || (status == requestStatus::complete))
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
    const auto status{ setStatusCond(requestStatus::complete, requestStatus::active) };
    if (status == requestStatus::active)
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
        setStatus(requestStatus::cancelled);
        {
            std::lock_guard lock(mutex);
            request.fileName = nullptr;
        }
        // raise producer event to wake thread up, and wait until thread exits
        notifyProducerAndWait(PRODUCER_TIMEOUT);
        // reset consumer event
        resetConsumer();
    }
    closeWorker();
}

/**
* Destructor
* Close handles and delete #Request::buffer
*/
ThreadData::~ThreadData()
{
    closeConsumer();
    closeProducer();
    request.release();
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
    if (!(((field == fiText) || (field == fiOutlines)) && (unit > 0)))
        request.ptr = request.buffer;   // set string end to begining of buffer
    // if buffer is empty set result to ft_fieldempty
    if (request.ptr == request.buffer)
    {
        request.result = ft_fieldempty;
        memset(request.ptr, 0, sizeof(int64_t) + sizeof(wchar_t)); // clear additional wchar_t for ft_numeric_floating
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
* Used for #fiFirstRow, #fiDocStart, #fiText and #fiOutlines.
* Data is collected until output buffer is full or end-of-line (EOL) is found (for #fiFirstRow).
* For #fiFirstRow and #fiDocStart request is marked as complete (#Request::status=#complete).
* For #fiText and #fiOutlines, thread raises consumer event and continues extraction.
* If consumer thread doesn't fetch data, this thread goes to wait state until consumer raises producer event.
* If consumer doesn't raise producer event in #Request::timeout, 
* request is marked as cancelled (#Request::status=#cancelled) and extraction aborts.
*
* @param[in]    text            pointer to text extracted from PDF
* @param[in]    len             number of chars in text; if textIsUnicode is true, nubmer of Unicode characters
* @param[in]    textIsUnicode   text is Unicode
* @return 0 - continue extraction, 1 abort extraction
*/
int ThreadData::output(const char *text, ptrdiff_t len, bool textIsUnicode)
{
    static const auto eol{ globalParams->getTextEOL() };
    do
    {
        int field{ 0 };
        ptrdiff_t cbDstW{ 0 };
        {
            std::unique_lock lock(mutex);
            // get data from request structure for later use outside of lock
            const auto timeout{ request.timeout };
            cbDstW = request.remaining();
            while (cbDstW <= 2)
            {
                lock.unlock();
                // wait for TC to get data
                if (waitForProducer(timeout) != WAIT_OBJECT_0)
                {
                    setStatusCond(requestStatus::cancelled, requestStatus::active);
                    return 1;
                }
                if (getStatus() != requestStatus::active)
                    return 1;

                lock.lock();
                cbDstW = request.remaining();
            }
            field = request.field;

            // get end of current string
            auto dstW{ static_cast<wchar_t*>(request.ptr) };
            // size of dstW in bytes
            const auto cbDstWtmp{ cbDstW };
            ptrdiff_t lenConverted;
            if (textIsUnicode)
            {
                auto utext{ reinterpret_cast<const Unicode*>(text) };
                lenConverted = UnicodeToUTF16(utext, len, dstW, &cbDstW);
                text = reinterpret_cast<const char*>(utext + lenConverted);
            }
            else
            {
                lenConverted = PdfTxtToUTF16(text, len, dstW, &cbDstW);
                text += lenConverted;
            }
            if (lenConverted)
                len -= lenConverted;
            else
                len = 0;    // prevent infinite loop

            if (cbDstWtmp != cbDstW)
            {
                if (field == fiText)
                {
                    request.result = ft_fulltextw;
                }
                else if (field == fiOutlines)
                {
                    request.result = ft_fulltextw;
                    if (cbDstW > 4)
                    {
                        if (eol == eolUnix)
                        {
                            StringCbCatW(dstW, cbDstWtmp, L"\n");
                            cbDstW -= 2;
                        }
                        else if (eol == eolDOS)
                        {
                            StringCbCatW(dstW, cbDstWtmp, L"\r\n");
                            cbDstW -= 4;
                        }
                        else if (eol == eolMac)
                        {
                            StringCbCatW(dstW, cbDstWtmp, L"\r");
                            cbDstW -= 2;
                        }
                    }
                    TRACE(L"%hs!outlines!buffer=%p ptr=%p!outline=[%ls]", __FUNCTION__, request.buffer, request.ptr, static_cast<const wchar_t*>(request.buffer));
                }
                else if (field == fiFirstRow)
                {
                    request.result = ft_stringw;
                    // search for EOL
                    auto pos{ wcspbrk(dstW, L"\r\n") };
                    if (pos)
                    {
                        // EOL found
                        *pos = 0;       // remove EOL
                        cbDstW = 0;     // flag to exit extraction
                        len = 0;        // stop conversion, ft_stringw doesn't support multiple calls as ft_fulltextw does
                    }
                    else if (cbDstW <= 2)
                    {
                        len = 0;        // stop conversion
                    }
                }
                else
                {
                    request.result = ft_stringw;
                    len = 0;  // stop conversion
                }

                // update end of string pointer
                request.ptr = static_cast<char*>(request.ptr) + (cbDstWtmp - cbDstW);
            }
        }

        // if no bytes left in dest buffer
        if (cbDstW <= 2)
        {
            if ((field == fiText) || (field == fiOutlines))
            {
                if (getStatus() == requestStatus::active)
                {
                    notifyConsumer();
                    TRACE(L"%hs!TC notified!%d b\n", __FUNCTION__, REQUEST_BUFFER_SIZE - cbDstW);
                }
                else
                    return 1;
            }
            else
            {
                // extraction is complete
                setStatusCond(requestStatus::complete, requestStatus::active);
                return 1;
            }
        }
    } while (len);

    return 0;
}
