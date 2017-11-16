#ifndef UBIXDESKTOP_H
#define UBIXDESKTOP_H

#include <vContext.h>

class ubixDesktop : public vContext {
 protected:
 public:
           ubixDesktop(vContext *);
  virtual bool vCreate(void);
  virtual void vDraw(void);
  virtual ~ubixDesktop(void);
}; // ubixDesktop
#endif
