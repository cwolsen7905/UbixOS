// (C) 2002-2026 The UbixOS Project
#include <string.h>
#include <stdlib.h>
#include <objgfx40.h>
#include <vContext.h>
#include <sStyle.h>
#include <sTypes.h>

vContext::vContext(vContext * parent) {
  pContext   = parent;
  realView   = new ogSurface();
  curX       = curY = 0;
  width      = height = 0;
  attached   = false;
  styleCount = 0;
  childCount = 0;
  return;
} // vContext::vContext

vContext *
vContext::vAttach(vContext * context) {
  if (childCount < VCONTEXT_MAX_CHILDREN)
    children[childCount++] = context;
  return context;
} // vContext::vAttach

void
vContext::vDeleteAllStyles(void) {
  for (int i = 0; i < styleCount; i++) {
    delete styles[i].value;
    styles[i].value  = NULL;
    styles[i].key[0] = '\0';
  }
  styleCount = 0;
} // vContext::vDeleteAllStyles

bool
vContext::vDeleteStyle(const char * styleKey) {
  for (int i = 0; i < styleCount; i++) {
    if (strncmp(styles[i].key, styleKey, VCONTEXT_KEY_LEN) == 0) {
      delete styles[i].value;
      // shift remaining entries down
      for (int j = i; j < styleCount - 1; j++)
        styles[j] = styles[j + 1];
      styleCount--;
      return true;
    }
  }
  return false;
} // vContext::vDeleteStyle

vContext *
vContext::vDetach(vContext * context) {
  for (int i = 0; i < childCount; i++) {
    if (children[i] == context) {
      for (int j = i; j < childCount - 1; j++)
        children[j] = children[j + 1];
      childCount--;
      return context;
    }
  }
  return context;
} // vContext::vDetach

sStyle *
vContext::vGetStyle(const char * styleKey) {
  for (int i = 0; i < styleCount; i++) {
    if (strncmp(styles[i].key, styleKey, VCONTEXT_KEY_LEN) == 0)
      return styles[i].value;
  }
  if (pContext != NULL)
    return pContext->vGetStyle(styleKey);
  return NULL;
} // vContext::vGetStyle

void
vContext::vSetPos(int32 newX, int32 newY) {
  if ((attached) && (pContext != NULL)) pContext->vDetach(this);
  curX = newX;
  curY = newY;
  if ((attached) && (pContext != NULL)) pContext->vAttach(this);
  return;
} // vContext::vSetPos

void
vContext::vSetSize(uInt32 newWidth, uInt32 newHeight) {
  width  = newWidth;
  height = newHeight;
} // vContext::vSetSize

void
vContext::vSetStyle(const char * styleKey, sStyle * style) {
  if (style == NULL) return;

  // update existing entry if found
  for (int i = 0; i < styleCount; i++) {
    if (strncmp(styles[i].key, styleKey, VCONTEXT_KEY_LEN) == 0) {
      styles[i].value = style;
      return;
    }
  }

  // add new entry
  if (styleCount < VCONTEXT_MAX_STYLES) {
    strncpy(styles[styleCount].key, styleKey, VCONTEXT_KEY_LEN - 1);
    styles[styleCount].key[VCONTEXT_KEY_LEN - 1] = '\0';
    styles[styleCount].value = style;
    styleCount++;
  }
} // vContext::vSetStyle

vContext::~vContext(void) {
  delete realView;
  realView = pContext = NULL;
  curX = curY = width = height = 0;
  vDeleteAllStyles();
  return;
} // vContext::~vContext
