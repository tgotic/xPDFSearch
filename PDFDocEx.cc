#include "PDFDocEx.hh"
#include <Zoox.h>
#include <Outline.h>
#include <TextString.h>
#include "xPDFInfo.hh"

/**
* Constructor
* 
* @param fileNameA      PDF file name to open
* @param fileNameLen    PDF file name length
*/
PDFDocEx::PDFDocEx(const wchar_t *fileNameA, size_t fileNameLen)
: PDFDoc(fileNameA, fileNameLen) 
{
}

/**
* PDF document has signature fields.
* It is not verified if document is signed or if signature is valid.
*
* @return true if SigFlags value > 0
*/
bool PDFDocEx::hasSignature()
{
    bool ret{ false };
    const auto cat{ getCatalog() };
    if (cat)
    {
        const auto acroForm{ cat->getAcroForm() };
        if (acroForm && acroForm->isDict())
        {
            Object obj;
            if (acroForm->dictLookup("SigFlags", &obj)->isInt())
            {
                // verify only bit position 1; 
                // bit position 2 marks that signature will be invalidated
                // if document is not saved as incremental
                ret = static_cast<bool>(obj.getInt() & 0x01);
            }
            obj.free();
        }
    }
    return ret;
}

/**
* PDF document has outlines (bookmarks).
* It is not verified if document outline has titles (labels).
*
* @return true if document has outlines
*/
bool PDFDocEx::hasOutlines()
{
    const auto outl{ getOutline() };
    if (outl && outl->getItems())
    {
        return true;
    }
    return false;
}

/**
* PDF document has embedded files in catalog.
* It is not verified if document has embedded files in annotations.
* This would require crawling all pages and checking annotations.
*
* @return true if document has embedded files
*/
bool PDFDocEx::hasEmbeddedFiles()
{
    bool ret{ false };
    const auto cat{ getCatalog() };
    if (cat)
    {
        Object objNames;
        const auto catObj{ cat->getCatalogObj() };
        if (catObj->isDict() && catObj->dictLookup("Names", &objNames)->isDict())
        {
            Object objEF;
            if (objNames.dictLookup("EmbeddedFiles", &objEF)->isDict())
            {
                ret = true;
            }
            objEF.free();
        }
        objNames.free();
    }
    return ret;
}

/**
* PDF document was updated incrementally without rewriting the entire file.
*
* @return true if PDF is incremental
*/
bool PDFDocEx::isIncremental()
{
    return (getXRef()->getNumXRefTables() > 1) ? true : false;
}

/**
* From Portable document format - Part 1: PDF 1.7
* 14.8 Tagged PDF
* "Tagged PDF (PDF 1.4) is a stylized use of PDF that builds on the logical structure framework described in 14.7, "Logical Structure""
*
* @return true if PDF is tagged
*/
bool PDFDocEx::isTagged()
{
    return getStructTreeRoot()->isDict() ? true : false;
}

/**
* Get document info from Document Info Directory.
* If there is no Document Info Directory (deprecated in PDF2.0),
* get info from XMP metadata.
* 
* @param[in]    key     one of keys from #DocInfoFields
* 
* @return pointer to allocated GString, needs to be deleted/released after usage.
*/
GString* PDFDocEx::getMetadataString(const char* key)
{
    std::unique_ptr<GString> ret{ nullptr };
    Object objDocInfo;
    if (getDocInfo(&objDocInfo)->isDict())
    {
        Object obj;
        if (objDocInfo.dictLookup(key, &obj)->isString())
        {
            // make a copy, ret is invalid after obj.free()
            ret.reset(obj.getString()->copy());
        }
        obj.free();
    }
    objDocInfo.free();

    if (!ret || (ret->getLength() == 0))
    {
        // in PDF2.0 Document information dictionary is deprecated
        // and document info is stored in XMP metadata
        if (!strcmp(key, "Title"))
        {
            ret.reset(getXmpValue(R"(http://purl.org/dc/elements/1.1/)", "title", "rdf:Alt"));
        }
        else if (!strcmp(key, "Subject"))
        {
            ret.reset(getXmpValue(R"(http://purl.org/dc/elements/1.1/)", "description", "rdf:Alt"));
        }
        else if (!strcmp(key, "Keywords"))
        {
            ret.reset(getXmpValue(R"(http://ns.adobe.com/pdf/1.3/)", "Keywords", nullptr));
        }
        else if (!strcmp(key, "Author"))
        {
            ret.reset(getXmpValue(R"(http://purl.org/dc/elements/1.1/)", "creator", "rdf:Seq"));
        }
        else if (!strcmp(key, "Creator"))
        {
            ret.reset(getXmpValue(R"(http://ns.adobe.com/xap/1.0/)", "CreatorTool", nullptr));
        }
        else if (!strcmp(key, "Producer"))
        {
            ret.reset(getXmpValue(R"(http://ns.adobe.com/pdf/1.3/)", "Producer", nullptr));
        }
        else if (!strcmp(key, "CreationDate"))
        {
            ret.reset(getXmpValue(R"(http://ns.adobe.com/xap/1.0/)", "CreateDate", nullptr));
        }
        else if (!strcmp(key, "ModDate"))
        {
            ret.reset(getXmpValue(R"(http://ns.adobe.com/xap/1.0/)", "ModifyDate", nullptr));
        }
        else if (!strcmp(key, "MetadataDate"))
        {
            ret.reset(getXmpValue( R"(http://ns.adobe.com/xap/1.0/)", "MetadataDate", nullptr));
        }
    }
    return ret.release();
}

