#ifndef VCANVAS_H
#define VCANVAS_H

#include <vContext.h>

class vCanvas : public vContext {
 protected:
 public: 
         vCanvas(vContext *);
 virtual ~vCanvas(void);
}; // vCanvas
#endif
