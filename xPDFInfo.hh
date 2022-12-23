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
    bool appendExtensionLevel{ true };   /**< append PDF Extension Level to PDF version, e.g. 1.7 extension level 3 = 1.73 */
    TextOutputMode textOutputMode{ textOutReadingOrder }; /**< text formatting mode, see TextOutputControl in TextOutputDev.h */
    int marginLeft{ 0 };                /**< discard all characters left of mediaBox + marginLeft */
    int marginRight{ 0 };               /**< discard all characters right of mediaBox - marginRight */
    int marginTop{ 0 };                 /**< discard all characters above of mediaBox - marginTop */
    int marginBottom{ 0 };              /**< discard all characters bellow of mediaBox + marginBottom */
} options_t;

extern options_t globalOptionsFromIni;

/**
* The fieldIndexes enumeration is used simplify access to fields.
*/
enum fieldIndexes
{
    fiTitle, fiSubject, fiKeywords, fiAuthor, fiCreator, fiProducer, fiDocStart, fiFirstRow,
    fiNumberOfPages, 
    fiPDFVersion, fiPageWidth, fiPageHeight,
    fiCopyingAllowed, fiPrintingAllowed, fiAddCommentsAllowed, fiChangingAllowed, fiEncrypted, fiTagged, fiLinearized, fiIncremental, fiSignature,
    fiCreationDate, fiLastModifiedDate, 
    fiID, fiAttributesString, fiConformance,
    fiOutlines, fiText
};

/**< used to globally set the number of supported fields. */
constexpr auto FIELD_COUNT{ 28 };

#ifdef _DEBUG
extern bool __cdecl _trace(const wchar_t *format, ...);
#define TRACE _trace
#else
#ifdef __MINGW32__
#define __noop(...)
#endif
#define TRACE __noop
#endif
