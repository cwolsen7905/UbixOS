#ifndef VWINDOW_H
#define VWINDOW_H

#include "objgfx.h"
#include "ogFont.h"
#include "vWidget.h"

class vWindow : public vWidget {
  protected:
    ogSurface * realWindow;
    ogBitFont * titleFont;
  public:
             vWindow(void);
    virtual  void vDraw(void) { return; }
    virtual  bool vCreate(void);
    	     void vSDECommand(uInt32);
    virtual  ~vWindow(void);
};
#endif