/**
* PDF date time string from Document Info Directory.
* If there is no DID, data is fetched from XMP metadata
*
* @param[in]    key     "CreationDate", "ModDate" or "MetadataDate"
* @return pointer to allocated GString, needs to be deleted/released after usage.
*/
GString* PDFDocEx::getMetadataDateTime(const char* key)
{
    std::unique_ptr<GString> ret{ getMetadataString(key) };
    if (ret)
    {
        return TextString(ret.get()).toUTF8();
    }
    return nullptr;
}

/**
* PDF extension level, valid for PDF 1.7 or highrer.
*
* @return extension level, -1 if error
*/
int PDFDocEx::getAdbeExtensionLevel()
{
    int ret{ -1 };
    const auto cat{ getCatalog() };
    if (cat)
    {
        Object objExts;
        const auto catObj{ cat->getCatalogObj() };
        if (catObj->isDict() && catObj->dictLookup("Extensions", &objExts)->isDict())
        {
            Object objAdbe;
            if (objExts.dictLookup("ADBE", &objAdbe)->isDict())
            {
                Object objExtLevel;
                if (objAdbe.dictLookup("ExtensionLevel", &objExtLevel)->isInt())
                {
                    ret = objExtLevel.getInt();
                }
                objExtLevel.free();
            }
            objAdbe.free();
        }
        objExts.free();
    }
    return ret;
}

/**
* Get data from Extension directory: BaseVersion, ExtensionLevel and ExtensionRevision.
* BaseVersion is in format X.Y, ExtensionLevel is integer and ExtensionRevision is string.
* 
* @param[in]    objExt  extension directory object
* @param[out]   data    output data in format BaseVersion.ExtensionLevel.ExtensionRevision
*/
void PDFDocEx::getExtensionValues(Object* objExt, GString& data)
{
    Object obj;
    if (objExt->dictLookup("BaseVersion", &obj)->isName())
    {
        data.append(' ')->append(obj.getName());
    }
    obj.free();
    if (objExt->dictLookup("ExtensionLevel", &obj)->isInt())
    {
        std::unique_ptr<GString> extLevel{ GString::fromInt(obj.getInt()) };
        data.append('.')->append(extLevel.get());
    }
    obj.free();
    if (objExt->dictLookup("ExtensionRevision", &obj)->isString())
    {
        data.append('.')->append(obj.getString());
    }
    obj.free();
}

