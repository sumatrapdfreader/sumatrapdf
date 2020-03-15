#include "../swell/swell.h"

#include "virtwnd-controls.h"

#include <AppKit/AppKit.h>

@class VWndNSAccessibility;
static VWndNSAccessibility *GetVWndNSAccessible(WDL_VWnd *vwnd);
static WDL_VWnd *__focus;
class VWndBridgeNS;


@interface VWndNSAccessibility : NSObject
{
@public
  VWndBridgeNS *m_br;
  NSArray *m_cached_children;
  int m_cached_children_lastcnt;
  NSArray *m_cached_attrnames;
}
-(id) initWithVWnd:(WDL_VWnd *)vw;
-(void)dealloc;
-(void)clearCaches;


// attribute methods
- (NSArray *)accessibilityAttributeNames;
- (id)accessibilityAttributeValue:(NSString *)attribute;
- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute;
- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute;

// parameterized attribute methods
- (NSArray *)accessibilityParameterizedAttributeNames;
- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter;

// action methods
- (NSArray *)accessibilityActionNames;
- (NSString *)accessibilityActionDescription:(NSString *)action;
- (void)accessibilityPerformAction:(NSString *)action;

// Return YES if the UIElement doesn't show up to the outside world - i.e. its parent should return the UIElement's children as its own - cutting the UIElement out. E.g. NSControls are ignored when they are single-celled.
- (BOOL)accessibilityIsIgnored;

// Returns the deepest descendant of the UIElement hierarchy that contains the point. You can assume the point has already been determined to lie within the receiver. Override this method to do deeper hit testing within a UIElement - e.g. a NSMatrix would test its cells. The point is bottom-left relative screen coordinates.
- (id)accessibilityHitTest:(NSPoint)point;

// Returns the UI Element that has the focus. You can assume that the search for the focus has already been narrowed down to the reciever. Override this method to do a deeper search with a UIElement - e.g. a NSMatrix would determine if one of its cells has the focus.
- (id)accessibilityFocusedUIElement;


@end


class VWndBridgeNS : public WDL_VWnd_IAccessibleBridge
{
public:
  VWndBridgeNS(VWndNSAccessibility *p, WDL_VWnd *vw) 
  { 
     [(par=p) retain]; 
     (vwnd=vw)->SetAccessibilityBridge(this);
  }
  virtual ~VWndBridgeNS()
  { 
//    if (vwnd) printf("Destroying self before Released, wtf!\n");
  }

  virtual void Release() 
  {  
    if (__focus == vwnd) __focus=0;

    vwnd=0; 
    if (par) 
    {
      NSAccessibilityPostNotification(par,NSAccessibilityUIElementDestroyedNotification);
      [par release];
       // this is probably no longer valid!
    }
  }
  virtual void ClearCaches()
  {
    if (par) [par clearCaches];
  }
  virtual void OnFocused() 
  {
    if (vwnd && __focus != vwnd && par)
    {
      __focus = vwnd;
//      NSAccessibilityPostNotification(par,NSAccessibilityFocusedWindowChangedNotification);
      NSAccessibilityPostNotification(par,NSAccessibilityFocusedUIElementChangedNotification);
   }
  } 
  virtual void OnStateChange() 
  {
    if (par) NSAccessibilityPostNotification(par,NSAccessibilityValueChangedNotification);
  }
  
  VWndNSAccessibility *par;
  WDL_VWnd *vwnd;
};

@implementation VWndNSAccessibility
-(id) initWithVWnd:(WDL_VWnd *)vw
{
  if ((self = [super init]))
  {
    m_br = new VWndBridgeNS(self,vw);
    m_cached_children=0;
    m_cached_attrnames = 0;
    m_cached_children_lastcnt=0;
  }
  return self;
}
-(void)clearCaches
{
  if (m_cached_children)
  {
    [m_cached_children release];
    m_cached_children=0;
    m_cached_children_lastcnt=0;
  }
  if (m_cached_attrnames)
  {
    [m_cached_attrnames release];
    m_cached_attrnames = 0;
  }
}
-(void)dealloc
{
  [self clearCaches];
  delete m_br;
  [super dealloc];
}

