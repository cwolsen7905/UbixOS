#include <ubixButton.h>
#include <vContext.h>
#include <sTypes.h>
#include <sys/types.h>

ubixButton::ubixButton(vContext * parent) : vButton(parent) {
  vSetSize(48, 48);
  vSetPos(0, 600-vGetHeight());
  vSetStyle("default.button.border.size", new sSize(0));
  return;
} // ubixButton::ubixButton()

void
ubixButton::vDraw(void) {
  return;
} // ubixButton::vDraw()

ubixButton::~ubixButton(void) {
  return;
} // ubixButton::~ubixButton()