/**
* PDF extensions data.
* Output format:
* PREFIX BaseVersion.ExtensionLevel.ExtensionRevision;PREFIX BaseVersion.ExtensionLevel.ExtensionRevision
*
* @return pointer to allocated GString, needs to be deleted/released after usage.
*/
GString* PDFDocEx::getExtensions()
{
    auto ret{ std::make_unique<GString>() };
    const auto cat{ getCatalog() };
    if (cat)
    {
        Object objExts;
        const auto catObj{ cat->getCatalogObj() };
        if (catObj->isDict() && catObj->dictLookup("Extensions", &objExts)->isDict())
        {
            const auto nExts{ objExts.dictGetLength() };
            for (int n{ 0 }; n < nExts; n++)
            {
                auto key{ objExts.dictGetKey(n) };
                Object objExt;
                objExts.dictGetVal(n, &objExt);
                auto createRetOrAppend = [&]()
                {
                    if (ret->getLength())
                    {
                        // append delimiter for extensions
                        ret->append(';');
                    }
                };
                if (objExt.isDict())
                {
                    createRetOrAppend();
                    ret->append(key);
                    getExtensionValues(&objExt, *ret);
                }
                else if (objExt.isArray())
                {
                    createRetOrAppend();
                    for (int i{ 0 }; i < objExt.arrayGetLength(); i++)
                    {
                        Object objArr;
                        if (i)
                        {
                            ret->append(';');
                        }
                        if (objExt.arrayGet(i, &objArr)->isDict())
                        {
                            ret->append(key);
                            getExtensionValues(&objArr, *ret);
                        }
                        objArr.free();
                    }
                }
                objExt.free();
            }
        }
        objExts.free();
    }
    return ret.release();
}

/**
* PDF version.
* Compare PDF file version and Version in the Catalog and return higher version.
*
* @return PDF version
*/
double PDFDocEx::getPDFVersion()
{
    auto ver{ PDFDoc::getPDFVersion() };
    const auto cat{ getCatalog() };
    if (cat)
    {
        Object objVer;
        const auto catObj{ cat->getCatalogObj() };
        if (catObj->isDict() && catObj->dictLookup("Version", &objVer)->isName())
        {
            auto pdfVer{ strtod(objVer.getName(), nullptr) };
            if (pdfVer > ver)
            {
                ver = pdfVer;
            }
        }
        objVer.free();
    }
    return ver;
}

/**
* PDF ID.
* Get PDF ID (MD5) and convert to readable format.
*
* @return pointer to allocated GString, needs to be deleted/released after usage.
*/
GString* PDFDocEx::getID()
{
    Object objID;
    std::unique_ptr<GString> id{ nullptr };

    getXRef()->getTrailerDict()->dictLookup("ID", &objID);
    if (objID.isArray())
    {
        id = std::make_unique<GString>();
        // convert byte arrays to human readable strings
        for (int i{ 0 }; i < objID.arrayGetLength(); i++)
        {
            Object objIdArr;
            if (objID.arrayGet(i, &objIdArr)->isString())
            {
                const auto strIdArr{ objIdArr.getString() };
                if (i)
                {
                    id->append('-');
                }

                for (int j{ 0 }; j < strIdArr->getLength(); j++)
                {
                    int c{ strIdArr->getChar(j) & 0xFF };
                    id->appendf("{0:02ux}", c);
                }
            }
            objIdArr.free();
        }
    }
    objID.free();

    return id.release();
}

/**
* Open PDF XMP metadata.
* Parsed XMP metadata is stored in #m_xmp.
* 
* @return true - xmp is ready to be consumed, false - no XMP metadata or error
*/
bool PDFDocEx::openXMP()
{
    if (m_xmp)
        return true;

    if (!m_xmpChecked)
    {
        m_xmpChecked = true;
        const auto metadata{ std::make_unique<GString>(readMetadata()) };
        if (metadata && metadata->getLength())
        {
            m_xmp.reset(ZxDoc::loadMem(metadata->getCString(), metadata->getLength()));
            if (m_xmp)
            {
                return true;
            }
        }
    }
    return false;
}

/**
* Search for XMP element attribute or child element.
*
* @param[in]    elem            pointer to XMP element
* @param[in]    entry        attribute or child element name
* @param[out]   value           append attribute value or child element value to this parameter
* @param[in]    prefix          prefix to append before value
* 
* @return true - attribute or child element found, false - not found
*/
bool PDFDocEx::getElemOrAttrData(const ZxElement* elem, const char* entry, GString& value, const char* prefix)
{
    const auto attr{ elem->findAttr(entry) };
    if (attr)
    {
        value.append(prefix)->append(attr->getValue());
        return true;
    }
    const auto child{ elem->findFirstChildElement(entry) };
    if (child)
    {
        const auto node{ child->getFirstChild() };
        if (node && node->isCharData())
        {
            auto data{ static_cast<ZxCharData*>(node)->getData() };
            if (data)
            {
                // trim spaces to check if this is really element data or just indendation
                auto ptr{ data->getCString() };
                auto endptr{ ptr + data->getLength() };
                while (ptr < endptr)
                {
                    if (isspace(*ptr))
                        ptr++;
                    else
                        break;
                }
                if (ptr < endptr)
                {
                    value.append(prefix)->append(ptr);
                    return true;
                }
            }
        }
    }
    return false;
}

