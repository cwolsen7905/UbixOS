#include <vContext.h>
#include <vDesktop.h>

vDesktop::vDesktop(vContext * parent) : vContext(parent) {
  return;
} // vDesktop::vDesktop

void
vDesktop::DeleteAllStyles(void) {  
  return;
}

bool
vDesktop::DeleteStyle(const std::string styleName) {
  return false;
}

sStyle * 
vDesktop::GetStyle(const std::string styleName) {
  /*
   * vDesktop::GetStyle
   * This will have to send a message to the Launcher to get the actual
   * style. For now use NULL
   */
  return NULL; 
} // vDesktop::GetStyle

void
vDesktop::SetPos(int32 newX, int32 newY) {
  return;
} // vDesktop::SetPos

void 
vDesktop::SetStyle(const std::string nameStyle, sStyle * style) {
  return;
} // vDesktop::SetStyle

vDesktop::~vDesktop(void) {
  return;
} // vDesktop::~vDesktop
