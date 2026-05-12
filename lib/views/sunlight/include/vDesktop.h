#ifndef VDESKTOP_H
#define VDESKTOP_H

#include <vContext.h>

class vDesktop : public vContext {
 protected:
 public:
          vDesktop(vContext *);
  virtual void     DeleteAllStyles(void);
  virtual bool     DeleteStyle(const char *);
  virtual sStyle * GetStyle(const char *);
  virtual void     SetPos(int32, int32);
  virtual void     SetStyle(const char *, sStyle *);
  virtual ~vDesktop(void);
}; // vDesktop
#endif
