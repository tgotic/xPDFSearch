#include "PDFExtractor.hh"
#include <CharTypes.h>
#include <TextString.h>
#include "xPDFInfo.hh"
#include <locale.h>
#include <wchar.h>
#include <strsafe.h>
#include <charconv>

/**
* @file
*
* PDF metadata and text extraction class.
* 
* PDF document is open on a first call to #PDFExtractor::extract or #PDFExtractor::compare functions.
* It stays open while #PDFExtractor::extract or #PDFExtractor::compare functions calls 
* have the same fileName value. Opening and processing PDF document can take significant
* time, CPU and memory. It is better to keep PDFDoc object active while TC may do
* multiple calls to extract or compare functions in short time.
* When fileName changes, currently open file is closed,
* and new one is open. This works fine until last file in the list/directory. 
* TC doesn't inform plugin that file can be closed. It stays open and cannot be modified,
* moved or deleted. To solve this problem, data extraction runs in another thread.
* If TC doesn't call #PDFExtractor::extract function in 100ms, file is closed.
* 
* @msc
* TC,WDX,PRODUCER,XPDF;
* TC=>WDX [label="ContentGetValueW"];
* WDX=>PRODUCER [label="StartWorkerThread"];
* PRODUCER=>PRODUCER [label="waitForProducer"];
* WDX->PRODUCER [label="PRODUCER EVENT"];
* WDX=>WDX [label="waitForConsumer"];
* PRODUCER=>XPDF [label="doWork"];
* XPDF>>PRODUCER [label="Request"];
* PRODUCER->WDX [label="CONSUMER EVENT"];
* WDX>>TC [label="fieldValue"];
* ...;
* PRODUCER=>PRODUCER [label="close"];
* @endmsc
*
* Similar principle has been used in text extraction. Data offset that TC sends in unit
* cannot be used to jump to a position in PDF. When a block of text is extracted from PDF,
* extraction thread stops and informs TC thread about extracted data. TC compares data
* with search string and informs plugin if data extraction can be aborted and document closed.
*
* @msc
* TC,WDX,PRODUCER,XPDF,OUTPUT_DEV;
* TC=>WDX [label="ContentGetValueW(unit=0)"];
* WDX=>PRODUCER [label="StartWorkerThread"];
* PRODUCER=>PRODUCER [label="waitForProducer"];
* WDX->PRODUCER [label="PRODUCER EVENT"];
* WDX=>WDX [label="waitForConsumer"];
* PRODUCER=>XPDF [label="open"];
* XPDF=>>OUTPUT_DEV [label="outputFunction"];
* OUTPUT_DEV->WDX [label="CONSUMER EVENT"];
* OUTPUT_DEV=>OUTPUT_DEV [label="wait for PRODUCER"];
* WDX>>TC [label="ft_fulltextw"];
* TC=>TC [label="compare"];
* TC=>WDX [label="ContentGetValueW(unit>0)"];
* WDX->OUTPUT_DEV [label="PRODUCER EVENT"];
* OUTPUT_DEV>>XPDF [label="continue"];
* XPDF=>>OUTPUT_DEV [label="outputFunction"];
* OUTPUT_DEV->WDX [label="CONSUMER EVENT"];
* OUTPUT_DEV=>OUTPUT_DEV [label="wait for PRODUCER"];
* WDX>>TC [label="ft_fulltextw"];
* TC=>TC [label="string found"];
* TC=>WDX [label="ContentGetValueW(unit=-1)"];
* WDX->OUTPUT_DEV [label="cancel, PRODUCER EVENT"];
* WDX=>WDX [label="waitForConsumer"];
* OUTPUT_DEV>>XPDF [label="abort"];
* XPDF>>PRODUCER [label="doWork"];
* PRODUCER=>PRODUCER [label="close"];
* PRODUCER->WDX [label="CONSUMER EVENT"];
* WDX>>TC [label="ft_fieldempty"];
* ...;
* PRODUCER=>PRODUCER [label="waitForProducer"]; 
* @endmsc
*
*/

/**
* The keys required to read the metadata fields. 
*/
static constexpr const char* metaDataFields[] =
{
    "Title", "Subject", "Keywords", "Author", "Creator", "Producer"
};

/**
* Destructor, free allocated resources.
* Don't call #abort() function from destructor.
*/
PDFExtractor::~PDFExtractor()
{
    TRACE(L"%hs\n", __FUNCTION__);
}
/**
* Close PDFDoc.
* Set Request::status to closed.
*/
void PDFExtractor::closeDoc()
{
    m_data->setStatus(request_status::closed);
    m_doc.reset();
}

/**
* Close PDFDoc and free resources.
*/
void PDFExtractor::close()
{
    // TRACE(L"%hs!%ls\n", __FUNCTION__, m_fileName.c_str());
    m_fileName.clear();
    closeDoc();
}

