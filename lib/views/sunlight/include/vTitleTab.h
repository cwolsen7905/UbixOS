#ifndef VTITLETAB_H
#define VTITLETAB_H

#include <vContext.h>
#include <ogFont.h>

class vTitleTab : public vContext {
 protected:
   ogBitFont *font;
   char       title[256];
 public:
               vTitleTab(vContext *);
  virtual void vDraw(void);
  void         vSetTitle(const char *);
  virtual     ~vTitleTab(void);
}; // vTitleTab

#endif
