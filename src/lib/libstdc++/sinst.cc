#include <std/straights.h>

extern "C" {
  void * __dynamic_cast(void *a,void *b,void *c,void *d) {
    return(0x0);
    }
  void * __rtti_user(void *a,void *b) {
    return(0x0);
    }
  void * __rtti_class(void *a,void *b,void *c,void *d) {
    return(0x0);
    }
  void * __rtti_si(void *a,void *b,void *c) {
    return(0x0);
    }
  void __eprintf(void *a,void *b,void *c,void *d) {
    return;
    }
  void cerr() {
    return;
    }
} 


typedef char c;

template class string_char_traits <c>;

ostream& endl(ostream& outs) {
  return flush(outs.put('\n'));
  }

ostream& ostream::flush()
{
//    if (_strbuf->sync())
  //      set(ios::badbit);
    return *this;
}



ostream& flush(ostream& outs) {
  return outs.flush();
  }

ostream& ostream::operator<<(const char *s) {
  return(*this);
  }

ostream& ostream::operator<<(char c) {
  return(*this);
  }

ostream& ostream::operator<<(int n) {
  return(*this);
  }

ostream& ostream::operator<<(unsigned int n) {
  return(*this);
  }

ostream& ostream::operator<<(long n) {
  return(*this);
  }

ostream& ostream::operator<<(unsigned long n) {
  return(*this);
  }

ostream& ostream::operator<<(ostream &(*)(ostream &)) {
  return(*this);
  }

void * operator new(unsigned int size,void * ptr) {
  return(ptr);
  }
