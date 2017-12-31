#ifndef VTITLETAB_H
#define VTITLETAB_H

#include <string>
#include <vContext.h>
#include <ogFont.h>

class vTitleTab : public vContext {
 protected:
   ogBitFont * font;
   std::string title;
 public:
               vTitleTab(vContext *);
  virtual void vDraw(void);
  void         vSetTitle(const std::string);
  virtual     ~vTitleTab(void);
}; // vTitleTab

#endif