- (NSArray *)accessibilityAttributeNames
{
  if (m_cached_attrnames) return m_cached_attrnames;
  NSString *s[64];
  int sidx=0;
  const char *type = NULL;
  if (m_br->vwnd)
  {
    type = m_br->vwnd->GetType();
    if (!type) type = "";
  }
  if (type)
  {
//    if (m_br->vwnd->GetNumChildren()) 
    {
      s[sidx++] = NSAccessibilityChildrenAttribute;
      s[sidx++] = NSAccessibilityVisibleChildrenAttribute;
    }
    s[sidx++]=NSAccessibilityTitleAttribute;
        
    if (!strcmp(type,"vwnd_iconbutton")) s[sidx++] = NSAccessibilityEnabledAttribute;
    
    s[sidx++] = NSAccessibilityFocusedAttribute;
    s[sidx++] = NSAccessibilityParentAttribute;
    
    RECT r;
    m_br->vwnd->GetPosition(&r);
    if (m_br->vwnd->IsVisible() && r.right>r.left && r.bottom>r.top)
    {
      s[sidx++] = NSAccessibilityPositionAttribute;
      s[sidx++] = NSAccessibilitySizeAttribute;
    }
    
    s[sidx++] = NSAccessibilityRoleAttribute;
    s[sidx++] = NSAccessibilityRoleDescriptionAttribute;
    
    if (!strcmp(type,"vwnd_statictext")) 
    {
  //    s[sidx++]=NSAccessibilityDescriptionAttribute;
//      s[sidx++]=NSAccessibilityValueDescriptionAttribute;
    }
    
    s[sidx++] = NSAccessibilityWindowAttribute;
    bool hasState = false;
    if (!strcmp(type,"vwnd_iconbutton"))
    {
      hasState = ((WDL_VirtualIconButton*)m_br->vwnd)->GetCheckState()>=0;
    }
    else if (!strcmp(type,"vwnd_combobox")) hasState=true;
    else if (!strcmp(type,"vwnd_slider"))
    {
      // eventually we could remove this check and just query GetAccessValueDesc() directly
      // (but for now do not, because some controls may be plug-in created and not have the 
      // updated base class)
      s[sidx++] = NSAccessibilityValueDescriptionAttribute;
      hasState=true;
    }
    else if (!strcmp(type,"vwnd_tabctrl_proxy"))
    {
      s[sidx++] = NSAccessibilityTabsAttribute;
    }

    if (hasState)
    {
      s[sidx++] = NSAccessibilityMaxValueAttribute;
      s[sidx++] = NSAccessibilityMinValueAttribute;
      s[sidx++] = NSAccessibilityValueAttribute;
    }
  }

  if (m_cached_attrnames) [m_cached_attrnames release];
  m_cached_attrnames = [NSArray arrayWithObjects:s count:sidx];
  [m_cached_attrnames retain];
  return m_cached_attrnames;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
  char buf[2048];
  if (!m_br->vwnd) return nil;
  const char *type = m_br->vwnd->GetType();
  if (!type) type="";
  
  //NSLog(@"Requesting attribute: %@ %s %p\n",attribute,type,m_br->vwnd);
  
  int a = [attribute isEqual:NSAccessibilityChildrenAttribute]?1:0;
  if (!a) a= [attribute isEqual:NSAccessibilityVisibleChildrenAttribute]?2:0;
  if (!a && !strcmp(type,"vwnd_tabctrl_proxy") && [attribute isEqual:NSAccessibilityTabsAttribute])
  {
    a=1;
  }

  if (a) // if 2, only add visible items
  {
    int nc = m_br->vwnd->GetNumChildren();
//    if (!nc) { if (m_cached_children) { [m_cached_children release]; m_cached_children=0; } printf("ret nil\n"); return nil; }

    if (m_cached_children && nc == m_cached_children_lastcnt) return m_cached_children;

    NSMutableArray *ar = [NSMutableArray arrayWithCapacity:nc];
    int x;
    for (x=0;x<nc;x++)
    {
      WDL_VWnd *ch = m_br->vwnd->EnumChildren(x);
      if (!ch) continue;
      RECT r;
      ch->GetPosition(&r);
      if (a==1 || (ch->IsVisible() && r.right>r.left && r.bottom>r.top))
      {
        VWndNSAccessibility *cid = GetVWndNSAccessible(ch);
        if (cid)
        {
          [ar addObject:cid];
          [cid release];
        }
      }
    }
    [m_cached_children release];
    m_cached_children_lastcnt = nc;
    m_cached_children = NSAccessibilityUnignoredChildren(ar);
    [m_cached_children retain];
    return m_cached_children;
  }

  if ([attribute isEqual:NSAccessibilityEnabledAttribute])
  {
    if (!strcmp(type,"vwnd_iconbutton"))
    {
      return [NSNumber numberWithBool:!!((WDL_VirtualIconButton *)m_br->vwnd)->GetEnabled()];
    }
    return nil;
  }
  if ([attribute isEqual:NSAccessibilityFocusedAttribute])
  {
    return [NSNumber numberWithBool:(__focus == m_br->vwnd || (m_br->vwnd && m_br->vwnd->IsDescendent(__focus)))]; // todo focus bleh
  }
  if ([attribute isEqual:NSAccessibilityParentAttribute])
  {
    WDL_VWnd *parw = m_br->vwnd->GetParent();
    if (parw) 
    {
      VWndNSAccessibility *cid = GetVWndNSAccessible(parw);
      if (cid) return NSAccessibilityUnignoredAncestor([cid autorelease]);      
    }
    HWND h =m_br->vwnd->GetRealParent(); 
    if (h) return NSAccessibilityUnignoredAncestor((id)h);
    return NULL;
  }
  if ([attribute isEqual:NSAccessibilityPositionAttribute])
  {
    RECT r;
    m_br->vwnd->GetPosition(&r);
    r.top = r.bottom; // this wants the lower left corner
    WDL_VWnd *p = m_br->vwnd->GetParent();
    while (p)
    {
      RECT tr;
      p->GetPosition(&tr);
      r.left += tr.left;
      r.top += tr.top;
      p = p->GetParent();
    }
    HWND h = m_br->vwnd->GetRealParent();
    if (h)
    {
      ClientToScreen(h,(LPPOINT)&r);
    }
    //printf("position of (%s) %d,%d\n",m_br->vwnd->GetAccessDesc()?m_br->vwnd->GetAccessDesc():"nul",r.left,r.top);
    return [NSValue valueWithPoint:NSMakePoint(r.left,r.top)];
  }
  if ([attribute isEqual:NSAccessibilitySizeAttribute])
  {
    RECT r;
    m_br->vwnd->GetPosition(&r);
//    printf("size of (%s) %d,%d\n",m_br->vwnd->GetAccessDesc()?m_br->vwnd->GetAccessDesc():"nul",r.right-r.left,r.bottom-r.top);
    return [NSValue valueWithSize:NSMakeSize(r.right-r.left,r.bottom-r.top)];
  }
  if ([attribute isEqual:NSAccessibilityRoleDescriptionAttribute])
  {
    const char *str= NULL; 
    if (!str || !*str)
    {
      if (!strcmp(type,"vwnd_statictext")) str = "text";
      else if (!strcmp(type,"vwnd_slider")) str = "slider";
      else if (!strcmp(type,"vwnd_combobox")) str = "selection box";
      else if (!strcmp(type,"vwnd_tabctrl_proxy")) str = "tab list";
      else if (!strcmp(type,"vwnd_tabctrl_child")) str = "tab";
      else if (!strcmp(type,"vwnd_iconbutton"))
      {
        WDL_VirtualIconButton *b = (WDL_VirtualIconButton *)m_br->vwnd;
        if (b->GetCheckState()>=0) str = "check box";
        else str = "button";
      }
      if (!str) str = m_br->vwnd->GetAccessDesc();
    }
    if (str && *str) return [(id)SWELL_CStringToCFString(str) autorelease];

  }
  if ([attribute isEqual:NSAccessibilityRoleAttribute])
  {
    if (!strcmp(type,"vwnd_statictext")) return NSAccessibilityButtonRole; // fail: seems to need 10.5+ to deliver text? NSAccessibilityStaticTextRole;
    if (!strcmp(type,"vwnd_slider")) return NSAccessibilitySliderRole;
    if (!strcmp(type,"vwnd_tabctrl_proxy")) return NSAccessibilityTabGroupRole; // bleh easiest way to get this to work
    if (!strcmp(type,"vwnd_combobox")) return NSAccessibilityPopUpButtonRole;
    if (!strcmp(type,"vwnd_iconbutton"))
    {
      WDL_VirtualIconButton *b = (WDL_VirtualIconButton *)m_br->vwnd;
      if (b->GetCheckState()>=0)
        return NSAccessibilityCheckBoxRole;
      return NSAccessibilityButtonRole;
    }
    return NSAccessibilityUnknownRole;
  }
  if ([attribute isEqual:NSAccessibilityTitleAttribute] || [attribute isEqual:NSAccessibilityDescriptionAttribute])// || [attribute isEqual:NSAccessibilityValueDescriptionAttribute])
  {
    const char *str=NULL;
    int cs=-1;
    if (!strcmp(type,"vwnd_statictext"))
    {
      WDL_VirtualStaticText *t = (WDL_VirtualStaticText *)m_br->vwnd;
      str = t->GetText();
    }
    if (!strcmp(type,"vwnd_combobox"))
    {
      WDL_VirtualComboBox *cb = (WDL_VirtualComboBox *)m_br->vwnd;
      str = cb->GetItem(cb->GetCurSel());
    }
    if (!strcmp(type,"vwnd_iconbutton")) 
    {
      WDL_VirtualIconButton *b = (WDL_VirtualIconButton *)m_br->vwnd;
      str = b->GetTextLabel();
      cs = b->GetCheckState();
    }
    if (!str || !*str) str= m_br->vwnd->GetAccessDesc();
    else
    {
      const char *p = m_br->vwnd->GetAccessDesc();
      if (p && *p)
      {
        sprintf(buf,"%.512s: %.512s",p,str);
        str=buf;
      }
    }

  
#if 0
    if (cs>=0)
    {
      if (str!=buf)
      {
        lstrcpyn(buf,str?str:"",sizeof(buf)-128);
        str=buf;
      }
//      strcat(buf,cs>0 ? " checked" : " unchecked");
      
    }
#endif
    
    if (str && *str) return [(id)SWELL_CStringToCFString(str) autorelease];
  }
  if ([attribute isEqual:NSAccessibilityWindowAttribute])
  {
    HWND h = m_br->vwnd->GetRealParent();
    if (h)
    {
      return [(NSView *)h window];
    }
  }
  int s;
  if ([attribute isEqual:NSAccessibilityValueDescriptionAttribute])
  {
    if (!strcmp(type,"vwnd_slider")) // eventually we can remove this check
    {
      WDL_VWnd *w = (WDL_VWnd *)m_br->vwnd;
      buf[0]=0;
      if (w->GetAccessValueDesc(buf,sizeof(buf)) && buf[0])
      {
        return [(id)SWELL_CStringToCFString(buf) autorelease];
      }
    }
  }
  if ((s=!![attribute isEqual:NSAccessibilityMaxValueAttribute]) ||
       (s=[attribute isEqual:NSAccessibilityValueAttribute]?2:0) || 
        [attribute isEqual:NSAccessibilityMinValueAttribute])
  {
     if (!strcmp(type,"vwnd_slider"))
     {
       WDL_VirtualSlider *slid = (WDL_VirtualSlider *)m_br->vwnd;
       int v=0;
       if (s!=2) slid->GetRange(s ? NULL : &v, s ? &v :NULL,NULL);
       else v= slid->GetSliderPosition();
       return [NSNumber numberWithInt:v];
     }
     if (!strcmp(type,"vwnd_combobox"))
     {
       int v=0;
       if (s==1) v=((WDL_VirtualComboBox*)m_br->vwnd)->GetCount();
       else if (s==2) v= !!((WDL_VirtualComboBox *)m_br->vwnd)->GetCurSel();
       if (v<0)v=0;
       return [NSNumber numberWithInt:v];
     }
     if (!strcmp(type,"vwnd_iconbutton"))
     {
       int v=0;
       if (s==1) v=1;
       else if (s==2) v= !!((WDL_VirtualIconButton *)m_br->vwnd)->GetCheckState()>0;
       return [NSNumber numberWithInt:v];
     }
  }
  return nil;
}
- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute
{
  {
    const char *type = m_br->vwnd ?  m_br->vwnd->GetType() : NULL;
    if (!type) type="";
   // NSLog(@"accessibilityIsAttributeSettable: %@ %s %p\n",attribute,type,m_br->vwnd);
  }

  if ([attribute isEqual:NSAccessibilityFocusedAttribute]) return YES;
  return false;
}
- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute
{
  {
    const char *type = m_br->vwnd ?  m_br->vwnd->GetType() : NULL;
    if (!type) type="";
    //NSLog(@"accessibilitySetValue: %@ %s %p\n",attribute,type,m_br->vwnd);
  }

  if ([attribute isEqual:NSAccessibilityFocusedAttribute]) 
  {
    if ([value isKindOfClass:[NSNumber class]])
    {
      NSNumber *p = (NSNumber *)value;
      if ([p boolValue]) __focus = m_br->vwnd;
      else if (__focus == m_br->vwnd) __focus=NULL;
    }
  }
}

