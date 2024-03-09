/**
* @file
*
* Declarations of constants and structures used in threads
*/

#pragma once

#include <Windows.h>
#include <process.h>
#include <TextOutputDev.h>
#include <GList.h>
#include <Outline.h>
#include <PDFDoc.h>
#include <TextString.h>

#include <mutex>
#include <atomic>

#include <strsafe.h>

/**
* @defgroup handles ThreadData::handles indexes
* @{ */
constexpr auto WORKER_HANDLE{ 0U };     /**< worker thread handle index in #ThreadData::handles[] */
constexpr auto CONSUMER_HANDLE{ 1U };   /**< consumer event handle index in #ThreadData::handles[] */
constexpr auto PRODUCER_HANDLE{ 2U };   /**< producer event handle index in #ThreadData::handles[] */
constexpr auto MAX_THREAD_HANDLES{ 3U }; /**< max ThreadData handles */
/** @} */

#if 0
constexpr auto CONSUMER_TIMEOUT{ INFINITE };
constexpr auto PRODUCER_TIMEOUT{ 100UL };
#else
/**
* wait for 10 s for producer to produce data (form PDF)
* wait for 100 ms for consumer (TC) to consume data and ask for more,
* on timeout close PDF file
* it is a bad idea to have infinite wait
*/

constexpr auto CONSUMER_TIMEOUT{ 10000UL };  /**< time for one data extraction */
constexpr auto PRODUCER_TIMEOUT{ 100UL };    /**< extractor waits for next request from TC, or closes PDF document */
#endif

constexpr auto REQUEST_BUFFER_SIZE{ 2048U };    /**< size of Request.buffer, if not provided form TC */

constexpr auto sizeOfWchar{ static_cast<int>(sizeof(wchar_t)) };/**< sizeof wchar_t */

/** 
* Request status enumeration 
*/
enum requestStatus
{
    closed,     /**< PDF document is closed */
    active,     /**< data extraction form PDF document in progress */
    complete,   /**< data extraction form PDF document complete */
    cancelled,  /**< data extraction is cancelled, waiting to close document */
};

/**
* PDF extraction request related data 
*/
struct Request
{
    int field{ 0 };                     /**< field index to extract */
    int unit{ 0 };                      /**< unit index */
    int flags{ 0 };                     /**< flags from TC */
    int result{ 0 };                    /**< result of an extraction */
    DWORD timeout{ 0 };                 /**< time to wait in text extraction procedure */
    std::atomic<requestStatus> status{ requestStatus::closed };   /**< request status, @see request_status */
    void* buffer{ new char[REQUEST_BUFFER_SIZE] };                  /**< extracted data buffer */
    void* ptr{ buffer };                                            /**< pointer to end of extracted data, offset pointer to buffer */
    const wchar_t* fileName{ nullptr }; /**< name of PDF document */
    auto remaining() const { return (buffer ? (REQUEST_BUFFER_SIZE - (static_cast<char*>(ptr) - static_cast<char*>(buffer))) : 0); }
    void release() { delete[] static_cast<char*>(buffer); buffer = nullptr; ptr = nullptr; }
};

/**
* Extraction thread related data 
*/
class ThreadData
{
public:
    std::mutex mutex;                  /**< mutex to protect Request structure while exchanging data */

public:
    ThreadData() = default;
    ThreadData(const ThreadData&) = delete;
    ThreadData& operator=(const ThreadData&) = delete;
    ~ThreadData();

    inline void resetProducer() { ResetEvent(handles[PRODUCER_HANDLE]); }
    inline void notifyConsumer() { SetEvent(handles[CONSUMER_HANDLE]); };
    auto hasProducer() const { return (handles[PRODUCER_HANDLE] != nullptr); }
    auto hasConsumer() const { return (handles[CONSUMER_HANDLE] != nullptr); }
    DWORD waitForProducer(DWORD timeout);
    DWORD waitForConsumer(DWORD timeout);
    int compareWaitForConsumers(ThreadData* searcher, DWORD timeout);
    DWORD notifyProducerWaitForConsumer(DWORD timeout);
    // DWORD notifyConsumerWaitForProducer(DWORD timeout);
    DWORD notifyProducerAndWait(DWORD timeout);
    uint32_t start(_beginthreadex_proc_type func, void* args);
    void abort();
    void done();
    void stop();
    int output(const char* text, ptrdiff_t len, bool textIsUnicode);
    inline bool isActive() const { return active; }
    inline bool setActive(bool state) { return active.exchange(state); }
    inline requestStatus getStatus() const { return request.status; }
    inline auto setStatus(requestStatus new_status) { return request.status.exchange(new_status); }
    auto setStatusCond(requestStatus new_status, requestStatus current_status)
    {
        auto expected{ current_status };
        request.status.compare_exchange_strong(expected, new_status);
        return expected;
    }
    int initRequest(const wchar_t* fileName, int field, int unit, int flags, DWORD timeout);

