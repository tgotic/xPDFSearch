/**
* @file
*
* Declaration of enumerations used in extraction
*/

#pragma once
#include "contentplug.h"
#include <TextOutputDev.h>

/**
* PDF page units
*/
enum SizeUnits
{
    suMilliMeters,  /**< millimeters */
    suCentiMeters,  /**< centimeters */
    suInches,       /**< inches */
    suPoints        /**< points */
};

/**
* Options from ini file.
*/
typedef struct options_s
{
    bool noCache{ false };              /**< Don't cache data for this ContentGetValue call, close file after returning data */
    bool discardInvisibleText{ true };  /**< discard all invisible characters */
    bool discardDiagonalText{ true };   /**< discard all text that's not close to 0/90/180/270 degrees */
    bool discardClippedText{ true };    /**< discard all clipped characters */
    bool appendExtensionLevel{ true };  /**< append PDF Extension Level to PDF version, e.g. 1.7 extension level 3 = 1.73 */
    bool removeDateRawDColon{ false };  /**< remove D: from DateRaw string */
    TextOutputMode textOutputMode{ textOutReadingOrder }; /**< text formatting mode, see TextOutputControl in TextOutputDev.h */
    int marginLeft{ 0 };                /**< discard all characters left of mediaBox + marginLeft */
    int marginRight{ 0 };               /**< discard all characters right of mediaBox - marginRight */
    int marginTop{ 0 };                 /**< discard all characters above of mediaBox - marginTop */
    int marginBottom{ 0 };              /**< discard all characters bellow of mediaBox + marginBottom */
    int pageContentsLengthMin{ 32 };    /**< minimal length of page Contents stream so page is not considered empty. Used for "Number of Fontless pages"  and "Number of pages with images"  fields */
    wchar_t attrCopyable{ L'\0' };
    wchar_t attrPrintable{ L'\0' };
    wchar_t attrCommentable{ L'\0' };
    wchar_t attrChangeable{ L'\0' };
    wchar_t attrEncrypted{ L'\0' };
    wchar_t attrTagged{ L'\0' };
    wchar_t attrLinearized{ L'\0' };
    wchar_t attrIncremental{ L'\0' };
    wchar_t attrSigned{ L'\0' };
    wchar_t attrOutlined{ L'\0' };
    wchar_t attrEmbeddedFiles{ L'\0' };
    wchar_t attrProtected{ L'\0' };
} options_t;

extern options_t globalOptionsFromIni;

/**
* The fieldIndexes enumeration is used simplify access to fields.
*/
enum fieldIndexes
{
    fiTitle, fiSubject, fiKeywords, fiAuthor, fiCreator, fiProducer, fiDocStart, fiFirstRow, fiExtensions,
    fiNumberOfPages, fiNumberOfFontlessPages,fiNumberOfPagesWithImages,
    fiPDFVersion, fiPageWidth, fiPageHeight,
    fiCopyable, fiPrintable, fiCommentable, fiChangeable, fiEncrypted, fiTagged, fiLinearized, fiIncremental, fiSigned, fiOutlined, fiEmbeddedFiles, fiProtected,
    fiCreationDate, fiModifiedDate, fiMetadataDate,
    fiID, fiAttributesString, fiConformance, fiCreationDateRaw, fiModifiedDateRaw, fiMetadataDateRaw,
    fiOutlines, fiText
};

/**< used to globally set the number of supported fields. */
constexpr size_t FIELD_COUNT{ static_cast<size_t>(fiText + 1) };

#ifdef _DEBUG
extern bool __cdecl _trace(const wchar_t *format, ...);
#define TRACE _trace
#else
#ifdef __MINGW32__
#define __noop(...)
#endif
#define TRACE __noop
#endif
