int static_constructors(void) {
    extern void (* __ctor_list)();
    void (** l_ctor)() = &__ctor_list;
    int l_ctorCount = *(int *)l_ctor;
    
    l_ctor++;
    while(l_ctorCount)
    {
	(*l_ctor)();
	l_ctorCount--;
	l_ctor++;
    }

    return 0;
  }