/**
* Get PDF conformance value from XMP metadata, or from PDF file (PDF/R)
*
* @return pointer to allocated GString, needs to be deleted/released after usage.
*/
GString* PDFDocEx::getConformance()
{
    auto conformance{ std::make_unique<GString>() };
    if (openXMP())
    {
        auto root{ m_xmp->getRoot() };
        if (root->isElement("x:xmpmeta"))
        {
            root = root->findFirstChildElement("rdf:RDF");
        }
        if (root && root->isElement("rdf:RDF"))
        {
            for (auto node{ root->getFirstChild() }; node; node = node->getNextChild())
            {
                if (node->isElement("rdf:Description"))
                {
                    GString nodeName;
                    const auto elem{ static_cast<ZxElement*>(node) };
                    auto ns{ findXmpPrefix(elem, R"(http://www.aiim.org/pdfa/ns/id/)") };
                    if (ns) // PDF/A
                    {
                        nodeName.append(ns)->append(":")->append("part");
                        getElemOrAttrData(elem, nodeName.getCString(), *conformance, "PDF/A-");
                        nodeName.clear()->append(ns)->append(":")->append("conformance");
                        getElemOrAttrData(elem, nodeName.getCString(), *conformance, "");
                        nodeName.clear()->append(ns)->append(":")->append("rev");
                        getElemOrAttrData(elem, nodeName.getCString(), *conformance, ":");
                    }
                    ns = findXmpPrefix(elem, R"(http://www.npes.org/pdfx/ns/id/)");
                    if (ns)    // PDF/X
                    {
                        if (conformance->getLength())
                        {
                            conformance->append(';');
                        }
                        nodeName.clear()->append(ns)->append(":")->append("GTS_PDFXVersion");
                        getElemOrAttrData(elem, nodeName.getCString(), *conformance, "");
                    }
                    ns = findXmpPrefix(elem, R"(http://ns.adobe.com/pdfx/1.3/)");
                    if (ns)    // PDF/X, non-standard
                    {
                        if (conformance->getLength())
                        {
                            conformance->append(';');
                        }
                        nodeName.clear()->append(ns)->append(":")->append("GTS_PDFXVersion");
                        getElemOrAttrData(elem, nodeName.getCString(), *conformance, "");
                    }
                    ns = findXmpPrefix(elem, R"(http://www.aiim.org/pdfe/ns/id/)");
                    if (ns)    // PDF/E
                    {
                        if (conformance->getLength())
                        {
                            conformance->append(';');
                        }
                        nodeName.clear()->append(ns)->append(":")->append("ISO_PDFEVersion");
                        getElemOrAttrData(elem, nodeName.getCString(), *conformance, "");
                    }
                    ns = findXmpPrefix(elem, R"(http://www.aiim.org/pdfua/ns/id/)");
                    if (ns)    // PDF/UA
                    {
                        if (conformance->getLength())
                        {
                            conformance->append(';');
                        }
                        nodeName.clear()->append(ns)->append(":")->append("part");
                        getElemOrAttrData(elem, nodeName.getCString(), *conformance, "PDF/UA-");
                    }
                }
            }
        }
    }
    {
        char buf[33]{ 0 };
        constexpr auto toRead{ sizeof(buf) - 1 };
        // auto current{ getBaseStream()->getPos() };
        auto end{ getXRef()->getLastStartxrefPos() };
        auto start{ end > toRead ? end - toRead : 0 };
        getBaseStream()->setPos(start, 0);
        getBaseStream()->getBlock(buf, toRead);
        auto pos{ strstr(buf, "%PDF-raster-") };
        if (pos)
        {
            pos += 12U;
            if (conformance->getLength())
            {
                conformance->append(';');
            }
            conformance->append("PDF/R-")->append(pos, 3);
        }

    }
    return conformance.release();
}

