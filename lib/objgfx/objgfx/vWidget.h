#ifndef VWIDGET_H
#define VWIDGET_H

#include "objgfx.h"

class vWidget : public ogSurface {
  protected:
    bool   active;
  public:
    vWidget(void) { active = true; }
    virtual void vDraw(void) = 0;
    virtual bool vGetActive(void) const { return active; }
    virtual bool vSetActive(bool);
    virtual bool vCreate(void) = 0;
};

#endif
