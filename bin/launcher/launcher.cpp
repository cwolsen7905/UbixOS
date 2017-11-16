#include <vButton.h>
#include <ubixButton.h>
#include <ubixDesktop.h>
#include <stdlib.h>

int 
main(void) {
  ubixDesktop * desktop = new ubixDesktop(NULL);
  ubixButton * daButton = new ubixButton(desktop);
  desktop->vCreate();
  daButton->vCreate();
  delete daButton;
  delete desktop;

  return 0;
}
