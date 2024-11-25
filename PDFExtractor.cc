#include "PDFExtractor.hh"
#include <CharTypes.h>
#include "xPDFInfo.hh"
#include <locale.h>
#include <wchar.h>
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
* The keys required to read the Document Info Directory fields.
*/
static constexpr const char* DocInfoFields[] =
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
    m_data->setStatus(requestStatus::closed);
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
            m_data->setStatus(requestStatus::active);
            m_doc = std::make_unique<PDFDocEx>(m_fileName.c_str(), m_fileName.size());
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
            {
                continue;
            }

            if (i != n)
            {
                str[i] = str[n];
            }

            ++i;
        }
        if (i != n)
        {
            str[i] = 0;
        }
    }
    return i;
}

/**
* Extract metadata information from PDF and convert to wchar_t.
* Data exchange is guarded with mutex.
* 
* @param[in]    key     one of values from #metaDataFields
*/
void PDFExtractor::getMetadataString(const char* key)
{
    std::unique_ptr<GString> value{ m_doc->getMetadataString(key) };
    if (value)
    {
        m_data->setValue(value.get(), ft_stringw);
    }
}

/**
* Extract PDF file identifier.
* This value should be two MD5 strings.
*/
void PDFExtractor::getDocID()
{
    std::unique_ptr<GString> id{ m_doc->getID() };
    if (id)
    {
        m_data->setValue(id.get(), ft_stringw);
    }
}

/**
* Traverse recursively through document's outlines (bookmarks).
*
* @param[in]    node    pointer to GList node
* @return false - continue with extraction, true - abort
*/
bool PDFExtractor::getOulinesTitles(GList* node)
{
    auto outlinesDone{ false };
    if (node)
    {
        for (auto n{ 0 }; (!outlinesDone) && (n < node->getLength()); n++)
        {
            const auto item{ static_cast<OutlineItem*>(node->get(n)) };
            const auto titleLen{ item->getTitleLength() };
            if (titleLen)
            {
                if (m_data->output(reinterpret_cast<const char*>(item->getTitle()), titleLen, true))
                {
                    return true;
                }
            }
            if (item->hasKids())
            {
                item->open();
                outlinesDone = getOulinesTitles(item->getKids());
                item->close();
            }
        }
    }
    return outlinesDone;
}

/**
* Extract document outline.
*/
void PDFExtractor::getOulines()
{
    const auto outline{ m_doc->getOutline() };
    if (outline)
    {
        getOulinesTitles(outline->getItems());
    }
}

/**
* "PDF Attribute" field data extraction.
* Data exchange is guarded with mutex.
*/
void PDFExtractor::getAttrStr()
{
    wchar_t attrs[]{ L"----------------"};
    size_t n{ 0 };

    if (globalOptionsFromIni.attrPrintable)
    {
        if (m_doc->okToPrint())
            attrs[n] = globalOptionsFromIni.attrPrintable;
        n++;
    }
    if (globalOptionsFromIni.attrCopyable)
    {
        if (m_doc->okToCopy())
            attrs[n] = globalOptionsFromIni.attrCopyable;
        n++;
    }
    if (globalOptionsFromIni.attrChangeable)
    {
        if (m_doc->okToChange())
            attrs[n] = globalOptionsFromIni.attrChangeable;
        n++;
    }
    if (globalOptionsFromIni.attrCommentable)
    {
        if (m_doc->okToAddNotes())
            attrs[n] = globalOptionsFromIni.attrCommentable;
        n++;
    }
    if (globalOptionsFromIni.attrIncremental)
    {
        if (m_doc->isIncremental())
            attrs[n] = globalOptionsFromIni.attrIncremental;
        n++;
    }
    if (globalOptionsFromIni.attrTagged)
    {
        if (m_doc->isTagged())
            attrs[n] = globalOptionsFromIni.attrTagged;
        n++;
    }
    if (globalOptionsFromIni.attrLinearized)
    {
        if (m_doc->isLinearized())
            attrs[n] = globalOptionsFromIni.attrLinearized;
        n++;
    }
    if (globalOptionsFromIni.attrEncrypted)
    {
        if (m_doc->isEncrypted())
            attrs[n] = globalOptionsFromIni.attrEncrypted;
        n++;
    }
    if (globalOptionsFromIni.attrProtected)
    {
        if (m_doc->getErrorCode() == errEncrypted)
            attrs[n] = globalOptionsFromIni.attrProtected;
        n++;
    }
    if (globalOptionsFromIni.attrSigned)
    {
        if (m_doc->hasSignature())
            attrs[n] = globalOptionsFromIni.attrSigned;
        n++;
    }
    if (globalOptionsFromIni.attrOutlined)
    {
        if (m_doc->hasOutlines())
            attrs[n] = globalOptionsFromIni.attrOutlined;
        n++;
    }
    if (globalOptionsFromIni.attrEmbeddedFiles)
    {
        if (m_doc->hasEmbeddedFiles())
            attrs[n] = globalOptionsFromIni.attrEmbeddedFiles;
        n++;
    }
    attrs[n] = L'\0';
    m_data->setValue(attrs, ft_stringw);
}

