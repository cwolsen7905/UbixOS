#ifndef UBIXBUTTON_H
#define UBIXBUTTON_H

#include <vButton.h>

class ubixButton : public vButton {
  public:
                ubixButton(vContext *);
//   virtual bool vCreate(void);
   virtual void vDraw(void);
   virtual     ~ubixButton(void);
}; // ubixButton
#endif