    template<typename T> void setValue(T value, int type);
#if defined(_MSC_VER) && !defined(__llvm__)
    // GCC BUG, it doesn't support Explicit specialization in non-namespace scope
    template<> void setValue(GString* value, int type);
    template<> void setValue(wchar_t* value, int type);
#endif
    auto getRequestField() const { return request.field; }
    auto getRequestFlags() const { return request.flags; }
    auto getRequestUnit() const { return request.unit; }
    auto getRequestResult() const { return request.result; }
    auto getRequestBuffer() const { return request.buffer; }
    auto getRequestPtr() const { return request.ptr; }
    auto getRequestFileName() const { return request.fileName; }

    void setRequestResult(int result) { request.result = result; }
    void setRequestPtr(void* ptr)
    { 
        if (request.buffer && (ptr >= request.buffer) && (ptr < static_cast<char*>(request.buffer) + REQUEST_BUFFER_SIZE))
            request.ptr = ptr;
    }

private:
    Request request;                    /**< extraction request */
    std::atomic_bool active{ false };   /**< thread status, true when active */
    HANDLE handles[MAX_THREAD_HANDLES]{ nullptr };  /**< thread, producer and consumer event handles */
    void createProducer();
    void createConsumer();
    uint32_t createWorker(_beginthreadex_proc_type func, void* args);
    void closeProducer();
    void closeConsumer();
    void closeWorker();
    inline auto hasWorker() const { return (handles[WORKER_HANDLE] != nullptr); }
    inline void notifyProducer() { SetEvent(handles[PRODUCER_HANDLE]); }
    inline void resetConsumer() { ResetEvent(handles[CONSUMER_HANDLE]); }
    void setGStringValue(GString* value, int type);
    void setWcharValue(wchar_t* value, int type);

    static ptrdiff_t UnicodeToUTF16(const Unicode* src, ptrdiff_t cchSrc, wchar_t* dst, ptrdiff_t *cbDst);
};

/**
* Sets simple result values (BOOL, int and double) to output buffer.
* Data exchange is guarded with mutex.
*
* @tparam       T       typedef of value
* @param        value   value to be set to output buffer
* @param[in]    type    type of result value
*/
template<typename T>
void ThreadData::setValue(T value, int type)
{
#if defined(__GNUC__) || defined(__llvm__)
    /*
    https://stackoverflow.com/questions/49707184/explicit-specialization-in-non-namespace-scope-does-not-compile-in-gcc
    */
    if constexpr (std::is_same_v<T, GString*>)
    {
        setGStringValue(value, type);
        return;
    }
    if constexpr (std::is_same_v<T, wchar_t*>)
    {
        setWcharValue(value, type);
        return;
    }
#endif
    std::lock_guard lock(mutex);
    *(static_cast<T*>(request.buffer)) = value;
    setRequestResult(type);
}

#if defined(_MSC_VER) && !defined(__llvm__)
/**
* Template specialization of #setValue for GString.
* Convert GString to TextString to get Unicode and then convert Unicode to UTF-16.
*
* @param        value   value to be set to output buffer
* @param[in]    type    type of result value
*/
template<>
void ThreadData::setValue(GString* value, int type)
{
    setGStringValue(value, type);
}

/**
* Template specialization of #setValue for wchar_t*.
*
* @param        value   value to be set to output buffer
* @param[in]    type    type of result value
*/
template<>
void ThreadData::setValue(wchar_t* value, int type)
{
    setWcharValue(value, type);
}
#endif // _MSC_VER
