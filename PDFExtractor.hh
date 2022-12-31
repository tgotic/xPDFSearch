/**
* @file
*
* PDFExtractor header file.
*/

#pragma once

#include "contentplug.h"

#include <Object.h>
#include <Zoox.h>
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
    void getMetadataString(const char* key);
    void getMetadataDate(const char* key);
    void getMetadataDateRaw(const char* key);
    void getMetadataAttrStr();
    void getDocID();
    bool getOulinesTitles(GList* node);
    void getOulines();
    void getVersion();
    void getConformance();

    static double getPaperSize(int units);
    static bool isIncremental(PDFDoc* doc);
    static bool isTagged(PDFDoc* doc);
    static bool hasSignature(PDFDoc* doc);
    static bool hasOutlines(PDFDoc* doc);
    static size_t removeDelimiters(wchar_t* str, size_t cchStr, const wchar_t* delims);
    static void appendHexValue(wchar_t* dst, size_t cbDst, int value);
    static wchar_t nibble2wchar(char nibble);
    static bool getElemOrAttrData(ZxElement* elem, const char* nodeName, GString& conformance, const char* prefix);
    static void getXmpConformance(GString* metadata, GString& conformance);
    static int getExtensionLevel(PDFDoc* doc);
    static bool dateToInt(const char* date, uint8_t len, uint16_t& result);
    static GString* getMetadataDateTimeString(PDFDoc* doc, const char* key);
    static bool PdfDateTimeToFileTime(GString* pdfDateTime, FILETIME& fileTime);
    int initData(const wchar_t* fileName, int field, int unit, int flags, DWORD timeout);

    uint32_t startWorkerThread();
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