/**
* Open new PDF document if requested file is different than open one.
* Close PDF if requested file name is nullptr.
* Set Request::status to active if new document has been open successfuly.
*
* @return true if PdfDoc is valid
*/
bool PDFExtractor::open()
{
    auto newFile{ false };
    std::wstring requestFileName;
    {
        std::lock_guard lock(m_data->mutex);
        const auto fileName{ m_data->getRequestFileName() };
        if (fileName)
        {
            TRACE(L"%hs!%request file name=%ls\n", __FUNCTION__, fileName);
            requestFileName.assign(fileName);
        }
        else
        {
            TRACE(L"%hs!nullptr\n", __FUNCTION__);
        }
    }
    if (requestFileName.empty())
    {
        close();
    }
    else
    {
        if (m_fileName.empty())
        {
            m_fileName = std::move(requestFileName);
            newFile = true;
        }
        else if (wcsicmp(m_fileName.c_str(), requestFileName.c_str()))
        {
            close();
            m_fileName = std::move(requestFileName);
            newFile = true;
        }
    }

    if (newFile)
    {
        closeDoc();
        if (!m_fileName.empty())
        {
            m_data->setStatus(request_status::active);
            m_doc = std::make_unique<PDFDoc>(m_fileName.c_str(), m_fileName.size());
            TRACE(L"%hs!%ls\n", __FUNCTION__, m_fileName.c_str());
        }

        if (m_doc)
        {
            if (!m_doc->isOk())
            {
                close();
                {
                    std::lock_guard lock(m_data->mutex);
                    m_data->setRequestResult(ft_fileerror);
                }

            }
        }
    }
    return m_doc ? true : false;
}

/**
* Remove characters from input string.
* 
* @param[in,out]    str     string to be cleaned up
* @param[in]        cchStr  count of characters in string
* @param[in]        delims  characters to clean up from str
* @return number of characters in str
*/
size_t PDFExtractor::removeDelimiters(wchar_t* str, size_t cchStr, const wchar_t* delims)
{
    size_t i{ 0 };
    if (str && (cchStr > 0) && delims)
    {
        size_t n{ 0 };
        for (; n < cchStr; ++n)
        {
            if (wcschr(delims, str[n]))
                continue;

            if (i != n)
                str[i] = str[n];

            ++i;
        }
        if (i != n)
            str[i] = 0;
    }
    return i;
}

/**
* Convert nibble value to hex character.
*
* @param[in]    nibble   nibble to convert
* @return converted hex character
*/
wchar_t PDFExtractor::nibble2wchar(char nibble)
{
    auto ret{ L'x' };
    if ((nibble >= 0) && (nibble <= 9))
        ret = nibble + L'0';
    else if ((nibble >= 0x0A) && (nibble <= 0x0F))
        ret = nibble - 0x0A + L'A';

    return ret;
}

/**
* Convert binary value to hex string and appends to destination.
*
* @param[out]   dst     destination string
* @param[in]    cbDst   size of dst in bytes
* @param[in]    value   value to convert to hex string
*/
void PDFExtractor::appendHexValue(wchar_t* dst, size_t cbDst, int value)
{
    wchar_t tmp[3]{ 0 };

    tmp[0] = nibble2wchar((value >> 4) & 0x0F);
    tmp[1] = nibble2wchar(value & 0x0F);
    StringCbCatW(dst, cbDst, tmp);
}

/**
* Extract metadata information from PDF and convert to wchar_t.
* Data exchange is guarded with mutex.
* 
* @param[in]    doc     pointer to PDFDoc object
* @param[in]    key     one of values from #metaDataFields
*/
void PDFExtractor::getMetadataString(PDFDoc* doc, const char* key)
{
    Object objDocInfo;
    if (doc->getDocInfo(&objDocInfo)->isDict())
    {
        Object obj;
        const auto dict{ objDocInfo.getDict() };
        if (dict->lookup(key, &obj)->isString())
        {
            TextString ts(obj.getString());
            if (ts.getLength())
            {
                ptrdiff_t len{ REQUEST_BUFFER_SIZE };

                std::lock_guard lock(m_data->mutex);
                UnicodeToUTF16(ts.getUnicode(), ts.getLength(), static_cast<wchar_t*>(m_data->getRequestBuffer()), &len);
                if (len != REQUEST_BUFFER_SIZE)
                    m_data->setRequestResult(ft_stringw);
            }
        }
        obj.free();
    }
    objDocInfo.free();
}

/**
* PDF document contains signature fields.
* It is not verified if document is signed or if signature is valid.
*
* @param[in]    doc     pointer to PDFDoc object
* @return true if SigFlags value > 0
*/
bool PDFExtractor::hasSignature(PDFDoc* doc)
{
    bool ret{ false };
    const auto catalog{ doc->getCatalog() };
    if (catalog)
    {
        const auto acroForm{ catalog->getAcroForm() };
        if (acroForm->isDict())
        {
            const auto dict{ acroForm->getDict() };
            if (dict)
            {
                Object obj;
                if (dict->lookup("SigFlags", &obj)->isInt())
                {
                    // verify only bit position 1; 
                    // bit position 2 informs that signature will be invalidated
                    // if document is not saved as incremental
                    ret = static_cast<bool>(obj.getInt() & 0x01);
                }
                obj.free();
            }
        }
    }
    return ret;
}

