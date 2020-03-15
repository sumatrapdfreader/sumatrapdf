#ifndef REMINDER

  #define MAKE_QUOTE(str) #str
  #define MAKE_STR(str) MAKE_QUOTE(str)
  #if defined WIN32
    // This enables: #pragma REMINDER("change this line!") with click-through from VC++.
    #define REMINDER(msg) message(__FILE__   "(" MAKE_STR(__LINE__) "): " msg)
  #else 
    #define REMINDER(msg) // no-op
  #endif

#endif

