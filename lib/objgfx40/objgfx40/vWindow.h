#ifndef VWINDOW_H
#define VWINDOW_H

#include <objgfx40/objgfx40.h>
#include <objgfx40/ogFont.h>
#include <objgfx40/vWidget.h>

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
