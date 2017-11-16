#ifndef VBUTTON_H
#define VBUTTON_H

#include "vContext.h"

class vButton : public vContext {
 protected:
 public:
               vButton(vContext *);
  virtual bool vCreate(void);
  virtual     ~vButton();
}; // vButton
#endif
