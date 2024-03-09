//========================================================================
//
// Catalog.h
//
// Copyright 1996-2007 Glyph & Cog, LLC
//
//========================================================================

#ifndef CATALOG_H
#define CATALOG_H

#include <aconf.h>

#if MULTITHREADED
#include "GMutex.h"
#endif
#include "CharTypes.h"

class GList;
class PDFDoc;
class XRef;
class Object;
class Page;
class PageAttrs;
struct Ref;
class LinkDest;
class PageTreeNode;
class PageLabelNode;
class AcroForm;
class TextString;

//------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------

class Catalog {
public:

  // Constructor.
  Catalog(PDFDoc *docA);
  Catalog(const Catalog&) = delete;
  Catalog& operator=(const Catalog&) = delete;

  // Destructor.
  ~Catalog();

  // Is catalog valid?
  GBool isOk() { return ok; }

  // Get number of pages.
  int getNumPages() { return numPages; }

  // Get a page.
  Page *getPage(int i);

  // Get the reference for a page object.
  Ref *getPageRef(int i);

  // Remove a page from the catalog.  (It can be reloaded later by
  // calling getPage).
  void doneWithPage(int i);

  // Return base URI, or NULL if none.
  GString *getBaseURI() { return baseURI; }

  // Return the contents of the metadata stream, or NULL if there is
  // no metadata.
  GString *readMetadata();

  // Return the structure tree root object.
  Object *getStructTreeRoot() { return &structTreeRoot; }

  // Find a page, given its object ID.  Returns page number, or 0 if
  // not found.
  int findPage(int num, int gen);

  // Find a named destination.  Returns the link destination, or
  // NULL if <name> is not a destination.
  LinkDest *findDest(GString *name);

  Object *getDests() { return &dests; }

  Object *getNameTree() { return &nameTree; }

  Object *getOutline() { return &outline; }

  Object *getAcroForm() { return &acroForm; }

  AcroForm* getForm();

  GBool getNeedsRendering() { return needsRendering; }

  Object *getOCProperties() { return &ocProperties; }

  // Return the DestOutputProfile stream, or NULL if there isn't one.
  Object *getDestOutputProfile(Object *destOutProf);
#ifndef NO_EMBEDDED_CONTENT
  // Get the list of embedded files.
  int getNumEmbeddedFiles();
  Unicode *getEmbeddedFileName(int idx);
  int getEmbeddedFileNameLength(int idx);
  Object *getEmbeddedFileStreamRef(int idx);
  Object *getEmbeddedFileStreamObj(int idx, Object *strObj);
#endif
  // Return true if the document has page labels.
  GBool hasPageLabels();

  // Get the page label for page number [pageNum].  Returns NULL if
  // the PDF file doesn't have page labels.
  TextString *getPageLabel(int pageNum);

  // Returns the page number corresponding to [pageLabel].  Returns -1
  // if there is no matching page label, or if the document doesn't
  // have page labels.
  int getPageNumFromPageLabel(TextString *pageLabel);

  Object *getViewerPreferences() { return &viewerPrefs; }

  Object* getCatalogObj() { return &catDict; }

  // Return true if the document uses JavaScript.
  GBool usesJavaScript();

private:

  PDFDoc *doc;
  XRef *xref;			// the xref table for this PDF file
  PageTreeNode *pageTree{ nullptr };	// the page tree
  Page **pages{ nullptr };			// array of pages
  Ref *pageRefs{ nullptr };		// object ID for each page
#if MULTITHREADED
  GMutex pageMutex;
#endif
  int numPages{ 0 };			// number of pages
  Object dests;			// named destination dictionary
  Object nameTree;		// name tree
  GString *baseURI{ nullptr };		// base URI for URI-type links
  Object metadata;		// metadata stream
  Object structTreeRoot;	// structure tree root dictionary
  Object outline;		// outline dictionary
  Object acroForm;		// AcroForm dictionary
  GBool needsRendering{ gFalse };		// NeedsRendering flag
  AcroForm *form{ nullptr };		// parsed form
  Object ocProperties;		// OCProperties dictionary
  GList *embeddedFiles{ nullptr };		// embedded file list [EmbeddedFile]
  Object pageLabelsObj;		// PageLabel object
  GList *pageLabels{ nullptr };		// page labels [PageLabelNode]
  Object viewerPrefs;		// ViewerPreferences object
  Object catDict;			// This catalog object
  GBool ok{ gFalse };			// true if catalog is valid

  Object *findDestInTree(Object *tree, GString *name, Object *obj);
  GBool readPageTree();
  int countPageTree(Object *pagesNodeRef, char *touchedObjs);
  void loadPage(int pg);
  void loadPage2(int pg, int relPg, PageTreeNode *node);
#ifndef NO_EMBEDDED_CONTENT
  void readEmbeddedFileList(Dict *catDict);
  void readEmbeddedFileTree(Object *nodeRef, char *touchedObjs);
  void readFileAttachmentAnnots(Object *pageNodeRef,
				char *touchedObjs);
  void readEmbeddedFile(Object *fileSpec, Object *name1);
#endif
  void readPageLabelTree(Object *root);
  void readPageLabelTree2(Object *node, char *touchedObjs);
  PageLabelNode *findPageLabel(int pageNum);
  GString *makeRomanNumeral(int num, GBool uppercase);
  GString *makeLetterLabel(int num, GBool uppercase);
  GBool convertPageLabelToInt(TextString *pageLabel, int prefixLength,
			      char style, int *n);
  GBool scanPageTreeForJavaScript(Object *pageNodeRef, char *touchedObjs);
  GBool scanAAForJavaScript(Object *aaObj);
};

#endif
