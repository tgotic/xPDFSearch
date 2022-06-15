/**
* @file
*
* TcOutputDev class declaration.
*/

#pragma once
#include "ThreadData.hh"

/**
* Class for text extraction from PDF to TC.
*/
class TcOutputDev
{
public:
    explicit TcOutputDev();
    TcOutputDev(const TcOutputDev&) = delete;
    TcOutputDev& operator=(const TcOutputDev&) = delete;

    void output(PDFDoc* doc, ThreadData* data);
private:
    std::unique_ptr<TextOutputDev>  m_dev{ nullptr };   /**< text extractor */
    TextOutputControl               toc;                /**< settings for TextOutputDev */
};
