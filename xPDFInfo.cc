/**
* @file
*
* TotalCommander context plugin (wdx, wdx64) for PDF data extraction and comparision.
* Based on xPDF v4.06 from Glyph & Cog, LLC.
*/

#include "xPDFInfo.hh"
#include <wchar.h>
#include "PDFExtractor.hh"
#include <GlobalParams.h>
#include <strsafe.h>
#include <array>

/** enableDateTimeField is used to indicate if date time fields are supported by currently used Total Commander version. */
static auto enableDateTimeField{ false };

/** enableCompareFields is used to indicate if compare fields are supported by currently used Total Commander version. */
static auto enableCompareFields{ false };

/** Options from ini file, global */
options_t globalOptionsFromIni;

/**
* Names of fields returned to TC.
* Names are grouped by field types.
*/
static constexpr std::array fieldNames
{
    "Title", "Subject", "Keywords", "Author", "Application", "PDF Producer", "Document Start", "First Row", "Extensions",
    "Number Of Pages", "Number Of Fontless Pages", "Number Of Pages With Images",
    "PDF Version", "Page Width", "Page Height",
    "Copying Allowed", "Printing Allowed", "Adding Comments Allowed", "Changing Allowed", "Encrypted", "Tagged", "Linearized", "Incremental", "Signature Field", "Outlined", "Embedded Files", "Protected",
    "Created", "Modified", "Metadata Date",
    "ID", "PDF Attributes", "Conformance", "Encryption", "Created Raw", "Modified Raw", "Metadata Date Raw",
    "Outlines", "Text"
};
static_assert(fieldNames.size() == FIELD_COUNT, "fieldNames size error");

/** Array used to simplify fieldType returning. */
constexpr std::array fieldTypes
{
    ft_stringw, ft_stringw, ft_stringw, ft_stringw, ft_stringw, ft_stringw, ft_stringw, ft_stringw, ft_stringw,
    ft_numeric_32, ft_numeric_32, ft_numeric_32,
    ft_numeric_floating, ft_numeric_floating, ft_numeric_floating,
    ft_boolean, ft_boolean, ft_boolean, ft_boolean, ft_boolean, ft_boolean, ft_boolean, ft_boolean, ft_boolean, ft_boolean, ft_boolean, ft_boolean,
    ft_datetime, ft_datetime, ft_datetime,
    ft_stringw, ft_stringw, ft_stringw, ft_stringw, ft_stringw, ft_stringw, ft_stringw,
    ft_fulltext, ft_fulltext
};
static_assert(fieldTypes.size() == FIELD_COUNT, "fieldTypes size error");

/** Supported field flags, special value for attributes */
constexpr std::array fieldFlags
{
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,0,
    0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0,
    0, contflags_substattributestr, 0, 0, 0, 0, 0 ,0, 0,
    0, 0
};
static_assert(fieldFlags.size() == FIELD_COUNT, "fieldFlags size error");

/**< Only one instance of PDFExtractor per thread. */
#if defined(_MSC_VER) && (_MSC_VER < 1900)
__declspec(thread)
#else
thread_local
#endif
static PDFExtractor* g_extractor{ nullptr };

static HMODULE hModule{ nullptr };

#ifdef _DEBUG
/** Writes debug trace.
* Please note that output trace is limited to 1024 characters!
*
* @param[in] format format options
* @param[in] ...     optional arguments
*
*/
bool __cdecl _trace(const wchar_t *format, ...)
{
    wchar_t buffer[1024];
    wchar_t* end{ nullptr };
    size_t remaining{ 0 };
    SYSTEMTIME now;

    GetLocalTime(&now);
    StringCchPrintfExW( buffer, ARRAYSIZE(buffer)
                      , &end, &remaining
                      , STRSAFE_NULL_ON_FAILURE
                      , L"%.2u%.2u%.2u.%.3u!%.5lu!"
                      , now.wHour, now.wMinute, now.wSecond, now.wMilliseconds
                      , GetCurrentThreadId()
                      );

    va_list argptr;
    va_start(argptr, format);
    StringCchVPrintfW(end, remaining, format, argptr);
    va_end(argptr);

    OutputDebugStringW(buffer);

    return true;
}
#endif

/**
* Destroys PDFExtractor instance.
* Before destruction, abort() is called to exit threads.
* It may take some time to exit thread if text extraction is in progress.
*/
static void destroy()
{
    if (g_extractor)
    {
        TRACE(L"%hs\n", __FUNCTION__);
        g_extractor->abort();
        delete g_extractor;
        g_extractor = nullptr;
    }
}
 /**
 * DLL (wdx) entry point.
 * When TC needs service from this plugin for the first time,
 * DllMain is called with DLL_PROCESS_ATTACH value. 
 * It may be called from TC's main GUI thread, or from woker threads.
 * xPDF globalParam structure is initialized with default values.
 * xPDF settings can be changed by putting <b>xpdfrc</b> file to the directory where wdx is located.
 *
 * When plugin is unloaded or TC is closing, DLL_PROCESS_DETACH is called.
 */
BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD reason, LPVOID)
{
    TRACE(L"%hs!%lu\n", __FUNCTION__, reason);
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
    {
        // Initialize globally used resources.
        globalParams = new GlobalParams(nullptr);
        globalParams->setTextEncoding("UCS-2");         // extracted text encoding (not for metadata)
        globalParams->setTextPageBreaks(gFalse);        // don't add \f for page breaks
        globalParams->setTextEOL("unix");               // extracted text line endings
        hModule = static_cast<HMODULE>(hDLL);

        RegCloseKey(nullptr);   // load Advapi32.dll from system32 so GetModuleHandleW doesn't need full system path

        break;
    }
    case DLL_PROCESS_DETACH:
        destroy();              // Release PDFExtractor instance, if any
        TRACE(L"%hs!globalParams\n", __FUNCTION__);
        delete globalParams;    // Clean up
        globalParams = nullptr;
        hModule = nullptr;
        break;
    case DLL_THREAD_ATTACH:
        TRACE(L"%hs!new TC thread\n", __FUNCTION__);
        break;
    case DLL_THREAD_DETACH:
        destroy();              // Release PDFExtractor instance, if any. Don't clean up globalParams
        break;
    }
    return TRUE;
}

/**
* Returns PDF detection string.
* @param[out]    detectString    detection buffer
* @param[in]     maxlen          detection buffer size in chars
* @return 0     unused
*/
int __stdcall ContentGetDetectString(char* detectString, int maxlen)
{
    // PDF files are all we can handle.
    StringCchCopyA(detectString, maxlen, R"(EXT="PDF")");
    return 0;
}

/**
* See "Content Plugin Interface" document
* xpdfsearch supports indexes in ranges 0-25 and 10000-10025.
* Range above 10000 is reserved for directory synchronization functions.
*
* @param[in]    fieldIndex     index
* @param[out]   fieldName      name of requested fieldIndex
* @param[in]    units          "mm|cm|in|pt" for fieldIndex=fiPageWidth and fiPageHeight
* @param[in]    maxlen         size of Units and fieldName in chars
* @return type of requested field
*/
int __stdcall ContentGetSupportedField(int fieldIndex, char* fieldName, char* units, int maxlen)
{
    TRACE(L"%hs!index=%d\n", __FUNCTION__, fieldIndex);

    // clear units
    units[0] = 0;

    // set field names for compare indexes
    if ((fieldIndex >= ft_comparebaseindex) && (fieldIndex < ft_comparebaseindex + FIELD_COUNT))
    {
        if (enableCompareFields)
        {
            StringCchPrintfA(fieldName, maxlen, "Compare %s", fieldNames[fieldIndex - ft_comparebaseindex]);
            return ft_comparecontent;
        }
        return ft_nomorefields;
    }

    // Exclude date time fields in older TC versions.
    if ((fieldIndex < 0) || (fieldIndex >= FIELD_COUNT) ||
        (!enableDateTimeField && ((fieldIndex == fiCreationDate) || (fieldIndex == fiModifiedDate) || (fieldIndex == fiMetadataDate))))
    {
        return ft_nomorefields;
    }

    // set field names
    StringCchCopyA(fieldName, maxlen, fieldNames[fieldIndex]);

    //set units names
    if ((fieldIndex == fiPageWidth) || (fieldIndex == fiPageHeight))
    {
        StringCchCopyA(units, maxlen, "mm|cm|in|pt");
    }

    return fieldTypes[fieldIndex];
}

/**
* Plugin state change.
* See "Content Plugin Interface" document.
* When TC reads new directory or re-reads current one, close current PDF.
*
* @param[in]    state   state value.
* @param[in]    path    current path
*/
void __stdcall ContentSendStateInformationW(int state, const wchar_t* path)
{
    TRACE(L"%hs!%d\n", __FUNCTION__, state);
   switch (state)
   {
   case contst_readnewdir:
       if (g_extractor)
       {
           g_extractor->stop();
       }
       break;
   default:
       break;
   }
}

/**
* ANSI version of ContentGetValue is not supported
*/
int __stdcall ContentGetValue(const char* fileName, int fieldIndex, int unitIndex, void* fieldValue, int cbfieldValue, int flags)
{
    TRACE(L"%hs\n", __FUNCTION__);
    return ft_notsupported;
}

