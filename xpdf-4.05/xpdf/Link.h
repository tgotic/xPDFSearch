//========================================================================
//
// Link.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef LINK_H
#define LINK_H

#include <aconf.h>

#include "Object.h"

class GString;
class Array;
class Dict;

//------------------------------------------------------------------------
// LinkAction
//------------------------------------------------------------------------

enum LinkActionKind {
  actionGoTo,			// go to destination
  actionGoToR,			// go to destination in new file
  actionLaunch,			// launch app (or open document)
  actionURI,			// URI
  actionNamed,			// named action
  actionMovie,			// movie action
  actionJavaScript,		// run JavaScript
  actionSubmitForm,		// submit form
  actionHide,			// hide annotation
  actionUnknown			// anything else
};

class LinkAction {
public:

  // Destructor.
  virtual ~LinkAction() {}

  // Was the LinkAction created successfully?
  virtual GBool isOk() = 0;

  // Check link action type.
  virtual LinkActionKind getKind() = 0;

  // Parse a destination (old-style action) name, string, or array.
  static LinkAction *parseDest(Object *obj);

  // Parse an action dictionary.
  static LinkAction *parseAction(Object *obj, GString *baseURI = NULL);

  // Extract a file name from a file specification (string or
  // dictionary).
  static GString *getFileSpecName(Object *fileSpecObj);
};

//------------------------------------------------------------------------
// LinkDest
//------------------------------------------------------------------------

enum LinkDestKind {
  destXYZ,
  destFit,
  destFitH,
  destFitV,
  destFitR,
  destFitB,
  destFitBH,
  destFitBV
};

class LinkDest {
public:

  // Build a LinkDest from the array.
  LinkDest(Array *a);

  // Copy a LinkDest.
  LinkDest *copy() { return new LinkDest(this); }

  // Was the LinkDest created successfully?
  GBool isOk() { return ok; }

  // Accessors.
  LinkDestKind getKind() { return kind; }
  GBool isPageRef() { return pageIsRef; }
  int getPageNum() { return pageNum; }
  Ref getPageRef() { return pageRef; }
  double getLeft() { return left; }
  double getBottom() { return bottom; }
  double getRight() { return right; }
  double getTop() { return top; }
  double getZoom() { return zoom; }
  GBool getChangeLeft() { return changeLeft; }
  GBool getChangeTop() { return changeTop; }
  GBool getChangeZoom() { return changeZoom; }

private:

  LinkDestKind kind;		// destination type
  GBool pageIsRef;		// is the page a reference or number?
  union {
    Ref pageRef;		// reference to page
    int pageNum;		// one-relative page number
  };
  double left, bottom;		// position
  double right, top;
  double zoom;			// zoom factor
  GBool changeLeft, changeTop;	// which position components to change:
  GBool changeZoom;		//   destXYZ uses all three;
				//   destFitH/BH use changeTop;
				//   destFitV/BV use changeLeft
  GBool ok;			// set if created successfully

  LinkDest(LinkDest *dest);
};

//------------------------------------------------------------------------
// LinkGoTo
//------------------------------------------------------------------------

class LinkGoTo: public LinkAction {
public:

  // Build a LinkGoTo from a destination (dictionary, name, or string).
  LinkGoTo(Object *destObj);
  LinkGoTo(const LinkGoTo&) = delete;
  LinkGoTo& operator=(const LinkGoTo&) = delete;
  // Destructor.
  virtual ~LinkGoTo();

  // Was the LinkGoTo created successfully?
  virtual GBool isOk() { return dest || namedDest; }

  // Accessors.
  virtual LinkActionKind getKind() { return actionGoTo; }
  LinkDest *getDest() { return dest; }
  GString *getNamedDest() { return namedDest; }

private:

  LinkDest *dest;		// regular destination (NULL for remote
				//   link with bad destination)
  GString *namedDest;		// named destination (only one of dest and
				//   and namedDest may be non-NULL)
};