/**
* Convert parts of Acrobat Date to uint16_t.
* Used to check if conversion is successful.
*
* param[in] date    pointer to start of the string to convert
* param[in] len     number of chars to convert
* param[out] result result of conversion
* return true - conversion success, false - conversion failed
*/
bool PDFExtractor::dateToInt(const char* date, uint8_t len, uint16_t& result)
{
    const auto conv{ std::from_chars(date, date + len, result) };
    if ((conv.ec == std::errc()) && (conv.ptr == date + len))
    {
        return true;
    }
    return false;
}


/**
* Convert PDF date and time to FILETIME structure.
* PDF 1.0 was published in 1993 and had arbitrary date format. This plugin doesn't handle those formats.
* PDF 1.1 was published in 1994 and specified date time format as D:YYYYMMDDHHmmss.
*
* String with year 1909-1913 is a Distiller y2k bug.
*
* @param[in]    pdfDateTime     pointer to PDF date time string
* @param[out]   fileTime        result of a conversion
* @return       true - conversion succeedeed, false - failed
*/
bool PDFExtractor::PdfDateTimeToFileTime(const GString& pdfDateTime, FILETIME& fileTime)
{
    auto const* acrobatDateTimeString{ pdfDateTime.getCString() };
    auto len{ pdfDateTime.getLength() };
    if (acrobatDateTimeString && (len >= 4))
    {
        // D:20080918111951
        // D:20080918111951Z
        // D:20080918111951-07'00'
        // 2023-04-25T12:13:14Z
        // 2023-04-25T12:13:14+01
        // 2023-04-25T12:13:14+0100
        // 2023-04-25T12:13:14-01:00
        if ((acrobatDateTimeString[0] == 'D') && (acrobatDateTimeString[1] == ':'))
        {
            acrobatDateTimeString += 2U;
            len -= 2;
        }
        // YYYY is minimum
        if (len >= 4)
        {
            SYSTEMTIME pdfLocalTime{ };
            // default values, PDF 1.7
            pdfLocalTime.wMonth = 1;
            pdfLocalTime.wDay = 1;

            if (dateToInt(acrobatDateTimeString, 4U, pdfLocalTime.wYear))
            {
                int offset{ 0 };
                // from gpdf/poppler, y2k bug in Distiller
                // CCYYYMMDDHHmmSS
                // CC - century = 19
                if ((pdfLocalTime.wYear >= 1909) && (pdfLocalTime.wYear <= 1913) && (len > 14))
                {
                    // if year is between 199x and 203x
                    acrobatDateTimeString += 2U; // skip century
                    if (dateToInt(acrobatDateTimeString, 3U, pdfLocalTime.wYear))
                    {
                        pdfLocalTime.wYear += 1900;
                        acrobatDateTimeString += 3U;
                        len -= 5U;
                    }
                    else
                    {
                        pdfLocalTime.wYear = 0;  // mark error
                    }
                }
                else
                {
                    acrobatDateTimeString += 4U;
                    len -= 4;
                }
                if (len && (*acrobatDateTimeString == '-'))
                {
                    acrobatDateTimeString++;
                    len--;
                }
                if ((len >= 2) && dateToInt(acrobatDateTimeString, 2U, pdfLocalTime.wMonth))
                {
                    acrobatDateTimeString += 2U;
                    len -= 2;
                    if (len && (*acrobatDateTimeString == '-'))
                    {
                        acrobatDateTimeString++;
                        len--;
                    }
                    if ((len >= 2) && dateToInt(acrobatDateTimeString, 2U, pdfLocalTime.wDay))
                    {
                        acrobatDateTimeString += 2U;
                        len -= 2;
                        if (len && (*acrobatDateTimeString == 'T'))
                        {
                            acrobatDateTimeString++;
                            len--;
                        }
                        if ((len >= 2) && dateToInt(acrobatDateTimeString, 2U, pdfLocalTime.wHour))
                        {
                            acrobatDateTimeString += 2U;
                            len -= 2;
                            if (len && (*acrobatDateTimeString == ':'))
                            {
                                acrobatDateTimeString++;
                                len--;
                            }
                            if ((len >= 2) && dateToInt(acrobatDateTimeString, 2U, pdfLocalTime.wMinute))
                            {
                                acrobatDateTimeString += 2U;
                                len -= 2;
                                if (len && (*acrobatDateTimeString == ':'))
                                {
                                    acrobatDateTimeString++;
                                    len--;
                                }
                                if ((len >= 2) && dateToInt(acrobatDateTimeString, 2U, pdfLocalTime.wSecond))
                                {
                                    acrobatDateTimeString += 2U;
                                    len -= 2;
                                    if (len >= 3)
                                    {
                                        offset = 1;
                                        if (*acrobatDateTimeString == '+')
                                        {
                                            offset = -1;
                                        }
                                        acrobatDateTimeString++;
                                        len--;
                                        uint16_t offsetHours{ 0 }, offsetMinutes{ 0 };
                                        if (dateToInt(acrobatDateTimeString, 2U, offsetHours))
                                        {
                                            acrobatDateTimeString += 2U;
                                            len -= 2;
                                            if (len && ((*acrobatDateTimeString == ':') || (*acrobatDateTimeString == '\'')))
                                            {
                                                acrobatDateTimeString++;
                                                len--;
                                            }
                                            if (len >= 2)
                                            {
                                                dateToInt(acrobatDateTimeString, 2U, offsetMinutes);
                                            }
                                        }
                                        // offset in minutes
                                        offset *= offsetHours * 60 + offsetMinutes;
                                    }
                                }
                            }
                        }
                    }
                }
                {
                    // from Windows 7
                    using TzSpecificLocalTimeToSystemTimeEx_t = BOOL (*)(const PDYNAMIC_TIME_ZONE_INFORMATION, const LPSYSTEMTIME, LPSYSTEMTIME);
                    auto TzSpecificLocalTimeToSystemTimeEx_fn{ (TzSpecificLocalTimeToSystemTimeEx_t)GetProcAddress(GetModuleHandleW(L"Kernel32.dll"), "TzSpecificLocalTimeToSystemTimeEx") };
                    // from Windows 8, Advapi32.dll is loaded by RegCloseKey in DllMain
                    using EnumDynamicTimeZoneInformation_t = DWORD (*)(const DWORD, PDYNAMIC_TIME_ZONE_INFORMATION);
                    auto EnumDynamicTimeZoneInformation_fn{ (EnumDynamicTimeZoneInformation_t)GetProcAddress(GetModuleHandleW(L"Advapi32.dll"), "EnumDynamicTimeZoneInformation") };

                    if (EnumDynamicTimeZoneInformation_fn && TzSpecificLocalTimeToSystemTimeEx_fn)
                    {
                        DYNAMIC_TIME_ZONE_INFORMATION dtzi{ };
                        DWORD dwResult;
                        DWORD i{ 0 };
                        do
                        {
                            dwResult = EnumDynamicTimeZoneInformation_fn(i++, &dtzi);
                            if ((dwResult == ERROR_SUCCESS) && (dtzi.Bias == offset))
                            {
                                SYSTEMTIME utcTime{};
                                if (TzSpecificLocalTimeToSystemTimeEx_fn(&dtzi, &pdfLocalTime, &utcTime))
                                {
                                    if (SystemTimeToFileTime(&utcTime, &fileTime))
                                    {
                                        return true;
                                    }
                                    TRACE(L"%hs!SystemTimeToFileTime(%s) failed: %lu\n", __FUNCTION__, acrobatDateTimeString, GetLastError());
                                }
                                else
                                {
                                    TRACE(L"%hs!TzSpecificLocalTimeToSystemTimeEx(%s) failed: %lu\n", __FUNCTION__, acrobatDateTimeString, GetLastError());
                                }
                            }
                        } while (dwResult != ERROR_NO_MORE_ITEMS);

                    }
                }
                if (SystemTimeToFileTime(&pdfLocalTime, &fileTime))
                {
                    if (offset)
                    {
                        ULARGE_INTEGER timeValue{fileTime.dwLowDateTime, fileTime.dwHighDateTime};
                        timeValue.QuadPart += offset * 600000000ULL; // minutes to 100ns units

                        fileTime.dwHighDateTime = timeValue.HighPart;
                        fileTime.dwLowDateTime = timeValue.LowPart;
                    }
                    return true;
                }
                else
                {
                    TRACE(L"%hs!SystemTimeToFileTime(%s) failed: %lu\n", __FUNCTION__, acrobatDateTimeString, GetLastError());
                }
            }
        }
    }
    return false;
}

