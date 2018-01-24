#include "vWidget.h"

bool 
vWidget::vSetActive(bool _active) {
  bool result = active;
  active = _active;
  return result;
} // vWidget::vSetActive

