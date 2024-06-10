# Version 1.42

ADDED
* New fields: Number Of Fontless Pages, Number Of Pages With Images
* Options in content plugin ini file:
    * \[xPDFSearch\] PageContentsLengthMin

CHANGED
* Bugfix for crash in text search

# Version 1.41

CHANGED
* Xpdf updated to version 4.05.
* minor code and documentation changes

# Version 1.40

ADDED
* New fields: Extensions, Outlined, Embedded Files, Metadata Date, Metadata Date Raw
* Options in content plugin ini file:
    * \[xPDFSearch\] AttrCopyingAllowed
    * \[xPDFSearch\] AttrPrintingAllowed
    * \[xPDFSearch\] AttrAddingCommentsAllowed
    * \[xPDFSearch\] AttrChangeable
    * \[xPDFSearch\] AttrEncrypted
    * \[xPDFSearch\] AttrTagged
    * \[xPDFSearch\] AttrLinearized
    * \[xPDFSearch\] AttrIncremental
    * \[xPDFSearch\] AttrSignatureField
    * \[xPDFSearch\] AttrOutlined
    * \[xPDFSearch\] AttrEmbeddedFiles

CHANGED
* Improved support for PDF 2.0 documents
* Updated Conformance for PDF/UA and PDF/R, multiple conformances separated by semicolon
* Updated Croatian translation
* Renamed fields:
    * CreatedRaw -> Created Raw
    * ModifiedRaw -> Modified Raw

# Version 1.38

ADDED
* New fields: CreatedRaw and ModifiedRaw (date strings from PDF)
* Options in content plugin ini file:
    * \[xPDFSearch\] RemoveDateRawDColon - remove D: from date string

CHANGED
* Relaxed date year in Created and Modified fields
* Updated Danish and Croatian translation

# Version 1.37

ADDED
* Load options from xPDFSearch.ini file in directory where plugin is located.
If xPDFSearch.ini is not found, load options from TC content plugin ini file.
* \[xPDFSearch\] AppendExtensionLevel - find PDF Extension Level and append it to PDF Version:  
PDF 1.7 Extension Level 3 = 1.73
* New field: Conformance - PDF/A, PDF/E, PDF/X

CHANGED
* Improved PDF Date parsing, allow dates that don't start with D:

# Version 1.36

ADDED
* Options in content plugin ini file:
    * \[xPDFSearch\] NoCache - disable file caching
        - allow renaming PDF file with values form xPDFSearch
        - allow changing file attributes (not content of PDF file)
    * \[xPDFSearch\] DiscardInvisibleText - discard all invisible characters
    * \[xPDFSearch\] DiscardDiagonalText - discard all text that's not close to 0/90/180/270 degrees
    * \[xPDFSearch\] DiscardClippedText - discard all clipped characters
    * \[xPDFSearch\] MarginLeft - discard all characters left of mediaBox + marginLeft
    * \[xPDFSearch\] MarginRight - discard all characters right of mediaBox - marginRight
    * \[xPDFSearch\] MarginTop - discard all characters above of mediaBox - marginTop
    * \[xPDFSearch\] MarginBottom  - discard all characters bellow of mediaBox + marginBottom
    * \[xPDFSearch\] TextOutputMode - text formatting mode, see TextOutputControl in TextOutputDev.h
* New field: Outline - search outlines (bookmarks) titles
* New attribute, O - PDF has outlines (bookmarks) directory

CHANGED
* Open PDF file with write deny share

# Version 1.35

CHANGED
* Xpdf updated to version 4.04.
* Open PDF file without write deny, allow change of file attributes

# Version 1.34

CHANGED
* Xpdf updated to version 4.03.
* use some of c++17 features

# Version 1.32

CHANGED
* Xpdf updated to version 4.02.
* Extraction moved to ThreadData.cc.

# Version 1.31

FIXED
* Values for binary fields were wrong in 1.3
* Plugin didn't return correct values if units index was not 0

# Version 1.30

ADDED
* Synchronize Directories, plugin can be used to compare content of PDF metadata and text

FIXED
* PaperSize did not return proper value for "points" units
* Signature flag updated to value 3
* 32-bit version

# Version 1.20

ADDED
* New fields: Incremental, Signature Field, ID and PDF Attributes.
* Data caching - faster retirieval of fields - PDF document stays open while reading fields
* Improved exit from PDF document when search string is found

CHANGED
* Document Start field moved to position 7
* First row field moved to position 8
* Xpdf updated to version 4.01.

# Version 1.11

FIXED
* Fixed a bug that could lead to not finding all search strings.

# Version 1.10

ADDED
* Added support for Unicode file names.
* Added Croatian translation.

CHANGED
* Xpdf version updated to version 3.04.


# Version 1.07

ADDED
* 64 bit support.

# Version 1.06

ADDED
* Linearized field.

CHANGED
* Xpdf version updated to version 3.02pl5.
* Xpdf version updated to version 3.02pl4.

# Version 1.05

ADDED
* Support for Unicode metadata. Full text search and Unicode file names are not supported.

CHANGED
* Xpdf version updated to version 3.02pl3.

# Version 1.04

ADDED
* Russian translation

CHANGED
* Xpdf version updated to version 3.02pl2.

# Version 1.03

ADDED
* New fields "Page Width" and "Page Height".
* New fields Document Start, First Line, Copying Allowed, Printing Allowed, Adding Comment Allowed, Changing Allowed, Encrypted, Tagged.
* Spanish translation
* Dutch translation.

CHANGED
* Xpdf version updated to version 3.02pl1.
* Xpdf version updated to version 3.02.

FIXED
* Moved the Text field to the bottom of the field list to avoid trouble.

# Version 1.02

CHANGED
* Xpdf version updated to version 3.01.

FIXED
* For the fields Created and Modified random values were displayed, although these information are not contained in the document. Now these fields display an empty field.
* For the fields PDF Version and Number of Pages random values were displayed if certain other metadata fields don't exist in the document. Now these fields are displayed correctly.
* Metadata string values were not displayed correctly.

# Version 1.01

ADDED
* New fields PDF Version, Created and Modified.
* Russian and Danish translation.

CHANGED
* Optimized speed and size.

FIXED
* Fixed various resource leaks.

# Version 1.00

First release.