/**
* "Created", "Modified" and and "MetadataDate" fields data extraction.
* Convert PDF date and time to FILETIME structure.
*
* @param[in]    key     "CreationDate" or "ModDate" or "MetadataDate"
*/
void PDFExtractor::getMetadataDate(const char* key)
{
    std::unique_ptr<GString> dateTime{ m_doc->getMetadataDateTime(key) };
    if (dateTime)
    {
        FILETIME fileTime{ };
        if (PdfDateTimeToFileTime(*dateTime, fileTime))
        {
            m_data->setValue(fileTime, ft_datetime);
        }
    }
}

/**
* "CreatedRaw", "ModifiedRaw" and "MetadataDateRaw" fields data extraction.
* Get raw date fields, without conversion to FILETIME.
*
* @param[in]    key     "CreationDate", "ModDate" or "MetadataDate"
*/
void PDFExtractor::getMetadataDateRaw(const char* key)
{
    std::unique_ptr<GString> dateTime{ m_doc->getMetadataDateTime(key) };
    if (dateTime)
    {
        if (globalOptionsFromIni.removeDateRawDColon)
        {
            if (dateTime->cmpN("D:", 2) == 0)
            {
                dateTime->del(0, 2);
            }
        }
        m_data->setValue(dateTime.get(), ft_stringw);
    }
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
    case suCentiMeters:
        return 0.03528;
    case suInches:
        return 0.0139;
    case suPoints:
        return 1.0;
    default:
        return 0.0;
    }
}

