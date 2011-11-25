// LFntConv.h : main header file for the LFNTCONV application
//

#if !defined(AFX_LFNTCONV_H__85CF98A8_4D06_4DF5_ADB9_E5DAEAA00BA0__INCLUDED_)
#define AFX_LFNTCONV_H__85CF98A8_4D06_4DF5_ADB9_E5DAEAA00BA0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CLFntConvApp:
// See LFntConv.cpp for the implementation of this class
//

class CLFntConvApp : public CWinApp
{
public:
	CLFntConvApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLFntConvApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CLFntConvApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LFNTCONV_H__85CF98A8_4D06_4DF5_ADB9_E5DAEAA00BA0__INCLUDED_)