//------------------------------------------------------------------------
// LinkGoToR
//------------------------------------------------------------------------

class LinkGoToR: public LinkAction {
public:

  // Build a LinkGoToR from a file spec (dictionary) and destination
  // (dictionary, name, or string).
  LinkGoToR(Object *fileSpecObj, Object *destObj);
  LinkGoToR(const LinkGoToR&) = delete;
  LinkGoToR& operator=(const LinkGoToR&) = delete;

  // Destructor.
  virtual ~LinkGoToR();

  // Was the LinkGoToR created successfully?
  virtual GBool isOk() { return fileName && (dest || namedDest); }

  // Accessors.
  virtual LinkActionKind getKind() { return actionGoToR; }
  GString *getFileName() { return fileName; }
  LinkDest *getDest() { return dest; }
  GString *getNamedDest() { return namedDest; }

private:

  GString *fileName;		// file name
  LinkDest *dest;		// regular destination (NULL for remote
				//   link with bad destination)
  GString *namedDest;		// named destination (only one of dest and
				//   and namedDest may be non-NULL)
};

//------------------------------------------------------------------------
// LinkLaunch
//------------------------------------------------------------------------

class LinkLaunch: public LinkAction {
public:

  // Build a LinkLaunch from an action dictionary.
  LinkLaunch(Object *actionObj);

  // Destructor.
  virtual ~LinkLaunch();

  // Was the LinkLaunch created successfully?
  virtual GBool isOk() { return fileName != NULL; }

  // Accessors.
  virtual LinkActionKind getKind() { return actionLaunch; }
  GString *getFileName() { return fileName; }
  GString *getParams() { return params; }

private:

  GString *fileName;		// file name
  GString *params;		// parameters
};

//------------------------------------------------------------------------
// LinkURI
//------------------------------------------------------------------------

class LinkURI: public LinkAction {
public:

  // Build a LinkURI given the URI (string) and base URI.
  LinkURI(Object *uriObj, GString *baseURI);
  LinkURI(const LinkURI&) = delete;
  LinkURI& operator=(const LinkURI&) = delete;

  // Destructor.
  virtual ~LinkURI();

  // Was the LinkURI created successfully?
  virtual GBool isOk() { return uri != NULL; }

  // Accessors.
  virtual LinkActionKind getKind() { return actionURI; }
  GString *getURI() { return uri; }

private:

  GString *uri;			// the URI
};

//------------------------------------------------------------------------
// LinkNamed
//------------------------------------------------------------------------

class LinkNamed: public LinkAction {
public:

  // Build a LinkNamed given the action name.
  LinkNamed(Object *nameObj);
  LinkNamed(const LinkNamed&) = delete;
  LinkNamed& operator=(const LinkNamed&) = delete;

  virtual ~LinkNamed();

  // Was the LinkNamed created successfully?
  virtual GBool isOk() { return name != NULL; }

  // Accessors.
  virtual LinkActionKind getKind() { return actionNamed; }
  GString *getName() { return name; }

private:

  GString *name;
};

//------------------------------------------------------------------------
// LinkMovie
//------------------------------------------------------------------------

class LinkMovie: public LinkAction {
public:

  LinkMovie(Object *annotObj, Object *titleObj);

  virtual ~LinkMovie();

  // Was the LinkMovie created successfully?
  virtual GBool isOk() { return annotRef.num >= 0 || title != NULL; }

  // Accessors.
  virtual LinkActionKind getKind() { return actionMovie; }
  GBool hasAnnotRef() { return annotRef.num >= 0; }
  Ref *getAnnotRef() { return &annotRef; }
  GString *getTitle() { return title; }

private:

  Ref annotRef;
  GString *title;
};

//------------------------------------------------------------------------
// LinkJavaScript
//------------------------------------------------------------------------

class LinkJavaScript: public LinkAction {
public:

