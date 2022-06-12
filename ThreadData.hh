#pragma once

#include <Windows.h>
#include <process.h>
#include <TextOutputDev.h>
#include <PDFDoc.h>
#include <mutex>
#include <atomic>

/**
* @file
* Declarations of constants and structures used in threads 
*/

/**
* @defgroup handles ThreadData::handles indexes
* @{ */
constexpr auto THREAD_HANDLE{ 0U };     /**< thread handle index in #ThreadData::handles[] */
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
* it is a bad idea to wait for infinite
*/

constexpr auto CONSUMER_TIMEOUT{ 10000UL };  /**< time for one data extraction */
constexpr auto PRODUCER_TIMEOUT{ 100UL };    /**< extractor waits for next request from TC, or closes PDF document */
#endif

constexpr auto REQUEST_BUFFER_SIZE{ 2048U };/**< size of Request.buffer, if not provided form TC */

constexpr auto sizeOfWchar{ static_cast<int>(sizeof(wchar_t)) };/**< sizeof wchar_t */

/** 
* Request status enumeration 
*/
enum request_status
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
    std::atomic<request_status> status{ request_status::closed };   /**< request status, @see request_status */
    void* buffer{ new char[REQUEST_BUFFER_SIZE] };                  /**< extracted data buffer */
    void* ptr{ buffer };                                            /**< pointer to end of extracted data, offset pointer to buffer */
    const wchar_t* fileName{ nullptr }; /**< name of PDF document */
    auto remaining() const { return (REQUEST_BUFFER_SIZE - (static_cast<char*>(ptr) - static_cast<char*>(buffer))); }
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
    unsigned int start(_beginthreadex_proc_type func, void* args);
    void abort();
    void done();
    void stop();
    int output(const char* text, int len);
    inline bool isActive() const { return active; }
    inline bool setActive(bool state) { return active.exchange(state); }
    inline request_status getStatus() const { return request.status; }
    inline auto setStatus(request_status s) { return request.status.exchange(s); }
    auto setStatusCond(request_status ns, request_status cs)
    {
        auto expected{ cs };
        request.status.compare_exchange_strong(expected, ns);
        return expected;
    }
    int initRequest(const wchar_t* fileName, int field, int unit, int flags, DWORD timeout);

    template<typename T> void setValue(T value, int type);

    auto getRequestField() const { return request.field; }
    auto getRequestUnit() const { return request.unit; }
    auto getRequestResult() const { return request.result; }
    auto getRequestBuffer() const { return request.buffer; }
    auto getRequestPtr() const { return request.ptr; }
    auto getRequestFileName() const { return request.fileName; }

    void setRequestResult(int result) { request.result = result; }
    void setRequestPtr(void* ptr) 
    { 
        if ((ptr >= request.buffer) && (ptr < static_cast<char*>(request.buffer) + REQUEST_BUFFER_SIZE))
            request.ptr = ptr;
    }

private:
    Request request;                    /**< extraction request */
    std::atomic_bool active{ false };   /**< thread status, true when active */
    HANDLE handles[MAX_THREAD_HANDLES]{ nullptr };  /**< thread, producer and consumer event handles */
    void createProducer();
    void createConsumer();
    inline auto isStarted() const { return (handles[THREAD_HANDLE] != nullptr); }
    inline void notifyProducer() { SetEvent(handles[PRODUCER_HANDLE]); }
    inline void resetConsumer() { ResetEvent(handles[CONSUMER_HANDLE]); }

};

/**
* Sets simple result values (BOOL, int and double) to output buffer.
* Data exchange is guarded in critical section.
*
* @tparam       T       typedef of value
* @param        value   value to be set to output buffer
* @param[in]    type    type of result value
*/
template<typename T>
void ThreadData::setValue(T value, int type)
{
    std::lock_guard lock(mutex);
    *(static_cast<T*>(request.buffer)) = value;
    request.result = type;
}