/**
* Retrieves the value of a specific field for a given PDF document.
* See "Content Plugin Interface" document.
* Creates PDFExtractor object, if not already created, calls extraction function.
* If fieldIndex is out of bounds, current PDF document is closed.
*
* @param[in]    fileName        full path to PDF document
* @param[in]    fieldIndex      index of the field
* @param[in]    unitIndex       index of the unit,  -1 for ft_fulltext type fields when searched string is found
* @param[out]   fieldValue      buffer for retrieved data
* @param[in]    cbfieldValue    sizeof buffer in bytes
* @param[in]    flags           TC flags
* @return       result of an extraction
*/
int __stdcall ContentGetValueW(const wchar_t* fileName, int fieldIndex, int unitIndex, void* fieldValue, int cbfieldValue, int flags)
{
    TRACE(L"%hs!%ls!%d %d\n", __FUNCTION__, fileName, fieldIndex, unitIndex);

    if ((fieldIndex >= fiTitle) && (fieldIndex <= fiText))
    {
        if (CONTENT_DELAYIFSLOW & flags)
        {
            return ft_delayed;
        }

        if (!g_extractor)
        {
            g_extractor = new PDFExtractor();
            TRACE(L"%hs!new extractor\n", __FUNCTION__);
        }

        if (g_extractor)
        {
            return g_extractor->extract(fileName, fieldIndex, unitIndex, fieldValue, cbfieldValue, flags);
        }
        TRACE(L"%hs!unable to create extractor\n", __FUNCTION__);
        return ft_fileerror;
    }
    else if (g_extractor)
    {
        g_extractor->stop();
    }

    return ft_nomorefields;
}