// parameterized attribute methods
- (NSArray *)accessibilityParameterizedAttributeNames
{
 // {
//    const char *type = m_br->vwnd ?  m_br->vwnd->GetType() : NULL;
//    if (!type) type="";
//    NSLog(@"accessibilityParameterizedAttributeNames: %@ %s %p\n",@"",type,m_br->vwnd);
//  }  
  return [NSArray array];
}
- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
{
  {
    const char *type = m_br->vwnd ?  m_br->vwnd->GetType() : NULL;
    if (!type) type="";
    //NSLog(@"accessibilityAttributeValue: %@ %s %p\n",attribute,type,m_br->vwnd);
  }  
  return nil;
}

// action methods
- (NSArray *)accessibilityActionNames
{
  {
    const char *type = m_br->vwnd ?  m_br->vwnd->GetType() : NULL;
    if (!type) type="";
    //NSLog(@"accessibilityActionNames: %@ %s %p\n",@"",type,m_br->vwnd);
  }  
  NSString *s[32];
  int sidx=0;
  
  const char *type = m_br->vwnd ? m_br->vwnd->GetType() : NULL;
  if (type)
  {
    if (!strcmp(type,"vwnd_combobox") ||
        !strcmp(type,"vwnd_iconbutton") ||
        !strcmp(type,"vwnd_tabctrl_child") ||
        !strcmp(type,"vwnd_statictext") 
        ) s[sidx++] =  NSAccessibilityPressAction;
    
    if (!strcmp(type,"vwnd_slider"))
    {
      s[sidx++] = NSAccessibilityDecrementAction;
      s[sidx++] = NSAccessibilityIncrementAction;
    }
  }
  
  return [NSArray arrayWithObjects:s count:sidx];
}
- (NSString *)accessibilityActionDescription:(NSString *)action
{
  {
    const char *type = m_br->vwnd ?  m_br->vwnd->GetType() : NULL;
    if (!type) type="";
    //NSLog(@"accessibilityActionDescription: %@ %s %p\n",action,type,m_br->vwnd);
  }  
  const char *type = m_br->vwnd ? m_br->vwnd->GetType() : NULL;
  if (type)
  {
    if ([action isEqual:NSAccessibilityPressAction])
    {
      if (!strcmp(type,"vwnd_combobox")) return @"Choose item";
      if (!strcmp(type,"vwnd_iconbutton")) return @"Press button";
      if (!strcmp(type,"vwnd_statictext")) return @"Doubleclick text";
      if (!strcmp(type,"vwnd_tabctrl_child")) return @"Select tab";
    }
    else if (!strcmp(type,"vwnd_slider")) 
    {
      if ([action isEqual:NSAccessibilityDecrementAction]) return @"Decrease value of control";
      else if ([action isEqual:NSAccessibilityIncrementAction])return @"Increase value of control";
    }
  }
  return nil;
}