/**
* Find XMP prefix name for nsURI.
* XMP namespace prefixes should be standardized, e.g.:
* xmlns:xmp="http://ns.adobe.com/xap/1.0/", but there is also xmlns:xap="http://ns.adobe.com/xap/1.0/"
*
* @return prefix for the selected namespace URI, or nullptr if not found
*/
const char* PDFDocEx::findXmpPrefix(const ZxElement* elem, const char* nsURI)
{
    for (const ZxAttr* attr{ elem->getFirstAttr() }; attr; attr = attr->getNextAttr())
    {
        if (attr->getValue()->cmp(nsURI) == 0)
        {
            auto attrName{ attr->getName() };
            auto ptr{ strchr(attrName->getCString(), ':') };
            if (ptr)
            {
                // it is possible to return pointer to attribute name, 
                // because XMP content is stored in m_xmp and not modified
                return ptr + 1;
            }
        }
    }
    return nullptr;
}

/**
* Get value from XMP metadata.
* 
* @param[in]    nsURI       XMP namespace URI
* @param[in]    key         XMP node name (element or attribute) without namespace prefix
* @param[in]    arrayType   type of ordered array (rdf:Alt rdf:Bag or rdf:Seq) or nullptr if node is not an array
*
* @return pointer to allocated GString, needs to be deleted/released after usage.
*/
GString* PDFDocEx::getXmpValue(const char* nsURI, const char* key, const char* arrayType)
{
    if (openXMP())
    {
        auto root{ m_xmp->getRoot() };
        if (root->isElement("x:xmpmeta"))
        {
            root = root->findFirstChildElement("rdf:RDF");
        }
        if (root && root->isElement("rdf:RDF"))
        {
            for (auto node{ root->getFirstChild() }; node; node = node->getNextChild())
            {
                if (node->isElement("rdf:Description"))
                {
                    const auto elem{ static_cast<ZxElement*>(node) };
                    GString nodeName(findXmpPrefix(elem, nsURI));
                    if (nodeName.getLength())
                    {
                        auto value{ std::make_unique<GString>() };
                        if (value)
                        {
                            nodeName.append(':')->append(key);
                            if (getElemOrAttrData(elem, nodeName.getCString(), *value, ""))
                            {
                                return value.release();
                            }
                            if (arrayType)
                            {
                                auto child{ node->findFirstChildElement(nodeName.getCString()) };
                                if (child)
                                {
                                    auto arr{ child->findFirstChildElement(arrayType) };
                                    if (arr)
                                    {
                                        if (getElemOrAttrData(arr, "rdf:li", *value, ""))
                                        {
                                            return value.release();
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }
    }
    return nullptr;
}

/**
* Check if page content is empty or too short.
* 
* @param[in]    page       pointer to Page object
* 
* @return true if page content is empty or too short
*/
bool PDFDocEx::pageContentIsEmpty(Page* page)
{
    auto isEmpy{ true };
    Object contentsObj;
    if (page->getContents(&contentsObj)->isArray())
    {
        // there should be some content
        isEmpy = false;
    }
    else if (contentsObj.isStream())
    {
        const auto dict{ contentsObj.streamGetDict() };
        if (dict)
        {
            Object lenObj;
            if (dict->lookup("Length", &lenObj)->isInt() && (lenObj.getInt() >= globalOptionsFromIni.pageContentsLengthMin))
            {
                isEmpy = false;
            }
            else
            {
                // empty page, stream too short
                TRACE(L"%hs!%d!empty page, stream len=%d\n", __FUNCTION__, page->getNum(), lenObj.getInt());
            }
        }
        else
        {
            TRACE(L"%hs!%d!stream has no dict\n", __FUNCTION__, page->getNum());
        }
    }
    else
    {
        // empty page
        TRACE(L"%hs!%d!empty page, no /Contents\n", __FUNCTION__, page->getNum());
    }
    contentsObj.free();

    return isEmpy;
}

/**
* Get number of pages without Font resource.
*
* It can be used to detect pages without searchable or extractable text.
* 
* @return Number of fontless pages.
*/
int PDFDocEx::getNumFontlessPages()
{
    int nPages{ 0 };
    const auto cat{ getCatalog() };
    if (cat)
    {
        const auto numPages{ getNumPages() };
        for (int i{ 1 }; i <= numPages; i++)
        {
            const auto page{ cat->getPage(i) };
            if (page)
            {
                auto countPageAsFontless{ true };
                const auto attrs{ page->getAttrs() };
                if (attrs)
                {
                    const auto resource{ attrs->getResourceDict() };
                    if (resource)
                    {
                        Object fontObj;
                        ;
                        if (resource->lookup("Font", &fontObj)->isDict())
                        {
                            countPageAsFontless = false;
                        }
                        else
                        {
                            TRACE(L"%hs!%d!no /Font\n", __FUNCTION__, i);
                        }
                        fontObj.free();
                    }
                    else
                    {
                        TRACE(L"%hs!%d!no /Resources\n", __FUNCTION__, i);
                    }
                }
                else
                {
                    TRACE(L"%hs!%d!no /Attrs\n", __FUNCTION__, i);
                }

                // don't count empty pages as fontless
                if (countPageAsFontless && (globalOptionsFromIni.pageContentsLengthMin > 0) && pageContentIsEmpty(page))
                {
                    countPageAsFontless = false;
                }
                if (countPageAsFontless)
                {
                    ++nPages;
                }
            }
            else
            {
                TRACE(L"%hs!%d!page is NULL\n", __FUNCTION__, i);
            }
        }
    }
    else
    {
        TRACE(L"%hs!no catalog\n", __FUNCTION__);
    }
    return nPages;
}

/**
* Get number of pages with XObject Image.
*
* It can be used to detect pages with images.
* If #globalOptionsFromIni.excludeEmptyPages is set to false,
* empty pages are not counted as pages with images.
* 
* Inline images are not checked.
*
* @return Number of pages with images.
*/
int PDFDocEx::getNumPagesWithImages()
{
    int nPages{ 0 };
    const auto cat{ getCatalog() };
    if (cat)
    {
        const auto numPages{ getNumPages() };
        for (int i{ 1 }; i <= numPages; i++)
        {
            const auto page{ cat->getPage(i) };
            if (page)
            {
                auto pageHasImages{ false };
                const auto attrs{ page->getAttrs() };
                if (attrs)
                {
                    const auto resource{ attrs->getResourceDict() };
                    if (resource)
                    {
                        Object xObjDict;
                        if (resource->lookup("XObject", &xObjDict)->isDict())
                        {
                            const auto xObjDictLen{ xObjDict.dictGetLength() };
                            for (int j{ 0 }; j < xObjDictLen; j++)
                            {
                                Object xObj;
                                if (xObjDict.dictGetVal(j, &xObj)->isStream())
                                {
                                    Object subtypeObj;
                                    xObj.streamGetDict()->lookup("Subtype", &subtypeObj);
                                    if (subtypeObj.isName("Image"))
                                    {
                                        // image object
                                        TRACE(L"%hs!%d!/XObject /Image\n", __FUNCTION__, i);
                                        pageHasImages = true;
                                        subtypeObj.free();
                                        xObj.free();
                                        break;
                                    }
                                    subtypeObj.free();
                                }
                                xObj.free();
                            }
                        }
                        else
                        {
                            TRACE(L"%hs!%d!no /XObject\n", __FUNCTION__, i);
                        }
                        xObjDict.free();
                    }
                    else
                    {
                        TRACE(L"%hs!%d!no /Resources\n", __FUNCTION__, i);
                    }
                }
                else
                {
                    TRACE(L"%hs!%d!no /Attrs\n", __FUNCTION__, i);
                }

                // Page has images, but stream is too short to show them, don't count it
                if (pageHasImages && (globalOptionsFromIni.pageContentsLengthMin > 0) && pageContentIsEmpty(page))
                {
                    pageHasImages = false;
                }
                if (pageHasImages)
                {
                    ++nPages;
                }
            }
            else
            {
                TRACE(L"%hs!%d!page is NULL\n", __FUNCTION__, i);
            }
        }
    }
    else
    {
        TRACE(L"%hs!no catalog\n", __FUNCTION__);
    }
    return nPages;
}
