typedef char charT;
typedef int size_t;

extern "C++" {

template <class charT> struct string_char_traits {
  typedef charT char_type;
  static char_type eos () { return char_type(); } // the null character
  static void assign (char_type& c1, const char_type& c2) { c1 = c2; }
  static bool ne (const char_type& c1, const char_type& c2) { return !(c1 == c2); }
  static bool lt (const char_type& c1, const char_type& c2) { return (c1 < c2); }
  static int compare (const char_type* s1, const char_type* s2, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i)
      if (ne (s1[i], s2[i]))
        return lt (s1[i], s2[i]) ? -1 : 1;
      return 0;
    }
  static size_t length (const char_type* s) {
    size_t l = 0;
    while (ne (*s++, eos ()))
      ++l;
    return l;
    }
  static char_type* copy (char_type* s1, const char_type* s2, size_t n) {
    for (; n--; )
      assign (s1[n], s2[n]);
    return s1;
    }
  static char_type* move (char_type* s1, const char_type* s2, size_t n) {
    char_type a[n];
    size_t i;
    for (i = 0; i < n; ++i)
      assign (a[i], s2[i]);
    for (i = 0; i < n; ++i)
      assign (s1[i], a[i]);
    return s1;
    }

  };

class streambuf;class ios;class istream; class ostream;

typedef ios& (*__manip)(ios&);
typedef istream& (*__imanip)(istream&);
typedef ostream& (*__omanip)(ostream&);

extern ostream& flush(ostream& outs);

struct _ios_fields { // The data members of an ios.
  void *test;
  };


class ios : public _ios_fields {
  public:
    ios& test(void) { return *this; }
  };

class ostream : virtual public ios {
  public:
    ostream() { }
    ostream& put(char c) { return *this; } 
    ostream& flush();


    ostream& operator<<(char c);
    ostream& operator<<(const char *s);
    ostream& operator<<(int n);
    ostream& operator<<(unsigned int n);
    ostream& operator<<(unsigned long n);
    ostream& operator<<(long n);

    ostream& operator<<(ostream &(*)(ostream &));

  };

class basic_string {
  public:
    void rep();
    };

 }