- (void)accessibilityPerformAction:(NSString *)action
{
  if (m_br->vwnd)
  {
    const char *type =  m_br->vwnd->GetType();
    if (!type) type="";
    
    if ([action isEqual:NSAccessibilityPressAction])
    {
      if (!strcmp(type,"vwnd_statictext")) m_br->vwnd->OnMouseDblClick(0,0);
      else
      {
        m_br->vwnd->OnMouseDown(0,0);
        m_br->vwnd->OnMouseUp(0,0);      
      }
    }
    else if ([action isEqual:NSAccessibilityDecrementAction])
    {
      m_br->vwnd->OnMouseWheel(-100,-100,-1);
    }
    else if ([action isEqual:NSAccessibilityIncrementAction])
    {
      m_br->vwnd->OnMouseWheel(-100,-100,1);
    }
    //NSLog(@"accessibilityPerformAction: %@ %s %p\n",action,type,m_br->vwnd);
  }  
  // todo
}

// Return YES if the UIElement doesn't show up to the outside world - i.e. its parent should return the UIElement's children as its own - cutting the UIElement out. E.g. NSControls are ignored when they are single-celled.
- (BOOL)accessibilityIsIgnored
{
  if (m_br->vwnd)
  {
    if (!m_br->vwnd->IsVisible()) return YES;
    if (m_br->vwnd->GetNumChildren()) 
    {
      const char *type = m_br->vwnd->GetType();
      if (type) if (!strcmp(type,"vwnd_unknown") || strstr(type,"container")) return YES;
    }
    else
    {
      RECT r;
      m_br->vwnd->GetPosition(&r);
      if (r.right <= r.left || r.bottom <= r.top) return YES;
    }
  }
  return NO;
}

