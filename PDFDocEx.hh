#pragma once
#include <PDFDoc.h>
#include <Page.h>
#include <Zoox.h>
#include <memory>

class PDFDocEx : public PDFDoc
{
public:
    PDFDocEx(const wchar_t *fileNameA, size_t fileNameLen);
    bool hasSignature();
    bool hasOutlines();
    bool hasEmbeddedFiles();
    bool isIncremental();
    bool isTagged();
    int getAdbeExtensionLevel();
    int getNumFontlessPages();
    int getNumPagesWithImages();
    GString* getExtensions();
    GString* getMetadataString(const char* key);
    GString* getMetadataDateTime(const char* key);
    GString* getConformance();
    GString* getID();
    double getPDFVersion();

private:
    static bool getElemOrAttrData(const ZxElement* elem, const char* nodeName, GString& value, const char* prefix);
    static const char* findXmpPrefix(const ZxElement* elem, const char* nsURI);
    static bool pageContentIsEmpty(Page* page);
    GString* getXmpValue(const char* nsURI, const char* key, const char* arrayType);
    void getExtensionValues(Object* objExt, GString& data);
    bool openXMP();

    std::unique_ptr<ZxDoc> m_xmp{ nullptr };
    bool m_xmpChecked{ false };
};
