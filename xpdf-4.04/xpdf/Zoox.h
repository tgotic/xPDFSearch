//========================================================================
//
// Zoox.h
//
//========================================================================

#ifndef ZOOX_H
#define ZOOX_H

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "gtypes.h"

class GString;
class GList;
class GHash;

class ZxAttr;
class ZxDocTypeDecl;
class ZxElement;
class ZxXMLDecl;

//------------------------------------------------------------------------

typedef bool (*ZxWriteFunc)(void *stream, const char *data, int length);

//------------------------------------------------------------------------

class ZxNode {
public:

  ZxNode();
  virtual ~ZxNode();

  virtual bool isDoc() const { return false; }
  virtual bool isXMLDecl()  const { return false; }
  virtual bool isDocTypeDecl()  const { return false; }
  virtual bool isComment()  const { return false; }
  virtual bool isPI()  const { return false; }
  virtual bool isElement() const { return false; }
  virtual bool isElement(const char *type) const { return false; }
  virtual bool isCharData() const { return false; }
  virtual ZxNode *getFirstChild() const { return firstChild; }
  virtual ZxNode *getNextChild() const { return next; }
  ZxNode *getParent() const { return parent; }
  ZxNode *deleteChild(ZxNode *child);
  void appendChild(ZxNode *child);
  void insertChildAfter(ZxNode *child, ZxNode *prev);
  ZxElement *findFirstElement(const char *type) const;
  ZxElement *findFirstChildElement(const char *type) const;
  GList *findAllElements(const char *type);
  GList *findAllChildElements(const char *type) const;
  virtual void addChild(ZxNode *child);

  virtual bool write(ZxWriteFunc writeFunc, void *stream) = 0;

protected:

  void findAllElements(const char *type, GList *results);

  ZxNode *next;
  ZxNode *parent;
  ZxNode *firstChild,
         *lastChild;
};

//------------------------------------------------------------------------

class ZxDoc: public ZxNode {
public:

  ZxDoc();

  // Parse from memory.  Returns NULL on error.
  static ZxDoc *loadMem(const char *data, Guint dataLen);

  // Parse from disk.  Returns NULL on error.
  static ZxDoc *loadFile(const char *fileName);

  virtual ~ZxDoc();

  // Write to disk.  Returns false on error.
  bool writeFile(const char *fileName);

  virtual bool isDoc() const { return true; }
  ZxXMLDecl *getXMLDecl() const { return xmlDecl; }
  ZxDocTypeDecl *getDocTypeDecl() const { return docTypeDecl; }
  ZxElement *getRoot() const { return root; }
  virtual void addChild(ZxNode *node);

  virtual bool write(ZxWriteFunc writeFunc, void *stream);

private:

  bool parse(const char *data, Guint dataLen);
  void parseXMLDecl(ZxNode *par);
  void parseDocTypeDecl(ZxNode *par);
  void parseElement(ZxNode *par);
  ZxAttr *parseAttr();
  void parseContent(ZxElement *par);
  void parseCharData(ZxElement *par);
  void appendUTF8(GString *s, unsigned int c);
  void parseCDSect(ZxNode *par);
  void parseMisc(ZxNode *par);
  void parseComment(ZxNode *par);
  void parsePI(ZxNode *par);
  GString *parseName();
  GString *parseQuotedString();
  void parseSpace();
  bool match(const char *s);

  ZxXMLDecl *xmlDecl;		// may be NULL
  ZxDocTypeDecl *docTypeDecl;	// may be NULL
  ZxElement *root;		// may be NULL

  const char *parsePtr;
  const char *parseEnd;
};

//------------------------------------------------------------------------

class ZxXMLDecl: public ZxNode {
public:

  ZxXMLDecl(GString *versionA, GString *encodingA, bool standaloneA);
  virtual ~ZxXMLDecl();

  virtual bool isXMLDecl() const { return true; }
  GString *getVersion() const { return version; }
  GString *getEncoding() const { return encoding; }
  bool getStandalone() const { return standalone; }

  virtual bool write(ZxWriteFunc writeFunc, void *stream);

private:

  GString *version;
  GString *encoding;		// may be NULL
  bool standalone;
};

//------------------------------------------------------------------------

class ZxDocTypeDecl: public ZxNode {
public:

  ZxDocTypeDecl(GString *nameA);
  virtual ~ZxDocTypeDecl();

  virtual bool isDocTypeDecl() const { return true; }
  GString *getName() const { return name; }

  virtual bool write(ZxWriteFunc writeFunc, void *stream);

private:

  GString *name;
};

//------------------------------------------------------------------------

class ZxComment: public ZxNode {
public:

  ZxComment(GString *textA);
  virtual ~ZxComment();

  virtual bool isComment() const { return true; }
  GString *getText() const { return text; }

  virtual bool write(ZxWriteFunc writeFunc, void *stream);

private:

  GString *text;
};

//------------------------------------------------------------------------

class ZxPI: public ZxNode {
public:

  ZxPI(GString *targetA, GString *textA);
  virtual ~ZxPI();

  virtual bool isPI() const { return true; }
  GString *getTarget() const { return target; }
  GString *getText() const { return text; }

  virtual bool write(ZxWriteFunc writeFunc, void *stream);

private:

  GString *target;
  GString *text;
};

//------------------------------------------------------------------------

class ZxElement: public ZxNode {
public:

  ZxElement(GString *typeA);
  ZxElement(const ZxElement&) = delete;
  ZxElement& operator=(const ZxElement&) = delete;

  virtual ~ZxElement();

  virtual bool isElement() const { return true; }
  virtual bool isElement(const char *typeA) const;
  GString *getType() const { return type; }
  ZxAttr *findAttr(const char *attrName)  const;
  ZxAttr *getFirstAttr() const { return firstAttr; }
  void addAttr(ZxAttr *attr);

  virtual bool write(ZxWriteFunc writeFunc, void *stream);

private:

  void appendEscapedAttrValue(GString *out, GString *s);

  GString *type;
  GHash *attrs;			// [ZxAttr]
  ZxAttr *firstAttr, *lastAttr;
};

//------------------------------------------------------------------------

class ZxAttr {
public:

  ZxAttr(GString *nameA, GString *valueA);
  ~ZxAttr();

  GString *getName() const { return name; }
  GString *getValue() const { return value; }
  ZxAttr *getNextAttr() const { return next; }
  ZxNode *getParent() const { return parent; }

private:

  GString *name;
  GString *value;
  ZxElement *parent;
  ZxAttr *next;

  friend class ZxElement;
};

//------------------------------------------------------------------------

class ZxCharData: public ZxNode {
public:

  ZxCharData(GString *dataA, bool parsedA);
  virtual ~ZxCharData();

  virtual bool isCharData() const { return true; }
  GString *getData() const { return data; }
  bool isParsed() const { return parsed; }

  virtual bool write(ZxWriteFunc writeFunc, void *stream);

private:

  GString *data;		// in UTF-8 format
  bool parsed;
};

#endif