/**
* PDF document has outlines (bookmarks).
* It is not verified if document outline has titles (labels).
*
* @param[in]    doc     pointer to PDFDoc object
* @return true if document has outlines
*/
bool PDFExtractor::hasOutlines(PDFDoc* doc)
{
    const auto outline{ doc->getOutline() };
    if (outline && outline->getItems())
    {
        return true;
    }
    return false;
}

/**
* Traverse recursively through document's outlines (bookmarks).
*
* @param[in]    node    pointer to GList node
* @return false - continue with extraction, true - abort
*/
bool PDFExtractor::getOulinesTitles(GList* node)
{
    int done{ false };
    if (node)
    {
        for (auto n{ 0 }; (!done) && (n < node->getLength()); n++)
        {
            const auto item{ static_cast<OutlineItem*>(node->get(n)) };
            const auto titleLen{ item->getTitleLength() };
            if (titleLen)
            {
                if (m_data->output(reinterpret_cast<const char*>(item->getTitle()), titleLen, true))
                    return true;
            }
            if (item->hasKids())
            {
                item->open();
                done = getOulinesTitles(item->getKids());
                item->close();
            }
        }
    }
    return done;
}

/**
* Extract document outline.
*
* @param[in]    doc     pointer to PDFDoc object
*/
void PDFExtractor::getOulines(PDFDoc* doc)
{
    const auto outline{ doc->getOutline() };
    if (outline)
    {
        getOulinesTitles(outline->getItems());
    }
}

/**
* Extract PDF file identifier. 
* This value should be two MD5 strings.
* Data exchange is guarded with mutex.
*
* @param[in]    doc     pointer to PDFDoc object
*/
void PDFExtractor::getDocID(PDFDoc* doc)
{
    Object fileIDObj;

    doc->getXRef()->getTrailerDict()->dictLookup("ID", &fileIDObj);
    if (fileIDObj.isArray()) 
    {
        auto len{ REQUEST_BUFFER_SIZE };
        std::lock_guard lock(m_data->mutex);
        auto dst{ static_cast<wchar_t*>(m_data->getRequestBuffer()) };
        *dst = 0;
        // convert byte arrays to human readable strings
        for (int i{ 0 }; i < fileIDObj.arrayGetLength(); i++)
        {
            Object fileIDObj1;
            if (fileIDObj.arrayGet(i, &fileIDObj1)->isString())
            {
                const auto str{ fileIDObj1.getString() };
                if (i)
                    StringCbCatW(dst, len, L"-");

                for (int j{ 0 }; j < str->getLength(); j++)
                {
                    appendHexValue(dst, len, str->getChar(j));
                }
            }
            fileIDObj1.free();
        }
        if (*dst)
            m_data->setRequestResult(ft_stringw);
    }
    fileIDObj.free();
}

/**
* PDF document was updated incrementally without rewriting the entire file.
*
* @param[in]    doc     pointer to PDFDoc object
* @return true if PDF is incremental
*/
bool PDFExtractor::isIncremental(PDFDoc* doc)
{
    return (doc->getXRef()->getNumXRefTables() > 1) ? true : false;
}

/**
* From Portable document format - Part 1: PDF 1.7
* 14.8 Tagged PDF
* "Tagged PDF (PDF 1.4) is a stylized use of PDF that builds on the logical structure framework described in 14.7, "Logical Structure""
*
* @param[in]    doc     pointer to PDFDoc object
* @return true if PDF is tagged
*/
bool PDFExtractor::isTagged(PDFDoc* doc)
{
    return doc->getStructTreeRoot()->isDict() ? true : false;
}

/**
* "PDF Attribute" field data extraction.
* Data exchange is guarded with mutex.
*
* @param[in]    doc     pointer to PDFDoc object
*/
void PDFExtractor::getMetadataAttrStr(PDFDoc* doc)
{
    wchar_t attrs[]{ L"----------" };

    if (doc->okToPrint())
        attrs[0] = L'P';
    if (doc->okToCopy())
        attrs[1] = L'C';
    if (doc->okToChange())
        attrs[2] = L'M';
    if (doc->okToAddNotes())
        attrs[3] = L'N';
    if (isIncremental(doc))
        attrs[4] = L'I';
    if (isTagged(doc))
        attrs[5] = L'T';
    if (doc->isLinearized())
        attrs[6] = L'L';
    if (doc->isEncrypted())
        attrs[7] = L'E';
    if (hasSignature(doc))
        attrs[8] = L'S';
    if (hasOutlines(doc))
        attrs[9] = L'O';

    auto len{ REQUEST_BUFFER_SIZE };
    std::lock_guard lock(m_data->mutex);
    auto dst{ static_cast<wchar_t*>(m_data->getRequestBuffer()) };
    StringCbCopyW(dst, len, attrs);
    m_data->setRequestResult(ft_stringw);
}

