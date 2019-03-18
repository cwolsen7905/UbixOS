#include <string>
#include <stdlib.h>
#include <objgfx40.h>
#include <vContext.h>
#include <sStyle.h>
#include <sTypes.h>
#include <map.h>

vContext::vContext(vContext * parent) {
  pContext = parent;
  realView = new ogSurface();
  curX = curY = width = height = 0;
  attached = false;
  return;
} // vContext::vContext

vContext *
vContext::vAttach(vContext * context) {
  /*
   * vContext::vAttach()
   * Accepts a vContext as a parameter and attaches it to the 
   * current child context list. 
   * Returns the child context pointer
   */
  cContexts.push_back(context);
  return context;
} // vContext::vAttach

void
vContext::vDeleteAllStyles(void) {
  /*
   * vContext::vDeleteAllStyle()
   * Deletes all style entries in this node of the style tree.
   */

  if (styles.empty()) return;

  // create a map<> iterator that points to the beginning style
  map<const std::string, sStyle *>::iterator curStyle = styles.begin();
  
  // loop through the styles, deleting them and calling the style object's
  // destructor

  while (curStyle != styles.end()) {
    std::string str = curStyle->first;
    sStyle * tmpStyle = dynamic_cast<sStyle *>(styles[str]);
// if (tmpStyle != NULL) cout << "deleting styles[\"" << str << "\"]" << endl;
    styles.erase(curStyle);
    delete tmpStyle;
    ++curStyle;
  } // while

  return;
} // vContext::vDeleteAllStyles

bool
vContext::vDeleteStyle(const std::string styleKey) {
  /*
   * vContext::vDeleteStyle()
   * returns true if style existed and was deleted
   * returns false if style didn't exist (or wasn't deleted)
   */

  sStyle * tmpStyle = styles[styleKey];
  styles.erase(styleKey);
  delete tmpStyle;
  return (tmpStyle != NULL);
} // vContext::vDeleteStyle

vContext *
vContext::vDetach(vContext * context) {
  cContexts.remove(context);
  return context;
} // vContext::vDetach

/* 
 * void
 * vContext::Draw(void) {
 *   return;
 * } // vContext::Draw
 */

sStyle *
vContext::vGetStyle(const std::string styleKey) {
  /* 
   * GetStyle()
   * retreives a style out of the style map using the styleKey string
   * If no style is present in this node, check the parent
   */
  sStyle * tmpStyle = styles[styleKey];

  if ((tmpStyle == NULL) && (pContext != NULL)) {
    return pContext->vGetStyle(styleKey);
  } // if

  return tmpStyle;
} // vContext::vGetStyle

void
vContext::vSetPos(int32 newX, int32 newY) {
  /*
   * vContext::SetPos()
   * Sets new position relative to parent's upper left corner
   */

  // I really should detach from the parent here
  if ((attached) && (pContext != NULL)) pContext->vDetach(this);
  curX = newX;
  curY = newY;
  // and reattach to parent here
  if ((attached) && (pContext != NULL)) pContext->vAttach(this);
  return;
} // vContext::vSetPos

void
vContext::vSetSize(uInt32 newWidth, uInt32 newHeight) {
//  if ((attached) && (pContext != NULL)) pContext->vDetach(this);
  width = newWidth;
  height = newHeight; 
//  if ((attached) && (pContext != NULL)) pContext->vAttach(this);
} // vContext::vSetSize

void
vContext::vSetStyle(const std::string styleKey, sStyle * style) {
  // I probably should check to see if a style exists before setting it
  
  // if the style is null, then just exit out without setting it 
  if (style == NULL) return;

  // set the new style
  styles[styleKey] = style;
} // vContext::vSetStyle

vContext::~vContext(void) {
  delete realView;
  realView = pContext = NULL;
  curX = curY = width = height = 0;

  vDeleteAllStyles();

  return;
} // vContext::~vContext
