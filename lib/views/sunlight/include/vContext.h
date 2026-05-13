#ifndef VCONTEXT_H
#define VCONTEXT_H

#include <objgfx40.h>
#include <sStyle.h>

#define VCONTEXT_MAX_STYLES   32
#define VCONTEXT_MAX_CHILDREN 32
#define VCONTEXT_KEY_LEN      64

struct sStyleEntry {
  char    key[VCONTEXT_KEY_LEN];
  sStyle *value;
};

class vContext : public ogSurface {
 protected:
  sStyleEntry   styles[VCONTEXT_MAX_STYLES];
  int           styleCount;
  vContext     *children[VCONTEXT_MAX_CHILDREN];
  int           childCount;
  vContext     *pContext;

  ogSurface    *realView;
  int32         curX, curY;
  uInt32        width, height;
  bool          attached;
 public:
                vContext(vContext *);
  virtual vContext *vAttach(vContext *);
  virtual bool  vCreate(void) = 0;
  virtual void  vDeleteAllStyles(void);
  virtual bool  vDeleteStyle(const char *);
  virtual void  vDraw(void) = 0;
  virtual vContext *vDetach(vContext *);
  virtual uInt32 vGetHeight(void) { return height; };
  virtual sStyle *vGetStyle(const char *);
  virtual uInt32 vGetWidth(void) { return width; };
  virtual bool  vIsAttached(void) { return attached; }
  virtual void  vSetPos(int32, int32);
  virtual void  vSetSize(uInt32, uInt32);
  virtual void  vSetStyle(const char *, sStyle *);
  virtual      ~vContext();
}; // vContext

#endif
