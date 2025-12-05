/**
* @file
*
* PDF text extraction class and callback functions.
*/

#include "TcOutputDev.hh"
#include "xPDFInfo.hh"

/**
* Callback function used in PdfDoc::displayPage to abort text extraction.
* If #Request::status is not #active, extraction should abort.
* 
* @param[in] stream     pointer to ThreadData structure
* @return gTrue if extraction should abort
*/
static GBool abortExtraction(void* stream)
{
    const auto data{ static_cast<ThreadData*>(stream) };
    if (data && (requestStatus::active == data->getStatus()))
    {
        return gFalse;
    }

    return gTrue;
}

/**
* Callback function used in PdfDoc::displayPage to copy extracted text to request structure.
* For #fiFirstRow field, text is extracted up to first EOL.
* For #fiDocStart field, #REQUEST_BUFFER_SIZE bytes is extracted.
* For #fiText field, data is extracted until TC responds that search string is found.
* To be able to continue to extract text, threading has been used. When block of text has been extracted,
* calling thread is woken up to send data to TC and caller thread goes to sleep. TC compares data and sends back result.
* This wakes thread up and continues text extraction or cancels if string has been found.
* TcOutputDev.c has been modified to speedup extraction cancelation when string has been found.
* Extracted text is converted to UTF-16 and stored to #Request::buffer buffer.
* This callback function may be called multiple times before #Request::buffer is filled up, or line ending has been found.
* 
* @param[in,out]    stream      pointer to ThreadData structure
* @param[in]        text        extracted text
* @param[in]        len         length of extracted text
* @return   0 - extraction shuld continue, 1 - extraction should abort
*/
static int outputFunction(void *stream, const char *text, int len)
{
    // TRACE(L"%hs!len=%d\n", __FUNCTION__, len);
    auto data{ static_cast<ThreadData*>(stream) };
    if (data && (requestStatus::active == data->getStatus()) && text && (len > 0))
    {
        return data->output(text, len, false);
    }
    return 0;
}

/**
* Start text extraction.
* Extraction goes through all document pages until search string is found.
*
* @param[in]        doc     pointer to xPDF PdcDoc instance
* @param[in,out]    data    pointer to request data
*/
void TcOutputDev::output(PDFDoc* doc, ThreadData* data)
{
    if (data && doc && doc->isOk())
    {
        if (!m_dev)
        {
            toc.discardInvisibleText = globalOptionsFromIni.discardInvisibleText;
            toc.discardDiagonalText = globalOptionsFromIni.discardDiagonalText;
            toc.discardClippedText = globalOptionsFromIni.discardClippedText;
            toc.marginBottom = globalOptionsFromIni.marginBottom;
            toc.marginTop = globalOptionsFromIni.marginTop;
            toc.marginLeft = globalOptionsFromIni.marginLeft;
            toc.marginRight = globalOptionsFromIni.marginRight;
            toc.mode = globalOptionsFromIni.textOutputMode;

            // register #outputFunction as a callback function for text extraction
            m_dev = std::make_unique<TextOutputDev>(&outputFunction, data, &toc);
        }

        if (m_dev && m_dev->isOk())
        {
            // for each page
            for (int page{ 1 }; (page <= doc->getNumPages()) && (requestStatus::active == data->getStatus()); ++page) {
                // extract text from page
                doc->displayPage(m_dev.get(), nullptr, page, 72.0, 72.0, 0, gFalse, gTrue, gFalse, abortExtraction, data);
                // release page resources
                doc->getCatalog()->doneWithPage(page);
            }
        }
    }
}
