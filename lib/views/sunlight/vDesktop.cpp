// (C) 2002-2026 The UbixOS Project
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
vDesktop::DeleteStyle(const char * /*styleName*/) {
  return false;
}

sStyle *
vDesktop::GetStyle(const char * /*styleName*/) {
  return NULL;
} // vDesktop::GetStyle

void
vDesktop::SetPos(int32 newX, int32 newY) {
  (void)newX; (void)newY;
  return;
} // vDesktop::SetPos

void
vDesktop::SetStyle(const char * /*nameStyle*/, sStyle * /*style*/) {
  return;
} // vDesktop::SetStyle

vDesktop::~vDesktop(void) {
  return;
} // vDesktop::~vDesktop
