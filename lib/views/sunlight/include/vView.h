#ifndef VVIEW_H
#define VVIEW_H

#include <vContext.h>
#include <vTitleTab.h>
#include <vCanvas.h>

class vView : public vTitleTab, public vCanvas {
 protected:
 public:
           vView(vContext *);
  virtual  ~vView(void);
}; // vView

#endif