/**
* Get PDF conformance value (PDF/A, PDF/X, PDF/E)
*/
void PDFExtractor::getConformance()
{
    std::unique_ptr<GString> conformance{ m_doc->getConformance() };
    if (conformance)
        m_data->setValue(conformance.get(), ft_stringw);
}

/**
* Get PDF version
* If globalOptionsFromIni.appendExtensionLevel=1, try to get PDF Extension Level.
* Valid for PDF > 1.7
*/
void PDFExtractor::getVersion()
{
    auto ver{ m_doc->getPDFVersion() };
    if ((ver >= 1.7) && globalOptionsFromIni.appendExtensionLevel)
    {
        auto ext{ m_doc->getAdbeExtensionLevel() };
        if ((ext > 0) && (ext < 10))
        {
            ver += ext / 100.0;

            std::lock_guard lock(m_data->mutex);
            auto dst{ static_cast<wchar_t*>(m_data->getRequestBuffer()) + sizeof(double) / sizeof(wchar_t) };
            StringCbPrintfW(dst, REQUEST_BUFFER_SIZE - sizeof(double), L"%.2f", ver);
        }
    }
    m_data->setValue(ver, ft_numeric_floating);
}

/**
* Get extensions form PDF Catalog.
* Extensions are in format:
* PREFIX BaseVersion.ExtensionLevel.ExtensionRevision
* e.g. "ADBE 1.7.3.1" or "ADBE 1.7.3;ISO_ 2.0.24064;ISO_ 2.0.24654;GLGR 1.7.1002"
*
*/
void PDFExtractor::getExtensions()
{
    std::unique_ptr<GString> extensions{ m_doc->getExtensions() };
    if (extensions)
    {
        m_data->setValue(extensions.get(), ft_stringw);
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
        [[fallthrough]];
    case fiSubject:
        [[fallthrough]];
    case fiKeywords:
        [[fallthrough]];
    case fiAuthor:
        [[fallthrough]];
    case fiCreator:
        [[fallthrough]];
    case fiProducer:
        getMetadataString(DocInfoFields[field]);
        break;
    case fiDocStart:
        [[fallthrough]];
    case fiFirstRow:
        [[fallthrough]];
    case fiText:
        m_tc.output(m_doc.get(), m_data.get());
        break;
    case fiNumberOfPages:
        m_data->setValue(m_doc->getNumPages(), ft_numeric_32);
        break;
    case fiNumberOfFontlessPages:
        m_data->setValue(m_doc->getNumFontlessPages(), ft_numeric_32);
        break;
    case fiNumberOfPagesWithImages:
        m_data->setValue(m_doc->getNumPagesWithImages(), ft_numeric_32);
        break;
    case fiPDFVersion:
        getVersion();
        break;
    case fiPageWidth:
        m_data->setValue(m_doc->getPageCropWidth(1) * getPaperSize(m_data->getRequestUnit()), ft_numeric_floating);
        break;
    case fiPageHeight:
        m_data->setValue(m_doc->getPageCropHeight(1) * getPaperSize(m_data->getRequestUnit()), ft_numeric_floating);
        break;
    case fiCopyable:
        m_data->setValue<BOOL>(m_doc->okToCopy(), ft_boolean);
        break;
    case fiPrintable:
        m_data->setValue<BOOL>(m_doc->okToPrint(), ft_boolean);
        break;
    case fiCommentable:
        m_data->setValue<BOOL>(m_doc->okToAddNotes(), ft_boolean);
        break;
    case fiChangeable:
        m_data->setValue<BOOL>(m_doc->okToChange(), ft_boolean);
        break;
    case fiEncrypted:
        m_data->setValue<BOOL>(m_doc->isEncrypted(), ft_boolean);
        break;
    case fiTagged:
        m_data->setValue(m_doc->isTagged(), ft_boolean);
        break;
    case fiLinearized:
        m_data->setValue<BOOL>(m_doc->isLinearized(), ft_boolean);
        break;
    case fiIncremental:
        m_data->setValue(m_doc->isIncremental(), ft_boolean);
        break;
    case fiSigned:
        m_data->setValue(m_doc->hasSignature(), ft_boolean);
        break;
    case fiOutlined:
        m_data->setValue(m_doc->hasOutlines(), ft_boolean);
        break;
    case fiEmbeddedFiles:
        m_data->setValue(m_doc->hasEmbeddedFiles(), ft_boolean);
        break;
    case fiProtected:
        m_data->setValue((m_doc->getErrorCode() == errEncrypted), ft_boolean);
        break;
    case fiCreationDate:
        getMetadataDate("CreationDate");
        break;
    case fiModifiedDate:
        getMetadataDate("ModDate");
        break;
    case fiMetadataDate:
        getMetadataDate("MetadataDate");
        break;
    case fiCreationDateRaw:
        getMetadataDateRaw("CreationDate");
        break;
    case fiModifiedDateRaw:
        getMetadataDateRaw("ModDate");
        break;
    case fiMetadataDateRaw:
        getMetadataDateRaw("MetadataDate");
        break;
    case fiID:
        getDocID();
        break;
    case fiAttributesString:
        getAttrStr();
        break;
    case fiConformance:
        getConformance();
        break;
    case fiOutlines:
        getOulines();
        break;
    case fiExtensions:
        getExtensions();
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
    auto timeout{ PRODUCER_TIMEOUT };

    while (m_data->isActive())
    {
        // !!! producer idle point !!!
        const auto dwRet{ m_data->waitForProducer(timeout) };
        if (dwRet == WAIT_OBJECT_0)
        {
            auto status{ m_data->getStatus() };
            if ((status != requestStatus::cancelled) && (status != requestStatus::complete) && open())
            {
                doWork();
            }
            // change status from active to complete
            m_data->setStatusCond(requestStatus::complete, requestStatus::active);
            // change status from cancelled to closed
            status = m_data->setStatusCond(requestStatus::closed, requestStatus::cancelled);
            if ((status == requestStatus::cancelled)
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

            timeout = PRODUCER_TIMEOUT;
        }
        else if (dwRet == WAIT_TIMEOUT)
        {
            // if there are no new requests, close PDFDoc and wait
            close();
            timeout = INFINITE;
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
static unsigned int __stdcall threadFunc(void* param)
{
    auto extractor{ static_cast<PDFExtractor*>(param) };
    TRACE(L"%hs!worker thread start\n", __FUNCTION__);
    if (extractor)
    {
        extractor->waitForProducer();
    }
    TRACE(L"%hs!worker thread end\n", __FUNCTION__);
    _endthreadex(0);

    return 0;
}

/**
* Start extraction thread, if not already started.
* Create unnamed events with automatic reset.
*
* @return thread ID number
*/
uint32_t PDFExtractor::startWorkerThread()
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
        m_data->setStatusCond(requestStatus::cancelled, requestStatus::active);
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
        {
            return retval;
        }
    }
    else if (m_data->getStatus() == requestStatus::cancelled)       // extraction has been cancelled, but PDFDoc isn't closed yet, wait for it
    {
        m_data->waitForConsumer(CONSUMER_TIMEOUT);
    }

    const auto status{ m_data->getStatus() };
    if (!(  (status == requestStatus::cancelled)
        || ((status == requestStatus::closed)   && (unit > 0) && fiTextOrOutlines)  // extraction is closed but TC wants next text block
        || ((status == requestStatus::complete) && (unit > 0) && fiTextOrOutlines)  // extraction is completed but TC wants next text block
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
                m_data->setStatusCond(requestStatus::active, requestStatus::complete);
                if (startWorkerThread())
                {
                    result = waitForConsumer(PRODUCER_TIMEOUT);
                }
            }
            else if (result == ft_setsuccess)
            {
                result = waitForConsumer(PRODUCER_TIMEOUT);
            }

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
            m_data->setStatusCond(requestStatus::active, requestStatus::complete);
            if (startWorkerThread())
            {
                result = waitForConsumer(CONSUMER_TIMEOUT);
            }

            // if producer thread is slow, send empty field
            if (result == ft_timeout)
            {
                result = ft_fieldempty;
            }
            else if ((result == ft_setsuccess) && dst)
            {
                std::lock_guard lock(m_data->mutex);
                auto src{ m_data->getRequestBuffer() };
                result = m_data->getRequestResult();
                switch (result)
                {
                case ft_numeric_32:
                    [[fallthrough]];
                case ft_boolean:
                    memcpy(dst, src, sizeof(int32_t));
                    break;
                case ft_numeric_floating:
                {
                    memcpy(dst, src, sizeof(int64_t));
                    auto wSrc{ static_cast<wchar_t*>(src) + sizeof(int64_t) / sizeof(wchar_t) };
                    if (*wSrc)
                    {
                        dstSize &= ~1; // round to 2
                        StringCbCopyW(static_cast<wchar_t*>(dst) + sizeof(int64_t) / sizeof(wchar_t), dstSize - sizeof(int64_t), wSrc);
                        *wSrc = 0;
                    }
                    break;
                }
                case ft_datetime:
                    memcpy(dst, src, sizeof(int64_t));
                    break;
                case ft_stringw:
                    [[fallthrough]];
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
    {
        m_search->abort();
    }
}

/**
* Notify text extracting threads that the state of requests is changed.
* Threads should return back to idle point in #waitForProducer and close PdfDocs.
*/
void PDFExtractor::stop()
{
    // if extraction is active, mark it as cancelled
    m_data->stop();
    if (m_search)
    {
        m_search->stop();
    }
}

/**
* Notifiy text extracting threads that the state of requests has changed.
* Threads should return back to idle point in #waitForProducer without closing PdfDocs.
*/
void PDFExtractor::done()
{
    m_data->done();
    if (m_search)
    {
        m_search->done();
    }
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
    auto eqTxt{ false };

    // set timeout to long wait, because it waits for another extraction thread
    auto result{ initData(fileName1, field, 0, 0, CONSUMER_TIMEOUT) };
    if (result != ft_setsuccess)
    {
        return ft_compare_next;
    }

    if (!m_search)
    {
        m_search = std::make_unique<PDFExtractor>();
    }

    // set timeout to long wait, because it waits for another extraction thread to complete
    result = m_search->initData(fileName2, field, 0, 0, CONSUMER_TIMEOUT);
    if (result != ft_setsuccess)
    {
        return ft_compare_next;
    }

    // start threads
    if (startWorkerThread() && m_search->startWorkerThread())
    {
        // change status from complete to active
        m_data->setStatusCond(requestStatus::active, requestStatus::complete);
        m_search->m_data->setStatusCond(requestStatus::active, requestStatus::complete);

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
                {
                    // cast from void* to wchar_t*
                    auto start1{ static_cast<wchar_t*>(m_data->getRequestBuffer()) };
                    size_t len1{ 0 };
                    StringCchLengthW(start1, REQUEST_BUFFER_SIZE / sizeof(wchar_t), &len1);

                    auto start2{ static_cast<wchar_t*>(m_search->m_data->getRequestBuffer()) };
                    size_t len2{ 0 };
                    StringCchLengthW(start2, REQUEST_BUFFER_SIZE / sizeof(wchar_t), &len2);

                    // string len to compare
                    const auto minLen{ len1 < len2 ? len1 : len2 };

                    if (minLen)
                    {
                        // compare binary
                        if (!wmemcmp(start1, start2, minLen))
                        {
                            TRACE(L"%hs!binary!%Iu wchars equal\n", __FUNCTION__, minLen);
                            bytesProcessed += minLen;
                            result = ft_compare_eq;
                        }
                        else
                        {
                            // remove delimiters, spaces
                            const auto len1X{ removeDelimiters(start1, len1, delims) };
                            const auto len2X{ removeDelimiters(start2, len2, delims) };
                            // string len to compare
                            const auto minLenX{ len1X < len2X ? len1X : len2X };
                            if (minLenX)
                            {
                                // compare as text, case-insensitive, using locale specific information
                                if (!_wcsnicoll_l(start1, start2, minLenX, m_locale.get()))
                                {
                                    TRACE(L"%hs!text!%Iu wchars equal\n", __FUNCTION__, minLenX);
                                    bytesProcessed += minLenX;
                                    result = ft_compare_eq;
                                    eqTxt = true;
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
                                eqTxt = true;
                            }
                        }
                    }
                    else if (len1 == len2)
                    {
                        TRACE(L"%hs!no data\n", __FUNCTION__);
                        result = ft_compare_eq;
                        bytesProcessed = 0;
                    }

                    if ((result == ft_compare_eq) && minLen && ((len1 > minLen) || (len2 > minLen)))
                    {
                        // discard compared data
                        if (len1 >= minLen)
                        {
                            wmemmove(start1, start1 + minLen, len1 - minLen);
                        }

                        if (len2 >= minLen)
                        {
                            wmemmove(start2, start2 + minLen, len2 - minLen);
                        }

                        // part of a string was equal, compare rest
                        result = ft_compare_not_eq;
                    }

                    // adjust string end pointer and remaining buffer size
                    m_data->setRequestPtr(start1 + len1 - minLen);
                    m_search->m_data->setRequestPtr(start2 + len2 - minLen);
                }

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
        } while ((requestStatus::active == m_data->getStatus()) && (requestStatus::active == m_search->m_data->getStatus()));

        // if data was once compared as text, it is not binary equal
        if ((result == ft_compare_eq) && eqTxt)
        {
            result = ft_compare_eq_txt;
        }

        // don't close PDFDocs, they may be used again
        done();
    }
    else
    {
        TRACE(L"%hs!unable to start threads\n", __FUNCTION__);
    }
    return result;
}