  LinkJavaScript(Object *jsObj);
  LinkJavaScript(const LinkJavaScript&) = delete;
  LinkJavaScript& operator=(const LinkJavaScript&) = delete;

  virtual ~LinkJavaScript();

  // Was the LinkJavaScript created successfully?
  virtual GBool isOk() { return js != NULL; }

  // Accessors.
  virtual LinkActionKind getKind() { return actionJavaScript; }
  GString *getJS() { return js; }

private:

  GString *js;
};

//------------------------------------------------------------------------
// LinkSubmitForm
//------------------------------------------------------------------------

class LinkSubmitForm: public LinkAction {
public:

  LinkSubmitForm(Object *urlObj, Object *fieldsObj, Object *flagsObj);

  virtual ~LinkSubmitForm();

  // Was the LinkSubmitForm created successfully?
  virtual GBool isOk() { return url != NULL; }

  // Accessors.
  virtual LinkActionKind getKind() { return actionSubmitForm; }
  GString *getURL() { return url; }
  Object *getFields() { return &fields; }
  int getFlags() { return flags; }

private:

  GString *url;
  Object fields;
  int flags;
};

//------------------------------------------------------------------------
// LinkHide
//------------------------------------------------------------------------

class LinkHide: public LinkAction {
public:

  LinkHide(Object *fieldsObj, Object *hideFlagObj);

  virtual ~LinkHide();

  // Was the LinkHide created successfully?
  virtual GBool isOk() { return !fields.isNull(); }

  // Accessors.
  virtual LinkActionKind getKind() { return actionHide; }
  Object *getFields() { return &fields; }
  GBool getHideFlag() { return hideFlag; }

private:

  Object fields;
  GBool hideFlag;
};

//------------------------------------------------------------------------
// LinkUnknown
//------------------------------------------------------------------------

class LinkUnknown: public LinkAction {
public:

  // Build a LinkUnknown with the specified action type.
  LinkUnknown(char *actionA);
  LinkUnknown(const LinkUnknown&) = delete;
  LinkUnknown& operator=(const LinkUnknown&) = delete;

  // Destructor.
  virtual ~LinkUnknown();

  // Was the LinkUnknown create successfully?
  virtual GBool isOk() { return action != NULL; }

  // Accessors.
  virtual LinkActionKind getKind() { return actionUnknown; }
  GString *getAction() { return action; }

private:

  GString *action;		// action subtype
};

//------------------------------------------------------------------------
// Link
//------------------------------------------------------------------------

class Link {
public:

  // Construct a link, given its dictionary.
  Link(Dict *dict, GString *baseURI);

  // Destructor.
  ~Link();

  // Was the link created successfully?
  GBool isOk() { return ok; }

  // Check if point is inside the link rectangle.
  GBool inRect(double x, double y)
    { return x1 <= x && x <= x2 && y1 <= y && y <= y2; }

  // Get action.
  LinkAction *getAction() { return action; }

  // Get the link rectangle.
  void getRect(double *xa1, double *ya1, double *xa2, double *ya2)
    { *xa1 = x1; *ya1 = y1; *xa2 = x2; *ya2 = y2; }

private:

  double x1, y1;		// lower left corner
  double x2, y2;		// upper right corner
  LinkAction *action;		// action
  GBool ok;			// is link valid?
};

//------------------------------------------------------------------------
// Links
//------------------------------------------------------------------------

class Links {
public:

  // Extract links from array of annotations.
  Links(Object *annots, GString *baseURI);

  // Destructor.
  ~Links();

  // Iterate through list of links.
  int getNumLinks() { return numLinks; }
  Link *getLink(int i) { return links[i]; }

  // If point <x>,<y> is in a link, return the associated action;
  // else return NULL.
  LinkAction *find(double x, double y);

  // Return true if <x>,<y> is in a link.
  GBool onLink(double x, double y);

private:

  Link **links;
  int numLinks;
};

#endif
