#ifndef VCONTEXT_H
#define VCONTEXT_H

#include <map>
#include <list>
#include <string>
#include <objgfx40.h>
#include <sStyle.h>

class vContext : public ogSurface {
 protected:
  std::map<const std::string, sStyle *> styles;
  std::list<vContext *> cContexts;   // child contexts
  vContext         * pContext;         // parent context
  
  ogSurface        * realView;
  int32              curX, curY;
  uInt32             width, height;
  bool               attached;
 public:
                     vContext(vContext *);
  virtual vContext * vAttach(vContext *);
  virtual bool       vCreate(void) = 0;
  virtual void       vDeleteAllStyles(void);
  virtual bool       vDeleteStyle(const std::string);
  virtual void       vDraw(void) = 0;
  virtual vContext * vDetach(vContext *);
  virtual uInt32     vGetHeight(void) { return height; };
  virtual sStyle   * vGetStyle(const std::string);
  virtual uInt32     vGetWidth(void) { return width; };
  virtual bool       vIsAttached(void) { return attached; }
  virtual void       vSetPos(int32, int32);
  virtual void       vSetSize(uInt32, uInt32);
  virtual void       vSetStyle(const std::string, sStyle *);
  virtual           ~vContext();

}; // vContext

#endif
