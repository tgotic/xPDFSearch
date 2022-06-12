#pragma once
#include "contentplug.h"
#include <tchar.h>

/**
* @file
* Declaration of enumerations used in extraction
*/

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
* The fieldIndexes enumeration is used simplify access to fields. 
*/
enum fieldIndexes
{
    fiTitle, fiSubject, fiKeywords, fiAuthor, fiCreator, fiProducer, fiDocStart, fiFirstRow,
    fiNumberOfPages, 
    fiPDFVersion, fiPageWidth, fiPageHeight,
    fiCopyingAllowed, fiPrintingAllowed, fiAddCommentsAllowed, fiChangingAllowed, fiEncrypted, fiTagged, fiLinearized, fiIncremental, fiSignature,
    fiCreationDate, fiLastModifiedDate, 
    fiID, fiAttributesString,
    fiText
};
/**< used to globally set the number of supported fields. */
constexpr auto FIELD_COUNT{ 26 };

#ifdef _DEBUG
extern bool __cdecl _trace(const wchar_t *format, ...);
#define TRACE _trace
#else
#ifdef __MINGW32__
#define __noop(...)
#endif
#define TRACE __noop
#endif