// Returns the deepest descendant of the UIElement hierarchy that contains the point. You can assume the point has already been determined to lie within the receiver. Override this method to do deeper hit testing within a UIElement - e.g. a NSMatrix would test its cells. The point is bottom-left relative screen coordinates.
- (id)accessibilityHitTest:(NSPoint)point
{
  {
    const char *type = m_br->vwnd ?  m_br->vwnd->GetType() : NULL;
    if (!type) type="";
//    NSLog(@"accessibilityHitTest: %f,%f %s %p\n",point.x,point.y,type,m_br->vwnd);
  }  
  
  if (m_br->vwnd)
  {
    HWND h = m_br->vwnd->GetRealParent();
    if (h)
    {
      POINT pt = {(int)point.x,(int)point.y};
      ScreenToClient(h,&pt);
      WDL_VWnd *par = m_br->vwnd;
      while (par->GetParent()) par=par->GetParent();
      RECT r;
      par->GetPosition(&r);     
      WDL_VWnd *hit = par->VirtWndFromPoint(pt.x-r.left,pt.y-r.top);
      if (hit)
      {
        VWndNSAccessibility *a = GetVWndNSAccessible(hit);
        if (a) 
        {
          [a autorelease];
          return a;
        }
      }
    }
  }
  return nil;
}
// Returns the UI Element that has the focus. You can assume that the search for the focus has already been narrowed down to the reciever. Override this method to do a deeper search with a UIElement - e.g. a NSMatrix would determine if one of its cells has the focus.
- (id)accessibilityFocusedUIElement
{
  {
    const char *type = m_br->vwnd ?  m_br->vwnd->GetType() : NULL;
    if (!type) type="";
    //NSLog(@"accessibilityFocusedUIElement: %s %p\n",type,m_br->vwnd);
  }  
  if (__focus && m_br && m_br->vwnd && m_br->vwnd->IsDescendent(__focus))
  {
    VWndBridgeNS *p = (VWndBridgeNS *)__focus->GetAccessibilityBridge();
    if (p) return p->par;
  }
  return self;
}


@end



static VWndNSAccessibility *GetVWndNSAccessible(WDL_VWnd *vwnd)
{
  if (!vwnd) return NULL;
  VWndBridgeNS *p = (VWndBridgeNS *)vwnd->GetAccessibilityBridge();
  if (p) 
  {
    if (p->par) [p->par retain];
    return p->par;
  }

  return [[VWndNSAccessibility alloc] initWithVWnd:vwnd];
}

LRESULT WDL_AccessibilityHandleForVWnd(bool isDialog, HWND hwnd, WDL_VWnd *vw, WPARAM wParam, LPARAM lParam)
{
  if (vw && lParam && wParam==0x1001)
  {
    VWndNSAccessibility *nsa = GetVWndNSAccessible(vw);
    if (nsa) *(id *)lParam = nsa;
  }
  return 0;
}
