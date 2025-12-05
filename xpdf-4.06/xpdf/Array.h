//========================================================================
//
// Array.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef ARRAY_H
#define ARRAY_H

#include <aconf.h>

#if MULTITHREADED
#include "GMutex.h"
#endif
#include "Object.h"

class XRef;

//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

class Array {
public:

  // Constructor.
  Array(XRef *xrefA);

  // Destructor.
  ~Array();

  // Reference counting.
#if MULTITHREADED
  long incRef() { return gAtomicIncrement(&ref); }
  long decRef() { return gAtomicDecrement(&ref); }
#else
  long incRef() { return ++ref; }
  long decRef() { return --ref; }
#endif

  // Get number of elements.
  int getLength() { return length; }

  // Add an element.
  void add(Object *elem);

  // Accessors.
  Object *get(int i, Object *obj, int recursion = 0);
  Object *getNF(int i, Object *obj);

private:

  XRef *xref;			// the xref table for this PDF file
  Object* elems{ nullptr };	// array of elements
  int size{ 0 };		// size of <elems> array
  int length{ 0 };		// number of elements in array
#if MULTITHREADED
  GAtomicCounter ref{ 1 };	// reference count
#else
  long ref{ 1 };		// reference count
#endif
};

#endif
