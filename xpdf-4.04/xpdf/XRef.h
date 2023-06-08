//========================================================================
//
// XRef.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XREF_H
#define XREF_H

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "gtypes.h"
#include "gfile.h"
#include "Object.h"
#if MULTITHREADED
#include "GMutex.h"
#endif
#include "ErrorCodes.h"

//------------------------------------------------------------------------
// Permission bits
//------------------------------------------------------------------------

#define permPrint    (1<<2)
#define permChange   (1<<3)
#define permCopy     (1<<4)
#define permNotes    (1<<5)
#define defPermFlags 0xfffc

class Dict;
class Stream;
class Parser;
class ObjectStream;
class XRefPosSet;

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

enum XRefEntryType {
  xrefEntryFree,
  xrefEntryUncompressed,
  xrefEntryCompressed
};

struct XRefEntry {
	GFileOffset offset{ 0 };
  int gen{ -1 };
  XRefEntryType type{ xrefEntryFree };
};

struct XRefCacheEntry {
	int num{ -1 };
	int gen{ -1 };
	Object obj;
};

#define xrefCacheSize 16

#define objStrCacheSize 128
#define objStrCacheTimeout 1000

class XRef {
public:

  // Constructor.  Read xref table from stream.
  XRef(BaseStream *strA, GBool repair);

  // Destructor.
  ~XRef();

  // Is xref table valid?
  GBool isOk() { return ok; }

  // Get the error code (if isOk() returns false).
  int getErrorCode() { return errCode; }

  // Was the xref constructed by the repair code?
  GBool isRepaired() { return repaired; }

  // Set the encryption parameters.
  void setEncryption(int permFlagsA, GBool ownerPasswordOkA,
		     Guchar *fileKeyA, int keyLengthA, int encVersionA,
		     CryptAlgorithm encAlgorithmA);

  // Is the file encrypted?
  GBool isEncrypted() { return encrypted; }
  GBool getEncryption(int *permFlagsA, GBool *ownerPasswordOkA,
		      int *keyLengthA, int *encVersionA,
		      CryptAlgorithm *encAlgorithmA);

  // Check various permissions.
  GBool okToPrint(GBool ignoreOwnerPW = gFalse);
  GBool okToChange(GBool ignoreOwnerPW = gFalse);
  GBool okToCopy(GBool ignoreOwnerPW = gFalse);
  GBool okToAddNotes(GBool ignoreOwnerPW = gFalse);
  int getPermFlags() { return permFlags; }

  // Get catalog object.
  Object *getCatalog(Object *obj) { return fetch(rootNum, rootGen, obj); }

  // Fetch an indirect reference.
  Object *fetch(int num, int gen, Object *obj, int recursion = 0);

  // Return the document's Info dictionary (if any).
  Object *getDocInfo(Object *obj);
  Object *getDocInfoNF(Object *obj);

  // Return the number of objects in the xref table.
  int getNumObjects() { return last + 1; }

  // Return the offset of the last xref table.
  GFileOffset getLastXRefPos() { return lastXRefPos; }

  // Return the offset of the 'startxref' at the end of the file.
  GFileOffset getLastStartxrefPos() { return lastStartxrefPos; }

  // Return the catalog object reference.
  int getRootNum() { return rootNum; }
  int getRootGen() { return rootGen; }

  // Get the xref table positions.
  int getNumXRefTables() { return xrefTablePosLen; }
  GFileOffset getXRefTablePos(int idx) { return xrefTablePos[idx]; }

  // Get end position for a stream in a damaged file.
  // Returns false if unknown or file is not damaged.
  GBool getStreamEnd(GFileOffset streamStart, GFileOffset *streamEnd);

  // Direct access.
  int getSize() { return size; }
  XRefEntry *getEntry(int i) { return &entries[i]; }
  Object *getTrailerDict() { return &trailerDict; }

private:

  BaseStream *str;		// input stream
  GFileOffset start{ 0 };		// offset in file (to allow for garbage
				//   at beginning of file)
  XRefEntry* entries{ nullptr };		// xref entries
  int size{ 0 };			// size of <entries> array
  int last{ -1 };			// last used index in <entries>
  int rootNum, rootGen;		// catalog dict
  GBool ok{ gTrue };			// true if xref table is valid
  int errCode{ errNone };			// error code (if <ok> is false)
  GBool repaired{ gFalse };		// set if the xref table was constructed by
				//   the repair code
  Object trailerDict;		// trailer dictionary
  GFileOffset lastXRefPos{ 0 };	// offset of last xref table
  GFileOffset lastStartxrefPos{ 0 };	// offset of 'startxref' at end of file
  GFileOffset *xrefTablePos{ nullptr };	// positions of all xref tables
  int xrefTablePosLen{ 0 };		// number of xref table positions
  GFileOffset *streamEnds{ nullptr };	// 'endstream' positions - only used in
				//   damaged files
  int streamEndsLen{ 0 };		// number of valid entries in streamEnds
  ObjectStream *		// cached object streams
	  objStrs[objStrCacheSize]{ nullptr };
  int objStrCacheLength{ 0 };	// number of valid entries in objStrs[]
  Guint				// time of last use for each obj stream
	  objStrLastUse[objStrCacheSize]{ 0 };
  Guint objStrTime{ 0 };		// current time for the obj stream cache
#if MULTITHREADED
  GMutex objStrsMutex;
#endif
  GBool encrypted{ gFalse };		// true if file is encrypted
  int permFlags{ defPermFlags };		// permission bits
  GBool ownerPasswordOk{ gFalse };	// true if owner password is correct
  Guchar fileKey[32];		// file decryption key
  int keyLength{ 0 };		// length of key, in bytes
  int encVersion{ 0 };		// encryption version
  CryptAlgorithm encAlgorithm;	// encryption algorithm
  XRefCacheEntry		// cache of recently accessed objects
	  cache[xrefCacheSize]{ };
#if MULTITHREADED
  GMutex cacheMutex;
#endif

  GFileOffset getStartXref();
  GBool readXRef(GFileOffset *pos, XRefPosSet *posSet, GBool hybrid);
  GBool readXRefTable(GFileOffset *pos, int offset, XRefPosSet *posSet);
  GBool readXRefStream(Stream *xrefStr, GFileOffset *pos, GBool hybrid);
  GBool readXRefStreamSection(Stream *xrefStr, int *w, int first, int n);
  GBool constructXRef();
  void constructTrailerDict(GFileOffset pos);
  void saveTrailerDict(Dict *dict, GBool isXRefStream);
  char *constructObjectEntry(char *p, GFileOffset pos, int *objNum);
  void constructObjectStreamEntries(Object *objStr, int objStrObjNum);
  GBool constructXRefEntry(int num, int gen, GFileOffset pos,
			   XRefEntryType type);
  GBool getObjectStreamObject(int objStrNum, int objIdx,
			      int objNum, Object *obj);
  ObjectStream *getObjectStream(int objStrNum);
  void cleanObjectStreamCache();
  GFileOffset strToFileOffset(char *s);
};

#endif