/**
* "Created" and "Modified" fields data extraction.
* Convert PDF date and time to FILETIME structure.
* Data exchange is guarded with mutex.
*
* @param[in]    doc     pointer to PDFDoc object
* @param[in]    key     "CreationDate" or "ModDate"
*/
void PDFExtractor::getMetadataDate(PDFDoc* doc, const char* key)
{
    Object objDocInfo;
    if (doc->getDocInfo(&objDocInfo)->isDict())
    {
        Object obj;
        const auto dict{ objDocInfo.getDict() };
        if (dict->lookup(key, &obj)->isString())
        {
            const auto acrobatDateTimeString{ obj.getString()->getCString() };
            if (acrobatDateTimeString && (acrobatDateTimeString[0] == 'D') && (acrobatDateTimeString[1] == ':'))
            {
                // D:20080918111951
                // D:20080918111951Z
                // D:20080918111951-07'00'
                const auto len{ strlen(acrobatDateTimeString) };
                SYSTEMTIME sysTime{ };
                int hours{ 0 }, minutes{ 0 }, offset{ 0 };
                if (len >= 6U)  // D:YYYY is minimum
                {
                    // default values for hours and minutes, PDF 1.7
                    sysTime.wHour = 1;
                    sysTime.wMinute = 1;

                    std::from_chars(acrobatDateTimeString + 2U, acrobatDateTimeString + 6U, sysTime.wYear);
                    if (len >= 8U)
                    {
                        std::from_chars(acrobatDateTimeString + 6U, acrobatDateTimeString + 8U, sysTime.wMonth);
                        if (len >= 10U)
                        {
                            std::from_chars(acrobatDateTimeString + 8U, acrobatDateTimeString + 10U, sysTime.wDay);
                            if (len >= 12U)
                            {
                                std::from_chars(acrobatDateTimeString + 10U, acrobatDateTimeString + 12U, sysTime.wHour);
                                if (len >= 14U)
                                {
                                    std::from_chars(acrobatDateTimeString + 12U, acrobatDateTimeString + 14U, sysTime.wMinute);
                                    if (len >= 16U)
                                    {
                                        std::from_chars(acrobatDateTimeString + 14U, acrobatDateTimeString + 16U, sysTime.wSecond);
                                        if (len >= 19U)
                                        {
                                            std::from_chars(acrobatDateTimeString + 17U, acrobatDateTimeString + 19U, hours);
                                            if (len >= 22U)
                                            {
                                                std::from_chars(acrobatDateTimeString + 20U, acrobatDateTimeString + 22U, minutes);
                                            }
                                            offset = hours * 3600 + minutes * 60;
                                            if (acrobatDateTimeString[16] == '-')
                                                offset = 0 - offset;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    FILETIME fileTime{ };
                    if (SystemTimeToFileTime(&sysTime, &fileTime))
                    {
                        if (offset)
                        {
                            LARGE_INTEGER timeValue;
                            timeValue.HighPart = fileTime.dwHighDateTime;
                            timeValue.LowPart = fileTime.dwLowDateTime;
                            timeValue.QuadPart -= offset * 10000000ULL;
    
                            fileTime.dwHighDateTime = timeValue.HighPart;
                            fileTime.dwLowDateTime = timeValue.LowPart;
                        }
                        {
                            std::lock_guard lock(m_data->mutex);
                            memcpy(m_data->getRequestBuffer(), &fileTime, sizeof(FILETIME));
                            m_data->setRequestResult(ft_datetime);
                        }
                    }
                }
            }
        }
        obj.free();
    }
    objDocInfo.free();
}

/** 
* Convert a given point value to the unit given in unit.
*
* @param[in]    units   units index
* @return units conversion ratio, 0 for unknown units
*/
double PDFExtractor::getPaperSize(int units)
{
    switch (units)
    {
    case suMilliMeters:
        return 0.3528;
        break;
    case suCentiMeters:
        return 0.03528;
        break;
    case suInches:
        return 0.0139;
        break;
    case suPoints:
        return 1.0;
        break;
    default:
        return 0.0;
    }
}

/**
* Call specific extraction functions.
*/
void PDFExtractor::doWork()
{
    const auto field{ m_data->getRequestField() };
    switch (field)
    {
    case fiTitle:
    case fiSubject:
    case fiKeywords:
    case fiAuthor:
    case fiCreator:
    case fiProducer:
        getMetadataString(m_doc.get(), metaDataFields[field]);
        break;
    case fiDocStart:
    case fiFirstRow:
    case fiText:
        m_tc.output(m_doc.get(), m_data.get());
        break;
    case fiNumberOfPages:
        m_data->setValue(m_doc->getNumPages(), ft_numeric_32);
        break;
    case fiPDFVersion:
        m_data->setValue(m_doc->getPDFVersion(), ft_numeric_floating);
        break;
    case fiPageWidth:
        m_data->setValue(m_doc->getPageCropWidth(1) * getPaperSize(m_data->getRequestUnit()), ft_numeric_floating);
        break;
    case fiPageHeight:
        m_data->setValue(m_doc->getPageCropHeight(1) * getPaperSize(m_data->getRequestUnit()), ft_numeric_floating);
        break;
    case fiCopyingAllowed:
        m_data->setValue<BOOL>(m_doc->okToCopy(), ft_boolean);
        break;
    case fiPrintingAllowed:
        m_data->setValue<BOOL>(m_doc->okToPrint(), ft_boolean);
        break;
    case fiAddCommentsAllowed:
        m_data->setValue<BOOL>(m_doc->okToAddNotes(), ft_boolean);
        break;
    case fiChangingAllowed:
        m_data->setValue<BOOL>(m_doc->okToChange(), ft_boolean);
        break;
    case fiEncrypted:
        m_data->setValue<BOOL>(m_doc->isEncrypted(), ft_boolean);
        break;
    case fiTagged:
        m_data->setValue(isTagged(m_doc.get()), ft_boolean);
        break;
    case fiLinearized:
        m_data->setValue<BOOL>(m_doc->isLinearized(), ft_boolean);
        break;
    case fiIncremental:
        m_data->setValue(isIncremental(m_doc.get()), ft_boolean);
        break;
    case fiSignature:
        m_data->setValue(hasSignature(m_doc.get()), ft_boolean);
        break;
    case fiCreationDate:
        getMetadataDate(m_doc.get(), "CreationDate");
        break;
    case fiLastModifiedDate:
        getMetadataDate(m_doc.get(), "ModDate");
        break;
    case fiID:
        getDocID(m_doc.get());
        break;
    case fiAttributesString:
        getMetadataAttrStr(m_doc.get());
        break;
    case fiOutlines:
        getOulines(m_doc.get());
        break;
    default:
        break;
    }
    TRACE(L"%hs!%ls!%d complete!status=%ld\n", __FUNCTION__, m_fileName.c_str(), field, m_data->getStatus());
}

/**
* Extractor thread main function.
* To start extraction, set request params and raise producer event from TC thread.
* When extraction is complete, it raises consumer event to wake TC thread up.
* To exit thread, TC must set active to 0 and raise producer event.
*/
void PDFExtractor::waitForProducer()
{
    m_data->setActive(true);
    while (m_data->isActive())
    {
        // !!! producer idle point !!!
        const auto dwRet{ m_data->waitForProducer(PRODUCER_TIMEOUT) };
        if (dwRet == WAIT_OBJECT_0)
        {
            auto status{ m_data->getStatus() };
            if ((status != request_status::cancelled) && (status != request_status::complete) && open())
                doWork();
            // change status from active to complete
            m_data->setStatusCond(request_status::complete, request_status::active);
            // change status from cancelled to closed
            status = m_data->setStatusCond(request_status::closed, request_status::cancelled);
            if ((status == request_status::cancelled)
                || (globalOptionsFromIni.noCache)                       // no cache in options, close file
                // || (m_data->getRequestFlags() & CONTENT_NO_CACHE)    // TODO request is marked as non cached, close file
                )
            {
                close();
            }

            // inform consumer that extraction is complete or closed
            TRACE(L"%hs!status=%ld!TC notified\n", __FUNCTION__, status);
            // if consumer has already notified us, reset that event and wait for new one
            m_data->resetProducer();
            // notify consumer that producer is ready for new request
            m_data->notifyConsumer();
        }
        else if (dwRet == WAIT_TIMEOUT)
        {
            // if there are no new requests, close PDFDoc
            close();
        }
        else
        {
            // set thread exit flag
            m_data->setActive(false);
        }
    }
    // thread is about to exit, close PDFDoc
    close();
}

/**
* Extraction thread entry function.
* This is a static function, there is no access to non-static functions.
* Pointer to PDFExtractor object (this) is passed in function parameter.
* 
* @param[in]    param   pointer to PDFExtractor object (this)
* @return 0
*/
unsigned int __stdcall threadFunc(void* param)
{
    auto extractor{ static_cast<PDFExtractor*>(param) };
    if (extractor)
    {
        extractor->waitForProducer();
    }
    TRACE(L"%hs!end thread\n", __FUNCTION__);
    _endthreadex(0);

    return 0;
}

/**
* Start extraction thread, if not already started.
* Create unnamed events with automatic reset.
*
* @return thread ID number
*/
unsigned int PDFExtractor::startWorkerThread()
{
    return m_data->start(threadFunc, this);
}

// #pragma optimize( "", off )

/**
* Raise producer event to start extraction and wait for consumer event.
* If consumer doesn't respond in time, function returns #ft_timeout.
* 
* @param[in]    timeout time to wait for Consumer signal in miliseconds
* @return result of an extraction, #ft_timeout if consumer did not send signal, #ft_fileerror if error
*/
int PDFExtractor::waitForConsumer(DWORD timeout)
{
    auto result{ ft_fileerror };
    const auto dwRet{ m_data->notifyProducerWaitForConsumer(timeout) };
    switch (dwRet)
    {
    case WAIT_OBJECT_0:
        result = ft_setsuccess;
        break;
    case WAIT_TIMEOUT:
        result = ft_timeout;
        break;
    default:
        TRACE(L"%hs!%ls!ret=%lx err=%lu\n", __FUNCTION__, dwRet, GetLastError());
        m_data->setStatusCond(request_status::cancelled, request_status::active);
        break;
    }

    return result;
}
// #pragma optimize( "", on )

/**
* Assign data from TC to internal structure.
* If TC doesn't provide buffer for output data (compare), a new buffer is created.
*
* @param[in]    fileName        full path to PDF document
* @param[in]    field           index of the field
* @param[in]    unit            index of the unit, -1 for fiText and fiOutlines fields when searched string is found
* @param[in]    flags           TC flags
* @param[in]    timeout         producer timeout (in text extraction)
* @return       ft_fieldempty if data cannot be set, ft_setsuccess if successfuly set
*/
int PDFExtractor::initData(const wchar_t* fileName, int field, int unit, int flags, DWORD timeout)
{
    auto retval{ ft_fieldempty };
    const auto fiTextOrOutlines{ (field == fiText) || (field == fiOutlines) };
    // check if previous extraction is still active, try to stop it
    if (fiTextOrOutlines && (unit <= 0))
    {
        stop();
        if (unit == -1)
            return retval;
    }
    else if (m_data->getStatus() == request_status::cancelled)       // extraction has been cancelled, but PDFDoc isn't closed yet, wait for it
        m_data->waitForConsumer(CONSUMER_TIMEOUT);

    const auto status{ m_data->getStatus() };
    if (!(  (status == request_status::cancelled)
        || ((status == request_status::closed)   && (unit > 0) && fiTextOrOutlines)  // extraction is closed but TC wants next text block
        || ((status == request_status::complete) && (unit > 0) && fiTextOrOutlines)  // extraction is completed but TC wants next text block
       ))
    {
        retval = m_data->initRequest(fileName, field, unit, flags, timeout);
    }
    TRACE(L"%hs!%ls!status=%ld retval=%d\n", __FUNCTION__, fileName, status, retval);
    return retval;
}

/**
* Start data extraction form PDF document.
* Thread state is changed from complete to active to enable new request.
* Producer timeout is set to low value, because producer is TC. It should respond in short time.
*
* @param[in]    fileName        full path to PDF document
* @param[in]    field           index of the field, where to search
* @param[in]    unit            index of the unit, -1 for fiText and fiOutlines fields when searched string is found
* @param[out]   dst             buffer for retrieved data
* @param[in]    dstSize         sizeof dst buffer in bytes (NUL char for stringw and fulltextw included)
* @param[in]    flags           TC flags
* @return       result of an extraction
*/
int PDFExtractor::extract(const wchar_t* fileName, int field, int unit, void* dst, int dstSize, int flags)
{
    auto result{ initData(fileName, field, unit, flags, PRODUCER_TIMEOUT) };
    if (result != ft_fieldempty)
    {
        const auto fiTextOrOutlines{ (field == fiText) || (field == fiOutlines) };
        if (fiTextOrOutlines)
        {
            if (unit == 0)
            {
                m_data->setStatusCond(request_status::active, request_status::complete);
                if (startWorkerThread())
                    result = waitForConsumer(PRODUCER_TIMEOUT);
            }
            else if (result == ft_setsuccess)
                result = waitForConsumer(PRODUCER_TIMEOUT);

            if (dst)
            {
                // if producer thread is slow, send anything what is extracted
                if ((result == ft_timeout) || (result == ft_setsuccess))
                {
                    result = ft_fulltextw;
                    std::lock_guard lock(m_data->mutex);
                    auto src{ m_data->getRequestBuffer() };
                    const auto end{ m_data->getRequestPtr() };
                    const auto srcLen{ static_cast<char*>(end) - static_cast<char*>(src) };
                    if (srcLen == 0)
                    {
                        TRACE(L"%hs!dstSize=%d SPACE\n", __FUNCTION__, dstSize);
                        StringCbCopyW(static_cast<wchar_t*>(dst), dstSize, L" ");
                    }
                    else
                    {
                        dstSize &= ~1; // round to 2
                        const auto srcLeft{ srcLen - dstSize + sizeOfWchar};

                        // wchar_t* dstEnd{ nullptr };
                        // size_t dstLeft{ 0 };
                        // StringCbCopyExW(static_cast<wchar_t*>(dst), dstSize, static_cast<const wchar_t*>(src), &dstEnd, &dstLeft, 0);
                        StringCbCopyW(static_cast<wchar_t*>(dst), dstSize, static_cast<const wchar_t*>(src));

                        // TRACE(L"%hs!srcLen=%lld srcLeft=%lld, dstSize=%d dstLeft=%llu\n", __FUNCTION__, srcLen, srcLeft, dstSize, dstLeft);
                        TRACE(L"%hs!srcLen=%lld srcLeft=%lld, dstSize=%d\n", __FUNCTION__, srcLen, srcLeft, dstSize);
                        if (srcLeft > 0)
                        {
                            memmove(src, static_cast<char*>(src) + dstSize - sizeOfWchar, srcLeft + sizeOfWchar);
                            m_data->setRequestPtr(static_cast<char*>(src) + srcLeft);
                        }
                        else
                        {
                            m_data->setRequestPtr(src);
                            *(static_cast<int64_t*>(src)) = 0;
                        }
                    }
                }
            }
            else
                result = ft_nosuchfield;
        }
        else
        {
            m_data->setStatusCond(request_status::active, request_status::complete);
            if (startWorkerThread())
                result = waitForConsumer(CONSUMER_TIMEOUT);

            // if producer thread is slow, send empty field
            if (result == ft_timeout)
                result = ft_fieldempty;
            else if ((result == ft_setsuccess) && dst)
            {
                std::lock_guard lock(m_data->mutex);
                auto src{ m_data->getRequestBuffer() };
                result = m_data->getRequestResult();
                switch (result)
                {
                case ft_numeric_32:
                case ft_boolean:
                    memcpy(dst, src, sizeof(int32_t));
                    break;
                case ft_numeric_floating:
                case ft_datetime:
                    memcpy(dst, src, sizeof(int64_t));
                    break;
                case ft_stringw:
                case ft_fulltextw:
                    dstSize &= ~1; // round to 2
                    StringCbCopyW(static_cast<wchar_t*>(dst), dstSize, static_cast<const wchar_t*>(src));
                    m_data->setRequestPtr(src);
                    *(static_cast<int64_t*>(src)) = 0LL;
                    break;
                }
            }
        }
    }
    TRACE(L"%hs!%ls!result=%d\n", __FUNCTION__, fileName, result);
    return result;
}

/**
* Notifiy text extracting threads that the state of requests is changed.
* Threads should close PdfDocs and exit.
*/
void PDFExtractor::abort()
{
    m_data->abort();
    if (m_search)
        m_search->abort();
}

/**
* Notify text extracting threads that eh state of requests is changed.
* Threads should return back to idle point in #waitForProducer and close PdfDocs.
*/
void PDFExtractor::stop()
{
    // if extraction is active, mark it as cancelled
    m_data->stop();
    if (m_search)
        m_search->stop();
}

/**
* Notifiy text extracting threads that the state of requests has changed.
* Threads should return back to idle point in #waitForProducer without closing PdfDocs.
*/
void PDFExtractor::done()
{
    m_data->done();
    if (m_search)
        m_search->done();
}

/**
* Start data extraction and compare extracted data from two PDF documents.
* If extracted data is binary identical, function returns #ft_compare_eq.
* If data is not binary identical, delimiters are removed and text is compared case-insensitive.
* If data is textualy identical, function returns ft_compare_eq_txt.
* If both data fields are empty, function returns #ft_compare_eq.
* 
* @param[in]    progresscallback    pointer to callback function to inform the calling program about the compare progress
* @param[in]    fileName1           first file name to be compared
* @param[in]    fileName2           second file name to be compared
* @param[in]    field               field data to compare
* @return result of comparision
*/
int PDFExtractor::compare(PROGRESSCALLBACKPROC progresscallback, const wchar_t* fileName1, const wchar_t* fileName2, int field)
{
    static const wchar_t delims[]{ L" \r\n\b\f\t\v\x00a0\x202f\x2007\x2009\x2060" };
    size_t bytesProcessed{ 0U };
    auto eq_txt{ false };

    // set timeout to long wait, because it waits for another extraction thread
    auto result{ initData(fileName1, field, 0, 0, CONSUMER_TIMEOUT) };
    if (result != ft_setsuccess)
        return ft_compare_next;

    if (!m_search)
        m_search = std::make_unique<PDFExtractor>();

    // set timeout to long wait, because it waits for another extraction thread to complete
    result = m_search->initData(fileName2, field, 0, 0, CONSUMER_TIMEOUT);
    if (result != ft_setsuccess)
        return ft_compare_next;

    // start threads
    if (startWorkerThread() && m_search->startWorkerThread())
    {
        // change status from complete to active
        m_data->setStatusCond(request_status::active, request_status::complete);
        m_search->m_data->setStatusCond(request_status::active, request_status::complete);

        // get start time
#if _WIN32_WINNT >= _WIN32_WINNT_VISTA
        auto startCounter{ GetTickCount64() };
#else
        auto startCounter{ GetTickCount() };
#endif
        do
        {
            // wait for consumers to extract data
            result = m_data->compareWaitForConsumers(m_search->m_data.get(), CONSUMER_TIMEOUT);
            // time spent in extraction = now - start
#if _WIN32_WINNT >= _WIN32_WINNT_VISTA
            const auto now{ GetTickCount64() };
#else
            const auto now{ GetTickCount() };
#endif
            if (result > 0)
            {
                // extraction completed successfuly, mark result as not equal
                result = ft_compare_not_eq;

                // protect data from both threads, std::scoped_lock needs C++17
                std::scoped_lock lock(m_data->mutex, m_search->m_data->mutex);

                // cast from void* to wchar_t*
                auto start1{ static_cast<wchar_t*>(m_data->getRequestBuffer()) };
                size_t len1{ 0 };
                StringCchLengthW(start1, REQUEST_BUFFER_SIZE / sizeof(wchar_t), &len1);

                auto start2{ static_cast<wchar_t*>(m_search->m_data->getRequestBuffer()) };
                size_t len2{ 0 };
                StringCchLengthW(start2, REQUEST_BUFFER_SIZE / sizeof(wchar_t), &len2);

                // string len to compare
                const auto min_len{ len1 < len2 ? len1 : len2 };
                    
                if (min_len)
                {
                    // compare binary
                    if (!wmemcmp(start1, start2, min_len))
                    {
                        TRACE(L"%hs!binary!%Iu wchars equal\n", __FUNCTION__, min_len);
                        bytesProcessed += min_len;
                        result = ft_compare_eq;
                    }
                    else
                    {
                        // remove delimiters, spaces
                        const auto len1X{ removeDelimiters(start1, len1, delims) };
                        const auto len2X{ removeDelimiters(start2, len2, delims) };
                        // string len to compare
                        const auto min_lenX{ len1X < len2X ? len1X : len2X };
                        if (min_lenX)
                        {
                            // compare as text, case-insensitive, using locale specific information
                            if (!_wcsnicoll_l(start1, start2, min_lenX, m_locale.get()))
                            {
                                TRACE(L"%hs!text!%Iu wchars equal\n", __FUNCTION__, min_lenX);
                                bytesProcessed += min_lenX;
                                result = ft_compare_eq;
                                eq_txt = true;
                            }
                            else
                            {
                                // text is not equal, abort
                                TRACE(L"%hs!not equal!'%ls' != '%ls'\n", __FUNCTION__, start1, start2);
                                break;
                            }
                        }
                        else if (len1X == len2X)
                        {
                            TRACE(L"%hs!empty text\n", __FUNCTION__);
                            result = ft_compare_eq;
                            eq_txt = true;
                        }
                    }
                }
                else if (len1 == len2)
                {
                    TRACE(L"%hs!no data\n", __FUNCTION__);
                    result = ft_compare_eq;
                }

                if ((result == ft_compare_eq) && min_len && ((len1 > min_len) || (len2 > min_len)))
                {
                    // discard compared data
                    if (len1 >= min_len)
                        wmemmove(start1, start1 + min_len, len1 - min_len);

                    if (len2 >= min_len)
                        wmemmove(start2, start2 + min_len, len2 - min_len);

                    // part of a string was equal, compare rest
                    result = ft_compare_not_eq;
                }

                // adjust string end pointer and remaining buffer size
                m_data->setRequestPtr(start1 + len1 - min_len);
                m_search->m_data->setRequestPtr(start2 + len2 - min_len);
            }
            else
            {
                // no data extracted in both files
                if (result == ft_fieldempty)
                {
                    TRACE(L"%hs!empty fields\n", __FUNCTION__);
                    // both fields are equaly "empty"
                    result = ft_compare_eq;
                }
                else
                {
                    // error
                    TRACE(L"%hs!error\n", __FUNCTION__);
                }
                break;
            }
            if (progresscallback && (now - startCounter > PRODUCER_TIMEOUT))
            {
                // inform TC about progress
                if (progresscallback(static_cast<int>(bytesProcessed)))
                {
                    // abort by user
                    TRACE(L"%hs!user abort\n", __FUNCTION__);
                    result = ft_compare_abort;
                    break;
                }
                // reset counter
                bytesProcessed = 0U;
                startCounter = now;
            }
        } 
        while (   (request_status::active == m_data->getStatus())
               && (request_status::active == m_search->m_data->getStatus())
              );

        // if data was once compared as text, it is not binary equal
        if ((result == ft_compare_eq) && eq_txt)
            result = ft_compare_eq_txt;

        // don't close PDFDocs, they may be used again
        done();
    }
    else
    {
        TRACE(L"%hs!unable to start threads\n", __FUNCTION__);
    }
    return result;
}
