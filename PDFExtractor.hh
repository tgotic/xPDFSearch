/**
* @file
*
* PDFExtractor header file.
*/

#pragma once

#include "contentplug.h"
#include <tchar.h>

#include <Object.h>
#include "TcOutputDev.hh"
#include <string>

/**
* RAII wrapper for collate category of locale ANSI Code Page 
*/
class ScopedCollateLocale
{
private:
    _locale_t _locale{ nullptr };
public:
    explicit ScopedCollateLocale() : _locale(_create_locale(LC_COLLATE, ".ACP")) { }
    ScopedCollateLocale(const ScopedCollateLocale&) = delete;
    ScopedCollateLocale& operator=(const ScopedCollateLocale&) = delete;
    ~ScopedCollateLocale() { _free_locale(_locale); }
    _locale_t get() const { return _locale; }
};

/**
* Extract various data from PDF document.
* Compare data from two PDF documents.
*/
class PDFExtractor
{
public:
    explicit PDFExtractor() { };
    PDFExtractor(const PDFExtractor&) = delete;
    PDFExtractor& operator=(const PDFExtractor&) = delete;
    ~PDFExtractor();
    int extract(const wchar_t* fileName, int field, int unit, void* dst, int dstSize, int flags);
    int compare(PROGRESSCALLBACKPROC progresscallback, const wchar_t* fileName1, const wchar_t* fileName2, int field);
    void abort();
    void stop();
    void waitForProducer();

private:
    void getMetadataString(PDFDoc* doc, const char* key);
    void getMetadataDate(PDFDoc* doc, const char* key);
    void getMetadataAttrStr(PDFDoc* doc);
    void getDocID(PDFDoc* doc);

    static double getPaperSize(int units);
    static BOOL isIncremental(PDFDoc* doc);
    static BOOL isTagged(PDFDoc* doc);
    static BOOL hasSignature(PDFDoc* doc);
    static ptrdiff_t UnicodeToUTF16(wchar_t* dst, int *cbDst, const Unicode* src, int cchSrc);
    static size_t removeDelimiters(wchar_t* str, size_t cchStr, const wchar_t* delims);
    static void appendHexValue(wchar_t* dst, size_t cbDst, int value);
    static wchar_t nibble2wchar(char nibble);
    int initData(const wchar_t* fileName, int field, int unit, int flags, DWORD timeout);

    unsigned int startWorkerThread();
    int waitForConsumer(DWORD timeout);
    bool open();
    void close();
    void closeDoc();
    void doWork();
    void done();

    std::unique_ptr<ThreadData>     m_data{ std::make_unique<ThreadData>() };      /**< pointer to thread data, request    */
    std::unique_ptr<PDFExtractor>   m_search{ nullptr };        /**< pointer to second instance of PDFExtractor, used to extract data from second file when comparing data */
    std::unique_ptr<PDFDoc>         m_doc{ nullptr };           /**< pointer to PDFDoc object   */
    ScopedCollateLocale             m_locale{ };                /**< locale-specific value, used for compare as text */
    std::wstring                    m_fileName{ };              /**< full patht to PDF document, used to compare open with new one  */
    TcOutputDev                     m_tc;                       /**< text extraction object */
};
