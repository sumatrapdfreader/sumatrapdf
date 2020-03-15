#ifndef _WDL_DESTROYCHECK_H_
#define _WDL_DESTROYCHECK_H_

// this is a useful class for verifying that an object (usually "this") hasn't been destroyed:
// to use it you add a WDL_DestroyState as a member of your class, then use the WDL_DestroyCheck 
// helper class (creating it when the pointer is known valid, and checking it later to see if it 
// is still valid).
//
// example:
// class myClass {
//   WDL_DestroyState dest;
//   ...
// };
//
// calling code (on myClass *classInstnace):
//  WDL_DestroyCheck chk(&classInstance->dest);
//  somefunction();
//  if (!chk.isOK()) printf("classInstance got deleted!\n");
//
// NOTE: only use this when these objects will be accessed from the same thread -- it will fail miserably
// in a multithreaded environment




class WDL_DestroyCheck
{
  public:
    class WDL_DestroyStateNextRec { public: WDL_DestroyCheck *next; };

    WDL_DestroyStateNextRec n, *prev; 
    WDL_DestroyCheck(WDL_DestroyStateNextRec *state)
    {
      n.next=NULL;
      if ((prev=state)) 
      {
        if ((n.next=prev->next)) n.next->prev = &n;
        prev->next=this;
      }
    }
    ~WDL_DestroyCheck() 
    { 
      if (prev)
      {
        prev->next = n.next; 
        if (n.next) n.next->prev = prev; 
      }
    }

    bool isOK() { return !!prev; }
};

class WDL_DestroyState : public WDL_DestroyCheck::WDL_DestroyStateNextRec
{
  public:
    WDL_DestroyState() { next=NULL; }

    ~WDL_DestroyState() 
    { 
      WDL_DestroyCheck *p = next;
      while (p) { WDL_DestroyCheck *np = p->n.next; p->prev=NULL; p->n.next=NULL; p=np; }
    }
};

#endif
