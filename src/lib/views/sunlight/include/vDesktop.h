#ifndef VDESKTOP_H
#define VDESKTOP_H

#include <vContext.h>

class vDesktop : public vContext {
 protected:
 public:
          vDesktop(vContext *);
  virtual void     DeleteAllStyles(void);
  virtual bool     DeleteStyle(const std::string);
  virtual sStyle * GetStyle(const std::string);
  virtual void     SetPos(int32, int32);
  virtual void     SetStyle(const std::string, sStyle *);

  virtual ~vDesktop(void);
}; // vDesktop
#endif