/**
* Get ini file name.
* Search for ini file in wdx directory. Ini file name is equal to wdx file name.
* If this ini file is not found, use ini file from ContentDefaultParamStruct.
*
* param[in]     defaultIniName  default ini from ContentDefaultParamStruct
* param[out]    iniFileName     ini file name with path
*/
static void getIniFileName(const char* defaultIniName, char* iniFileName)
{
    bool useDefaultIni{ true };
    
    if (GetModuleFileNameA(hModule, iniFileName, MAX_PATH - 1)) // play safe with Windows XP:  The string is truncated to nSize characters and is not null-terminated.
    {
        auto dot{ strrchr(iniFileName, '.') };
        if (dot)
        {
            *dot = 0;
            if (SUCCEEDED(StringCbCatA(iniFileName, MAX_PATH, ".ini")))
            {
                auto attr{ GetFileAttributesA(iniFileName) };   // check if file exists
                if ((attr & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY)
                {
                    useDefaultIni = false;
                }
            }
        }
    }
    if (useDefaultIni)
    {
        StringCbCopyA(iniFileName, MAX_PATH, defaultIniName);
    }
}

/**
* Check for version of currently used Total Commander / version of plugin interface.
* If plugin interface is lower than 1.2, PDF date and time fields are not supported.
* If plugin interface is lower than 2.1, compare by content fields are not supported.
* Load options from TC content plugin ini file.
*
* @param[in]    dps see ContentDefaultParamStruct in "Content Plugin Interface" document.
*/
void __stdcall ContentSetDefaultParams(ContentDefaultParamStruct* dps)
{
    TRACE(L"%hs\n", __FUNCTION__);
    constexpr auto appName{ "xPDFSearch" };
    // Check content plugin interface version to enable fields of type datetime.
    enableDateTimeField = ((dps->pluginInterfaceVersionHi == 1) && (dps->pluginInterfaceVersionLow >= 2)) || (dps->pluginInterfaceVersionHi > 1);
    enableCompareFields = ((dps->pluginInterfaceVersionHi == 2) && (dps->pluginInterfaceVersionLow >= 10)) || (dps->pluginInterfaceVersionHi > 2);

    char iniFileName[MAX_PATH]{};
    getIniFileName(dps->defaultIniName, iniFileName);

    globalOptionsFromIni.noCache = GetPrivateProfileIntA(appName, "NoCache", 0, iniFileName);
    globalOptionsFromIni.discardInvisibleText = GetPrivateProfileIntA(appName, "DiscardInvisibleText", 1, iniFileName);
    globalOptionsFromIni.discardDiagonalText = GetPrivateProfileIntA(appName, "DiscardDiagonalText", 1, iniFileName);
    globalOptionsFromIni.discardClippedText = GetPrivateProfileIntA(appName, "DiscardClippedText", 1, iniFileName);
    globalOptionsFromIni.appendExtensionLevel = GetPrivateProfileIntA(appName, "AppendExtensionLevel", 1, iniFileName);
    globalOptionsFromIni.removeDateRawDColon = GetPrivateProfileIntA(appName, "RemoveDateRawDColon", 0, iniFileName);
    globalOptionsFromIni.marginLeft = GetPrivateProfileIntA(appName, "MarginLeft", 0, iniFileName);
    globalOptionsFromIni.marginRight = GetPrivateProfileIntA(appName, "MarginRight", 0, iniFileName);
    globalOptionsFromIni.marginTop = GetPrivateProfileIntA(appName, "MarginTop", 0, iniFileName);
    globalOptionsFromIni.marginBottom = GetPrivateProfileIntA(appName, "MarginBottom", 0, iniFileName);
    globalOptionsFromIni.pageContentsLengthMin = GetPrivateProfileIntA(appName, "PageContentsLengthMin", 32, iniFileName);
    globalOptionsFromIni.textOutputMode = static_cast<TextOutputMode>(GetPrivateProfileIntA(appName, "TextOutputMode", 0, iniFileName) % (textOutRawOrder + 1));

    char tmp[2];
    if (GetPrivateProfileStringA(appName, "AttrCopyingAllowed", "C", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrCopyable, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrPrintingAllowed", "P", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrPrintable, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrAddingCommentsAllowed", "N", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrCommentable, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrChangingAllowed", "M", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrChangeable, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrIncremental", "I", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrIncremental, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrTagged", "T", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrTagged, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrLinearized", "L", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrLinearized, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrEncrypted", "E", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrEncrypted, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrSignatureField", "S", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrSigned, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrOutlined", "O", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrOutlined, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrEmbeddedFiles", "F", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrEmbeddedFiles, tmp, 1);
    if (GetPrivateProfileStringA(appName, "AttrProtected", "X", tmp, sizeof(tmp), iniFileName) == 1)
        mbtowc(&globalOptionsFromIni.attrProtected, tmp, 1);
}

/**
* Plugin is being unloaded. Close extraction thread.
* This function is called only form main GUI thread.
* Don't free globalParams form here, because other worker threads
* may be using it.
*/
void __stdcall ContentPluginUnloading()
{
    TRACE(L"%hs\n", __FUNCTION__);
    if (g_extractor)
    {
        g_extractor->abort();
    }
}

/**
* Directory change has occurred, stop extraction.
* See "Content Plugin Interface" document.
*
* @param[in]  fileName      not used
*/
void __stdcall ContentStopGetValueW(const wchar_t* fileName)
{
    TRACE(L"%hs\n", __FUNCTION__);
    if (g_extractor)
    {
        g_extractor->stop();
    }
}
/**
* ContentGetSupportedFieldFlags is called to get various information about a plugin variable.
* See "Content Plugin Interface" document.
* Only "PDF Attributes" fields has non-default flag.
* 
* @param[in]    fieldIndex  index of the field for which flags should be returned
* @return flags for selected field
*/
int __stdcall ContentGetSupportedFieldFlags(int fieldIndex)
{
    if (fieldIndex == -1)
    {
        return contflags_substmask;
    }

    if ((fieldIndex >= fiTitle) && (fieldIndex <= fiText))
    {
        return fieldFlags[fieldIndex];
    }

    return 0;
}
/**
* ContentCompareFiles is called in Synchronize dirs to compare two files by content.
* xPDFsearch provides option to compare content of all exposed fields.
* Content of field for each file is extracted in it's own thread.
* 
* @param[in] progressCallback   callback function to inform the calling program about the compare progress
* @param[in] compareIndex       field to compare, staring with 10000
* @param[in] fileName1          name of first file to be compared
* @param[in] fileName2          name of second file to be compated
* @param[in] fileDetails        not used
* @return result of comparison. For values see "Content Plugin Interface" document.
*/
int __stdcall ContentCompareFilesW(PROGRESSCALLBACKPROC progressCallback, int compareIndex, wchar_t* fileName1, wchar_t* fileName2, FileDetailsStruct* fileDetails)
{
    TRACE(L"%hs\n", __FUNCTION__);
    if ((compareIndex < ft_comparebaseindex) || (compareIndex >= ft_comparebaseindex + FIELD_COUNT))
    {
        return ft_compare_next;
    }

    if (!g_extractor)
    {
        g_extractor = new PDFExtractor();
        TRACE(L"%hs!new extractor\n", __FUNCTION__);
    }

    if (g_extractor)
    {
        return g_extractor->compare(progressCallback, fileName1, fileName2, compareIndex - ft_comparebaseindex);
    }

    TRACE(L"%hs!unable to create extractor\n", __FUNCTION__);
    return ft_compare_next;
}
