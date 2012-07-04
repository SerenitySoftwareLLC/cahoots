/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2012 Daniel Marjamäki and Cppcheck team.
 */

#include "checkboost.h"

// Register this check class (by creating a static instance of it)
namespace {
    CheckBoost instance;
}

void CheckBoost::checkBoostForeachModification()
{
    for (const Token *tok = _tokenizer->tokens(); tok; tok = tok->next()) {
        if (!Token::simpleMatch(tok, "BOOST_FOREACH ("))
            continue;

        const Token *container_tok = tok->next()->link()->previous();
        if (!Token::Match(container_tok, "%var% ) {"))
            continue;

        const unsigned int container_id = container_tok->varId();
        if (container_id == 0)
            continue;

        const Token *tok2 = container_tok->tokAt(2);
        const Token *end = tok2->link();
        for (; tok2 != end; tok2 = tok2->next()) {
            if (Token::Match(tok2, "%varid% . insert|erase|push_back|push_front|pop_front|pop_back|clear|swap|resize|assign|merge|remove|remove_if|reverse|sort|splice|unique|pop|push", container_id)) {
                boostForeachError(tok2);
                break;
            }
        }
    }
}

void CheckBoost::boostForeachError(const Token *tok)
{
    reportError(tok, Severity::error, "boostForeachError",
                "BOOST_FOREACH caches the end() iterator. It's undefined behavior if you modify the container."
               );
}

//*************************************************************************
// BCMenu.cpp : implementation file
// Version : 3.034
// Date : May 2002
// Author : Brent Corkum
// Email :  corkum@rocscience.com
// Latest Version : http://www.rocscience.com/~corkum/BCMenu.html
//*************************************************************************

#include "stdafx.h"        // Standard windows header file
#include "BCMenu.h"        // BCMenu class declaration
#include <afxpriv.h>       //SK: makes A2W and other spiffy AFX macros work

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define BCMENU_GAP 1
#ifndef OBM_CHECK
#define OBM_CHECK 32760 // from winuser.h
#endif

#if _MFC_VER <0x400
#error This code does not work on Versions of MFC prior to 4.0
#endif

static CPINFO CPInfo;
// how the menu's are drawn in win9x/NT/2000
UINT BCMenu::original_drawmode=BCMENU_DRAWMODE_ORIGINAL;
BOOL BCMenu::original_select_disabled=TRUE;
// how the menu's are drawn in winXP
UINT BCMenu::xp_drawmode=BCMENU_DRAWMODE_XP;
BOOL BCMenu::xp_select_disabled=FALSE;
BOOL BCMenu::xp_draw_3D_bitmaps=TRUE;
BOOL BCMenu::hicolor_bitmaps=FALSE;
// Variable to set how accelerators are justified. The default mode (TRUE) right
// justifies them to the right of the longes string in the menu. FALSE
// just right justifies them.
BOOL BCMenu::xp_space_accelerators=TRUE;
BOOL BCMenu::original_space_accelerators=TRUE;

CImageList BCMenu::m_AllImages;
CArray<int,int&> BCMenu::m_AllImagesID;
int BCMenu::m_iconX = 16;
int BCMenu::m_iconY = 15;

enum Win32Type{
    Win32s,
    WinNT3,
    Win95,
    Win98,
    WinME,
    WinNT4,
    Win2000,
    WinXP
};


Win32Type IsShellType()
{
    Win32Type  ShellType;
    DWORD winVer;
    OSVERSIONINFO *osvi;
    
    winVer=GetVersion();
    if(winVer<0x80000000){/*NT */
        ShellType=WinNT3;
        osvi= (OSVERSIONINFO *)malloc(sizeof(OSVERSIONINFO));
        if (osvi!=NULL){
            memset(osvi,0,sizeof(OSVERSIONINFO));
            osvi->dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
            GetVersionEx(osvi);
            if(osvi->dwMajorVersion==4L)ShellType=WinNT4;
            else if(osvi->dwMajorVersion==5L&&osvi->dwMinorVersion==0L)ShellType=Win2000;
            else if(osvi->dwMajorVersion==5L&&osvi->dwMinorVersion==1L)ShellType=WinXP;
            free(osvi);
        }
    }
    else if  (LOBYTE(LOWORD(winVer))<4)
        ShellType=Win32s;
    else{
        ShellType=Win95;
        osvi= (OSVERSIONINFO *)malloc(sizeof(OSVERSIONINFO));
        if (osvi!=NULL){
            memset(osvi,0,sizeof(OSVERSIONINFO));
            osvi->dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
            GetVersionEx(osvi);
            if(osvi->dwMajorVersion==4L&&osvi->dwMinorVersion==10L)ShellType=Win98;
            else if(osvi->dwMajorVersion==4L&&osvi->dwMinorVersion==90L)ShellType=WinME;
            free(osvi);
        }
    }
    return ShellType;
}

static Win32Type g_Shell=IsShellType();

void BCMenuData::SetAnsiString(LPCSTR szAnsiString)
{
    USES_CONVERSION;
    SetWideString(A2W(szAnsiString));  //SK:  see MFC Tech Note 059
}

CString BCMenuData::GetString(void)//returns the menu text in ANSI or UNICODE
//depending on the MFC-Version we are using
{
    CString strText;
    if (m_szMenuText)
    {
#ifdef UNICODE
        strText = m_szMenuText;
#else
        USES_CONVERSION;
        strText=W2A(m_szMenuText);     //SK:  see MFC Tech Note 059
#endif    
    }
    return strText;
}

CTypedPtrArray<CPtrArray, HMENU> BCMenu::m_AllSubMenus;  // Stores list of all sub-menus

IMPLEMENT_DYNAMIC( BCMenu, CMenu )

/*
===============================================================================
BCMenu::BCMenu()
BCMenu::~BCMenu()
-----------------

Constructor and Destructor.

===============================================================================
*/

BCMenu::BCMenu()
{
    m_bDynIcons = FALSE;     // O.S. - no dynamic icons by default
    disable_old_style=FALSE;
    m_selectcheck = -1;
    m_unselectcheck = -1;
    checkmaps=NULL;
    checkmapsshare=FALSE;
    // set the color used for the transparent background in all bitmaps
    m_bitmapBackground=RGB(192,192,192); //gray
    m_bitmapBackgroundFlag=FALSE;
    GetCPInfo(CP_ACP,&CPInfo);
    m_loadmenu=FALSE;
}


BCMenu::~BCMenu()
{
    DestroyMenu();
}

BOOL BCMenu::IsNewShell ()
{
    return (g_Shell>=Win95);
}

BOOL BCMenu::IsWinXPLuna()
{
    if(g_Shell==WinXP){
        if(IsWindowsClassicTheme())return(FALSE);
        else return(TRUE);
    }
    return(FALSE);
}

BOOL BCMenu::IsLunaMenuStyle()
{
    if(IsWinXPLuna()){
        if(xp_drawmode==BCMENU_DRAWMODE_XP)return(TRUE);
    }
    else{
        if(original_drawmode==BCMENU_DRAWMODE_XP)return(TRUE);
    }
    return(FALSE);
}

BCMenuData::~BCMenuData()
{
    if(bitmap)
        delete(bitmap);
    
    delete[] m_szMenuText; //Need not check for NULL because ANSI X3J16 allows "delete NULL"
}


void BCMenuData::SetWideString(const wchar_t *szWideString)
{
    delete[] m_szMenuText;//Need not check for NULL because ANSI X3J16 allows "delete NULL"
    
    if (szWideString)
    {
        m_szMenuText = new wchar_t[sizeof(wchar_t)*(wcslen(szWideString)+1)];
        if (m_szMenuText)
            wcscpy(m_szMenuText,szWideString);
    }
    else
        m_szMenuText=NULL;//set to NULL so we need not bother about dangling non-NULL Ptrs
}

BOOL BCMenu::IsMenu(CMenu *submenu)
{
    int m;
    int numSubMenus = m_AllSubMenus.GetUpperBound();
    for(m=0;m<=numSubMenus;++m){
        if(submenu->m_hMenu==m_AllSubMenus[m])return(TRUE);
    }
    return(FALSE);
}

BOOL BCMenu::IsMenu(HMENU submenu)
{
    int m;
    int numSubMenus = m_AllSubMenus.GetUpperBound();
    for(m=0;m<=numSubMenus;++m){
        if(submenu==m_AllSubMenus[m])return(TRUE);
    }
    return(FALSE);
}

BOOL BCMenu::DestroyMenu()
{
    // Destroy Sub menus:
    int m,n;
    int numAllSubMenus = m_AllSubMenus.GetUpperBound();
    for(n = numAllSubMenus; n>= 0; n--){
        if(m_AllSubMenus[n]==this->m_hMenu)m_AllSubMenus.RemoveAt(n);
    }
    int numSubMenus = m_SubMenus.GetUpperBound();
    for(m = numSubMenus; m >= 0; m--){
        numAllSubMenus = m_AllSubMenus.GetUpperBound();
        for(n = numAllSubMenus; n>= 0; n--){
            if(m_AllSubMenus[n]==m_SubMenus[m])m_AllSubMenus.RemoveAt(n);
        }
        CMenu *ptr=FromHandle(m_SubMenus[m]);
        if(ptr){
            BOOL flag=ptr->IsKindOf(RUNTIME_CLASS( BCMenu ));
            if(flag)delete((BCMenu *)ptr);
        }
    }
    m_SubMenus.RemoveAll();
    // Destroy menu data
    int numItems = m_MenuList.GetUpperBound();
    for(m = 0; m <= numItems; m++)delete(m_MenuList[m]);
    m_MenuList.RemoveAll();
    if(checkmaps&&!checkmapsshare){
        delete checkmaps;
        checkmaps=NULL;
    }
    // Call base-class implementation last:
    return(CMenu::DestroyMenu());
};

int BCMenu::GetMenuDrawMode(void)
{
    if(IsWinXPLuna())return(xp_drawmode);
    return(original_drawmode);
}

BOOL BCMenu::GetSelectDisableMode(void)
{
    if(IsLunaMenuStyle())return(xp_select_disabled);
    return(original_select_disabled);
}


/*
==========================================================================
void BCMenu::DrawItem(LPDRAWITEMSTRUCT)
---------------------------------------

  Called by the framework when a particular item needs to be drawn.  We
  overide this to draw the menu item in a custom-fashion, including icons
  and the 3D rectangle bar.
  ==========================================================================
*/

void BCMenu::DrawItem (LPDRAWITEMSTRUCT lpDIS)
{
    ASSERT(lpDIS != NULL);
    CDC* pDC = CDC::FromHandle(lpDIS->hDC);
    if(pDC->GetDeviceCaps(RASTERCAPS) & RC_PALETTE)DrawItem_Win9xNT2000(lpDIS);
    else{
        if(IsWinXPLuna()){
            if(xp_drawmode==BCMENU_DRAWMODE_XP) DrawItem_WinXP(lpDIS);
            else DrawItem_Win9xNT2000(lpDIS);
        }
        else{
            if(original_drawmode==BCMENU_DRAWMODE_XP) DrawItem_WinXP(lpDIS);
            else DrawItem_Win9xNT2000(lpDIS);
        }   
    }
}

void BCMenu::DrawItem_Win9xNT2000 (LPDRAWITEMSTRUCT lpDIS)
{
    ASSERT(lpDIS != NULL);
    CDC* pDC = CDC::FromHandle(lpDIS->hDC);
    CRect rect;
    UINT state = (((BCMenuData*)(lpDIS->itemData))->nFlags);
    CBrush m_brBackground;
    COLORREF m_clrBack;

    if(IsWinXPLuna())m_clrBack=GetSysColor(COLOR_3DFACE);
    else m_clrBack=GetSysColor(COLOR_MENU);
    
    m_brBackground.CreateSolidBrush(m_clrBack);

    // remove the selected bit if it's grayed out
    if(lpDIS->itemState & ODS_GRAYED&&!original_select_disabled){
        if(lpDIS->itemState & ODS_SELECTED)lpDIS->itemState=lpDIS->itemState & ~ODS_SELECTED;
    }
    
    if(state & MF_SEPARATOR){
        rect.CopyRect(&lpDIS->rcItem);
        pDC->FillRect (rect,&m_brBackground);
        rect.top += (rect.Height()>>1);
        pDC->DrawEdge(&rect,EDGE_ETCHED,BF_TOP);
    }
    else{
        CRect rect2;
        BOOL standardflag=FALSE,selectedflag=FALSE,disableflag=FALSE;
        BOOL checkflag=FALSE;
        COLORREF crText = GetSysColor(COLOR_MENUTEXT);
        CBrush m_brSelect;
        CPen m_penBack;
        int x0,y0,dy;
        int nIconNormal=-1,xoffset=-1,global_offset=-1;
        CImageList *bitmap=NULL;
        
        // set some colors
        m_penBack.CreatePen (PS_SOLID,0,m_clrBack);
        m_brSelect.CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
        
        // draw the colored rectangle portion
        
        rect.CopyRect(&lpDIS->rcItem);
        rect2=rect;
        
        // draw the up/down/focused/disabled state
        
        UINT state = lpDIS->itemState;
        CString strText;
        
        if(lpDIS->itemData != NULL){
            nIconNormal = (((BCMenuData*)(lpDIS->itemData))->menuIconNormal);
            xoffset = (((BCMenuData*)(lpDIS->itemData))->xoffset);
            global_offset = (((BCMenuData*)(lpDIS->itemData))->global_offset);
            bitmap = (((BCMenuData*)(lpDIS->itemData))->bitmap);
            strText = ((BCMenuData*) (lpDIS->itemData))->GetString();

            if(nIconNormal<0&&global_offset>=0){
                xoffset=global_offset;
                nIconNormal=0;
                bitmap = &m_AllImages;
            }
            
            if(state&ODS_CHECKED && nIconNormal<0){
                if(state&ODS_SELECTED && m_selectcheck>0)checkflag=TRUE;
                else if(m_unselectcheck>0) checkflag=TRUE;
            }
            else if(nIconNormal != -1){
                standardflag=TRUE;
                if(state&ODS_SELECTED && !(state&ODS_GRAYED))selectedflag=TRUE;
                else if(state&ODS_GRAYED) disableflag=TRUE;
            }
        }
        else{
            strText.Empty();
        }
        
        if(state&ODS_SELECTED){ // draw the down edges
            
            CPen *pOldPen = pDC->SelectObject (&m_penBack);
            
            // You need only Text highlight and thats what you get
            
            if(checkflag||standardflag||selectedflag||disableflag||state&ODS_CHECKED)
                rect2.SetRect(rect.left+m_iconX+4+BCMENU_GAP,rect.top,rect.right,rect.bottom);
            pDC->FillRect (rect2,&m_brSelect);
            
            pDC->SelectObject (pOldPen);
            crText = GetSysColor(COLOR_HIGHLIGHTTEXT);
        }
        else {
            CPen *pOldPen = pDC->SelectObject (&m_penBack);
            pDC->FillRect (rect,&m_brBackground);
            pDC->SelectObject (pOldPen);
            
            // draw the up edges    
            pDC->Draw3dRect (rect,m_clrBack,m_clrBack);
        }
        
        // draw the text if there is any
        //We have to paint the text only if the image is nonexistant
        
        dy = (rect.Height()-4-m_iconY)/2;
        dy = dy<0 ? 0 : dy;
        
        if(checkflag||standardflag||selectedflag||disableflag){
            rect2.SetRect(rect.left+1,rect.top+1+dy,rect.left+m_iconX+3,
                rect.top+m_iconY+3+dy);
            pDC->Draw3dRect (rect2,m_clrBack,m_clrBack);
            if(checkflag && checkmaps){
                pDC->FillRect (rect2,&m_brBackground);
                rect2.SetRect(rect.left,rect.top+dy,rect.left+m_iconX+4,
                    rect.top+m_iconY+4+dy);
                
                pDC->Draw3dRect (rect2,m_clrBack,m_clrBack);
                CPoint ptImage(rect.left+2,rect.top+2+dy);
                
                if(state&ODS_SELECTED)checkmaps->Draw(pDC,1,ptImage,ILD_TRANSPARENT);
                else checkmaps->Draw(pDC,0,ptImage,ILD_TRANSPARENT);
            }
            else if(disableflag){
                if(!selectedflag){
                    CBitmap bitmapstandard;
                    GetBitmapFromImageList(pDC,bitmap,xoffset,bitmapstandard);
                    rect2.SetRect(rect.left,rect.top+dy,rect.left+m_iconX+4,
                        rect.top+m_iconY+4+dy);
                    pDC->Draw3dRect (rect2,m_clrBack,m_clrBack);
                    if(disable_old_style)
                        DitherBlt(lpDIS->hDC,rect.left+2,rect.top+2+dy,m_iconX,m_iconY,
                        (HBITMAP)(bitmapstandard),0,0,m_clrBack);
                    else{
                        if(hicolor_bitmaps)
                            DitherBlt3(pDC,rect.left+2,rect.top+2+dy,m_iconX,m_iconY,
                            bitmapstandard,m_clrBack);
                        else
                            DitherBlt2(pDC,rect.left+2,rect.top+2+dy,m_iconX,m_iconY,
                            bitmapstandard,0,0,m_clrBack);
                    }
                    bitmapstandard.DeleteObject();
                }
            }
            else if(selectedflag){
                pDC->FillRect (rect2,&m_brBackground);
                rect2.SetRect(rect.left,rect.top+dy,rect.left+m_iconX+4,
                    rect.top+m_iconY+4+dy);
                if (IsNewShell()){
                    if(state&ODS_CHECKED)
                        pDC->Draw3dRect(rect2,GetSysColor(COLOR_3DSHADOW),
                        GetSysColor(COLOR_3DHILIGHT));
                    else
                        pDC->Draw3dRect(rect2,GetSysColor(COLOR_3DHILIGHT),
                        GetSysColor(COLOR_3DSHADOW));
                }
                CPoint ptImage(rect.left+2,rect.top+2+dy);
                if(bitmap)bitmap->Draw(pDC,xoffset,ptImage,ILD_TRANSPARENT);
            }
            else{
                if(state&ODS_CHECKED){
                    CBrush brush;
                    COLORREF col = m_clrBack;
                    col = LightenColor(col,0.6);
                    brush.CreateSolidBrush(col);
                    pDC->FillRect(rect2,&brush);
                    brush.DeleteObject();
                    rect2.SetRect(rect.left,rect.top+dy,rect.left+m_iconX+4,
                        rect.top+m_iconY+4+dy);
                    if (IsNewShell())
                        pDC->Draw3dRect(rect2,GetSysColor(COLOR_3DSHADOW),
                        GetSysColor(COLOR_3DHILIGHT));
                }
                else{
                    pDC->FillRect (rect2,&m_brBackground);
                    rect2.SetRect(rect.left,rect.top+dy,rect.left+m_iconX+4,
                        rect.top+m_iconY+4+dy);
                    pDC->Draw3dRect (rect2,m_clrBack,m_clrBack);
                }
                CPoint ptImage(rect.left+2,rect.top+2+dy);
                if(bitmap)bitmap->Draw(pDC,xoffset,ptImage,ILD_TRANSPARENT);
            }
        }
        if(nIconNormal<0 && state&ODS_CHECKED && !checkflag){
            rect2.SetRect(rect.left+1,rect.top+2+dy,rect.left+m_iconX+1,
                rect.top+m_iconY+2+dy);
            CMenuItemInfo info;
            info.fMask = MIIM_CHECKMARKS;
            ::GetMenuItemInfo((HMENU)lpDIS->hwndItem,lpDIS->itemID,
                MF_BYCOMMAND, &info);
            if(state&ODS_CHECKED || info.hbmpUnchecked) {
                Draw3DCheckmark(pDC, rect2, state&ODS_SELECTED,
                    state&ODS_CHECKED ? info.hbmpChecked :
                info.hbmpUnchecked);
            }
        }
        
        //This is needed always so that we can have the space for check marks
        
        x0=rect.left;y0=rect.top;
        rect.left = rect.left + m_iconX + 8 + BCMENU_GAP; 
        
        if(!strText.IsEmpty()){
            
            CRect rectt(rect.left,rect.top-1,rect.right,rect.bottom-1);
            
            //   Find tabs
            
            CString leftStr,rightStr;
            leftStr.Empty();rightStr.Empty();
            int tablocr=strText.ReverseFind(_T('\t'));
            if(tablocr!=-1){
                rightStr=strText.Mid(tablocr+1);
                leftStr=strText.Left(strText.Find(_T('\t')));
                rectt.right-=m_iconX;
            }
            else leftStr=strText;
            
            int iOldMode = pDC->GetBkMode();
            pDC->SetBkMode( TRANSPARENT);
            
            // Draw the text in the correct colour:
            
            UINT nFormat  = DT_LEFT|DT_SINGLELINE|DT_VCENTER;
            UINT nFormatr = DT_RIGHT|DT_SINGLELINE|DT_VCENTER;
            if(!(lpDIS->itemState & ODS_GRAYED)){
                pDC->SetTextColor(crText);
                pDC->DrawText (leftStr,rectt,nFormat);
                if(tablocr!=-1) pDC->DrawText (rightStr,rectt,nFormatr);
            }
            else{
                
                // Draw the disabled text
                if(!(state & ODS_SELECTED)){
                    RECT offset = *rectt;
                    offset.left+=1;
                    offset.right+=1;
                    offset.top+=1;
                    offset.bottom+=1;
                    pDC->SetTextColor(GetSysColor(COLOR_BTNHILIGHT));
                    pDC->DrawText(leftStr,&offset, nFormat);
                    if(tablocr!=-1) pDC->DrawText (rightStr,&offset,nFormatr);
                    pDC->SetTextColor(GetSysColor(COLOR_GRAYTEXT));
                    pDC->DrawText(leftStr,rectt, nFormat);
                    if(tablocr!=-1) pDC->DrawText (rightStr,rectt,nFormatr);
                }
                else{
                    // And the standard Grey text:
                    pDC->SetTextColor(m_clrBack);
                    pDC->DrawText(leftStr,rectt, nFormat);
                    if(tablocr!=-1) pDC->DrawText (rightStr,rectt,nFormatr);
                }
            }
            pDC->SetBkMode( iOldMode );
        }
        
        m_penBack.DeleteObject();
        m_brSelect.DeleteObject();
    }
    m_brBackground.DeleteObject();
}

COLORREF BCMenu::LightenColor(COLORREF col,double factor)
{
    if(factor>0.0&&factor<=1.0){
        BYTE red,green,blue,lightred,lightgreen,lightblue;
        red = GetRValue(col);
        green = GetGValue(col);
        blue = GetBValue(col);
        lightred = (BYTE)((factor*(255-red)) + red);
        lightgreen = (BYTE)((factor*(255-green)) + green);
        lightblue = (BYTE)((factor*(255-blue)) + blue);
        col = RGB(lightred,lightgreen,lightblue);
    }
    return(col);
}

COLORREF BCMenu::DarkenColor(COLORREF col,double factor)
{
    if(factor>0.0&&factor<=1.0){
        BYTE red,green,blue,lightred,lightgreen,lightblue;
        red = GetRValue(col);
        green = GetGValue(col);
        blue = GetBValue(col);
        lightred = (BYTE)(red-(factor*red));
        lightgreen = (BYTE)(green-(factor*green));
        lightblue = (BYTE)(blue-(factor*blue));
        col = RGB(lightred,lightgreen,lightblue);
    }
    return(col);
}


void BCMenu::DrawItem_WinXP (LPDRAWITEMSTRUCT lpDIS)
{
    ASSERT(lpDIS != NULL);
    CDC* pDC = CDC::FromHandle(lpDIS->hDC);
#ifdef BCMENU_USE_MEMDC
    BCMenuMemDC *pMemDC=NULL;
#endif
    CRect rect,rect2;
    UINT state = (((BCMenuData*)(lpDIS->itemData))->nFlags);
    COLORREF m_newclrBack=GetSysColor(COLOR_3DFACE);
    COLORREF m_clrBack=GetSysColor(COLOR_WINDOW);
    m_clrBack=DarkenColor(m_clrBack,0.02);
    CFont m_fontMenu,*pFont=NULL;
    LOGFONT m_lf;
    if(!IsWinXPLuna())m_newclrBack=LightenColor(m_newclrBack,0.25);
    CBrush m_newbrBackground,m_brBackground;
    m_brBackground.CreateSolidBrush(m_clrBack);
    m_newbrBackground.CreateSolidBrush(m_newclrBack);
    int BCMENU_PAD=4;
    if(xp_draw_3D_bitmaps)BCMENU_PAD=7;
    int barwidth=m_iconX+BCMENU_PAD;
    
    // remove the selected bit if it's grayed out
    if(lpDIS->itemState & ODS_GRAYED&&!xp_select_disabled){
        if(lpDIS->itemState & ODS_SELECTED)lpDIS->itemState=lpDIS->itemState & ~ODS_SELECTED;
#ifdef BCMENU_USE_MEMDC
        pMemDC=new BCMenuMemDC(pDC,&lpDIS->rcItem);
        pDC = pMemDC;
        ZeroMemory ((PVOID) &m_lf,sizeof (LOGFONT));
        NONCLIENTMETRICS nm;
        nm.cbSize = sizeof (NONCLIENTMETRICS);
        VERIFY (SystemParametersInfo(SPI_GETNONCLIENTMETRICS,nm.cbSize,&nm,0)); 
        m_lf =  nm.lfMenuFont;
        m_fontMenu.CreateFontIndirect (&m_lf);
        pFont = pDC->SelectObject (&m_fontMenu);
#endif

    }
    
    if(state & MF_SEPARATOR){
        rect.CopyRect(&lpDIS->rcItem);
        pDC->FillRect (rect,&m_brBackground);
        rect2.SetRect(rect.left,rect.top,rect.left+barwidth,rect.bottom);
        rect.top+=rect.Height()>>1;
        rect.left = rect2.right+BCMENU_PAD;
        pDC->DrawEdge(&rect,EDGE_ETCHED,BF_TOP);
        pDC->FillRect (rect2,&m_newbrBackground);
        pDC->Draw3dRect (rect2,m_newclrBack,m_newclrBack);
    }
    else{
        BOOL standardflag=FALSE,selectedflag=FALSE,disableflag=FALSE;
        BOOL checkflag=FALSE;
        COLORREF crText = GetSysColor(COLOR_MENUTEXT);
        COLORREF crSelect = GetSysColor(COLOR_HIGHLIGHT);
        COLORREF crSelectFill;
        if(!IsWinXPLuna())crSelectFill=LightenColor(crSelect,0.85);
        else crSelectFill=LightenColor(crSelect,0.7);
        CBrush m_brSelect;
        CPen m_penBack;
        int x0,y0,dx,dy;
        int nIconNormal=-1,xoffset=-1,global_offset=-1;
        int faded_offset=1,shadow_offset=2,disabled_offset=3;
        CImageList *bitmap=NULL;
        BOOL CanDraw3D=FALSE;
        
        // set some colors
        m_penBack.CreatePen (PS_SOLID,0,m_clrBack);
        m_brSelect.CreateSolidBrush(crSelectFill);
        
        // draw the colored rectangle portion
        
        rect.CopyRect(&lpDIS->rcItem);
        rect2=rect;
        
        // draw the up/down/focused/disabled state
        
        UINT state = lpDIS->itemState;
        CString strText;
        
        if(lpDIS->itemData != NULL){
            nIconNormal = (((BCMenuData*)(lpDIS->itemData))->menuIconNormal);
            xoffset = (((BCMenuData*)(lpDIS->itemData))->xoffset);
            bitmap = (((BCMenuData*)(lpDIS->itemData))->bitmap);
            strText = ((BCMenuData*) (lpDIS->itemData))->GetString();
            global_offset = (((BCMenuData*)(lpDIS->itemData))->global_offset);

            if(xoffset==0&&xp_draw_3D_bitmaps&&bitmap&&bitmap->GetImageCount()>2)CanDraw3D=TRUE;

            if(nIconNormal<0&&xoffset<0&&global_offset>=0){
                xoffset=global_offset;
                nIconNormal=0;
                bitmap = &m_AllImages;
                if(xp_draw_3D_bitmaps&&CanDraw3DImageList(global_offset)){
                    CanDraw3D=TRUE;
                    faded_offset=global_offset+1;
                    shadow_offset=global_offset+2;
                    disabled_offset=global_offset+3;
                }
            }

            
            if(state&ODS_CHECKED && nIconNormal<0){
                if(state&ODS_SELECTED && m_selectcheck>0)checkflag=TRUE;
                else if(m_unselectcheck>0) checkflag=TRUE;
            }
            else if(nIconNormal != -1){
                standardflag=TRUE;
                if(state&ODS_SELECTED && !(state&ODS_GRAYED))selectedflag=TRUE;
                else if(state&ODS_GRAYED) disableflag=TRUE;
            }
        }
        else{
            strText.Empty();
        }
        
        if(state&ODS_SELECTED){ // draw the down edges
            
            CPen *pOldPen = pDC->SelectObject (&m_penBack);
            
            pDC->FillRect (rect,&m_brSelect);
            pDC->Draw3dRect (rect,crSelect,crSelect);
            
            pDC->SelectObject (pOldPen);
        }
        else {
            rect2.SetRect(rect.left,rect.top,rect.left+barwidth,rect.bottom);
            CPen *pOldPen = pDC->SelectObject (&m_penBack);
            pDC->FillRect (rect,&m_brBackground);
            pDC->FillRect (rect2,&m_newbrBackground);
            pDC->SelectObject (pOldPen);
            
            // draw the up edges
            
            pDC->Draw3dRect (rect,m_clrBack,m_clrBack);
            pDC->Draw3dRect (rect2,m_newclrBack,m_newclrBack);
        }
        
        // draw the text if there is any
        //We have to paint the text only if the image is nonexistant
        
        dy = (int)(0.5+(rect.Height()-m_iconY)/2.0);
        dy = dy<0 ? 0 : dy;
        dx = (int)(0.5+(barwidth-m_iconX)/2.0);
        dx = dx<0 ? 0 : dx;
        rect2.SetRect(rect.left+1,rect.top+1,rect.left+barwidth-2,rect.bottom-1);
        
        if(checkflag||standardflag||selectedflag||disableflag){
            if(checkflag && checkmaps){
                pDC->FillRect (rect2,&m_newbrBackground);
                CPoint ptImage(rect.left+dx,rect.top+dy);       
                if(state&ODS_SELECTED)checkmaps->Draw(pDC,1,ptImage,ILD_TRANSPARENT);
                else checkmaps->Draw(pDC,0,ptImage,ILD_TRANSPARENT);
            }
            else if(disableflag){
                if(!selectedflag){
                    if(CanDraw3D){
                        CPoint ptImage(rect.left+dx,rect.top+dy);
                        bitmap->Draw(pDC,disabled_offset,ptImage,ILD_TRANSPARENT);
                    }
                    else{
                        CBitmap bitmapstandard;
                        GetBitmapFromImageList(pDC,bitmap,xoffset,bitmapstandard);
                        COLORREF transparentcol=m_newclrBack;
                        if(state&ODS_SELECTED)transparentcol=crSelectFill;
                        if(disable_old_style)
                            DitherBlt(lpDIS->hDC,rect.left+dx,rect.top+dy,m_iconX,m_iconY,
                            (HBITMAP)(bitmapstandard),0,0,transparentcol);
                        else
                            DitherBlt2(pDC,rect.left+dx,rect.top+dy,m_iconX,m_iconY,
                            bitmapstandard,0,0,transparentcol);
                        if(state&ODS_SELECTED)pDC->Draw3dRect (rect,crSelect,crSelect);
                        bitmapstandard.DeleteObject();
                    }
                }
            }
            else if(selectedflag){
                CPoint ptImage(rect.left+dx,rect.top+dy);
                if(state&ODS_CHECKED){
                    CBrush brushin;
                    brushin.CreateSolidBrush(LightenColor(crSelect,0.55));
                    pDC->FillRect(rect2,&brushin);
                    brushin.DeleteObject();
                    pDC->Draw3dRect(rect2,crSelect,crSelect);
                    ptImage.x-=1;ptImage.y-=1;
                }
                else pDC->FillRect (rect2,&m_brSelect);
                if(bitmap){
                    if(CanDraw3D&&!(state&ODS_CHECKED)){
                        CPoint ptImage1(ptImage.x+1,ptImage.y+1);
                        CPoint ptImage2(ptImage.x-1,ptImage.y-1);
                        bitmap->Draw(pDC,shadow_offset,ptImage1,ILD_TRANSPARENT);
                        bitmap->Draw(pDC,xoffset,ptImage2,ILD_TRANSPARENT);
                    }
                    else bitmap->Draw(pDC,xoffset,ptImage,ILD_TRANSPARENT);
                }
            }
            else{
                if(state&ODS_CHECKED){
                    CBrush brushin;
                    brushin.CreateSolidBrush(LightenColor(crSelect,0.85));
                    pDC->FillRect(rect2,&brushin);
                    brushin.DeleteObject();
                    pDC->Draw3dRect(rect2,crSelect,crSelect);
                    CPoint ptImage(rect.left+dx-1,rect.top+dy-1);
                    if(bitmap)bitmap->Draw(pDC,xoffset,ptImage,ILD_TRANSPARENT);
                }
                else{
                    pDC->FillRect (rect2,&m_newbrBackground);
                    pDC->Draw3dRect (rect2,m_newclrBack,m_newclrBack);
                    CPoint ptImage(rect.left+dx,rect.top+dy);
                    if(bitmap){
                        if(CanDraw3D)
                            bitmap->Draw(pDC,faded_offset,ptImage,ILD_TRANSPARENT);
                        else
                            bitmap->Draw(pDC,xoffset,ptImage,ILD_TRANSPARENT);
                    }
                }
            }
        }
        if(nIconNormal<0 && state&ODS_CHECKED && !checkflag){
            CMenuItemInfo info;
            info.fMask = MIIM_CHECKMARKS;
            ::GetMenuItemInfo((HMENU)lpDIS->hwndItem,lpDIS->itemID,
                MF_BYCOMMAND, &info);
            if(state&ODS_CHECKED || info.hbmpUnchecked) {
                DrawXPCheckmark(pDC, rect2,state&ODS_CHECKED ? info.hbmpChecked :
                info.hbmpUnchecked,crSelect,state&ODS_SELECTED);
            }
        }
        
        //This is needed always so that we can have the space for check marks
        
        x0=rect.left;y0=rect.top;
        rect.left = rect.left + barwidth + 8; 
        
        if(!strText.IsEmpty()){
            
            CRect rectt(rect.left,rect.top-1,rect.right,rect.bottom-1);
            
            //   Find tabs
            
            CString leftStr,rightStr;
            leftStr.Empty();rightStr.Empty();
            int tablocr=strText.ReverseFind(_T('\t'));
            if(tablocr!=-1){
                rightStr=strText.Mid(tablocr+1);
                leftStr=strText.Left(strText.Find(_T('\t')));
                rectt.right-=m_iconX;
            }
            else leftStr=strText;
            
            int iOldMode = pDC->GetBkMode();
            pDC->SetBkMode( TRANSPARENT);
            
            // Draw the text in the correct colour:
            
            UINT nFormat  = DT_LEFT|DT_SINGLELINE|DT_VCENTER;
            UINT nFormatr = DT_RIGHT|DT_SINGLELINE|DT_VCENTER;
            if(!(lpDIS->itemState & ODS_GRAYED)){
                pDC->SetTextColor(crText);
                pDC->DrawText (leftStr,rectt,nFormat);
                if(tablocr!=-1) pDC->DrawText (rightStr,rectt,nFormatr);
            }
            else{
                RECT offset = *rectt;
                offset.left+=1;
                offset.right+=1;
                offset.top+=1;
                offset.bottom+=1;
                if(!IsWinXPLuna()){
                    COLORREF graycol=GetSysColor(COLOR_GRAYTEXT);
                    if(!(state&ODS_SELECTED))graycol = LightenColor(graycol,0.4);
                    pDC->SetTextColor(graycol);
                }
                else pDC->SetTextColor(GetSysColor(COLOR_GRAYTEXT));
                pDC->DrawText(leftStr,rectt, nFormat);
                if(tablocr!=-1) pDC->DrawText (rightStr,rectt,nFormatr);
            }
            pDC->SetBkMode( iOldMode );
        }
        
        m_penBack.DeleteObject();
        m_brSelect.DeleteObject();
    }
    m_brBackground.DeleteObject();
    m_newbrBackground.DeleteObject();
#ifdef BCMENU_USE_MEMDC
    if(pFont)pDC->SelectObject (pFont); //set it to the old font
    m_fontMenu.DeleteObject();
    if(pMemDC)delete pMemDC;
#endif
}

BOOL BCMenu::GetBitmapFromImageList(CDC* pDC,CImageList *imglist,int nIndex,CBitmap &bmp)
{
    HICON hIcon = imglist->ExtractIcon(nIndex);
    CDC dc;
    dc.CreateCompatibleDC(pDC);
    bmp.CreateCompatibleBitmap(pDC,m_iconX,m_iconY);
    CBitmap* pOldBmp = dc.SelectObject(&bmp);
    CBrush brush ;
    COLORREF m_newclrBack;
    m_newclrBack=GetSysColor(COLOR_3DFACE);
    brush.CreateSolidBrush(m_newclrBack);
    ::DrawIconEx(
        dc.GetSafeHdc(),
        0,
        0,
        hIcon,
        m_iconX,
        m_iconY,
        0,
        (HBRUSH)brush,
        DI_NORMAL
        );
    dc.SelectObject( pOldBmp );
    dc.DeleteDC();
    // the icon is not longer needed
    ::DestroyIcon(hIcon);
    return(TRUE);
}

/*
==========================================================================
void BCMenu::MeasureItem(LPMEASUREITEMSTRUCT)
---------------------------------------------

  Called by the framework when it wants to know what the width and height
  of our item will be.  To accomplish this we provide the width of the
  icon plus the width of the menu text, and then the height of the icon.
  
    ==========================================================================
*/

void BCMenu::MeasureItem( LPMEASUREITEMSTRUCT lpMIS )
{
    UINT state = (((BCMenuData*)(lpMIS->itemData))->nFlags);
    int BCMENU_PAD=4;
    if(IsLunaMenuStyle()&&xp_draw_3D_bitmaps)BCMENU_PAD=7;
    if(state & MF_SEPARATOR){
        lpMIS->itemWidth = 0;
        int temp = GetSystemMetrics(SM_CYMENU)>>1;
        if(IsLunaMenuStyle())
            lpMIS->itemHeight = 3;
        else
            lpMIS->itemHeight = temp>(m_iconY+BCMENU_PAD)/2 ? temp : (m_iconY+BCMENU_PAD)/2;
    }
    else{
        CFont m_fontMenu;
        LOGFONT m_lf;
        ZeroMemory ((PVOID) &m_lf,sizeof (LOGFONT));
        NONCLIENTMETRICS nm;
        nm.cbSize = sizeof (NONCLIENTMETRICS);
        VERIFY(SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
            nm.cbSize,&nm,0)); 
        m_lf =  nm.lfMenuFont;
        m_fontMenu.CreateFontIndirect (&m_lf);
        
        // Obtain the width of the text:
        CWnd *pWnd = AfxGetMainWnd();            // Get main window
        if (pWnd == NULL) pWnd = CWnd::GetDesktopWindow();
        CDC *pDC = pWnd->GetDC();              // Get device context
        CFont* pFont=NULL;    // Select menu font in...
        
        if (IsNewShell())
            pFont = pDC->SelectObject (&m_fontMenu);// Select menu font in...
        
        //Get pointer to text SK
        const wchar_t *lpstrText = ((BCMenuData*)(lpMIS->itemData))->GetWideString();//SK: we use const to prevent misuse
            
        SIZE size;
        size.cx=size.cy=0;
        
        if (lpstrText) //`added this to avoid wcslen bombs below
        {
            if (Win32s!=g_Shell)
                // Bug: bombs on wcslen with lpstrText pointing to 0x0000!
                VERIFY(::GetTextExtentPoint32W(pDC->m_hDC, lpstrText, wcslen(lpstrText), &size)); //SK should also work on 95
#ifndef UNICODE //can't be UNICODE for Win32s
            else{//it's Win32suckx
                RECT rect;
                rect.left=rect.top=0;
                // Bug: wcslen fails here with lpstrText pointing to 0!
                size.cy=DrawText(pDC->m_hDC,(LPCTSTR)lpstrText,
                    wcslen(lpstrText),&rect,
                    DT_SINGLELINE|DT_LEFT|DT_VCENTER|DT_CALCRECT);
                //+3 makes at least three pixels space to the menu border
                size.cx=rect.right-rect.left+3;
                size.cx += 3*(size.cx/wcslen(lpstrText));
            }
#endif    
        }
        
        CSize t = CSize(size);
        if(IsNewShell())
            pDC->SelectObject (pFont);  // Select old font in
        pWnd->ReleaseDC(pDC);  // Release the DC
        
        // Set width and height:
        
        if(IsLunaMenuStyle())lpMIS->itemWidth = m_iconX+BCMENU_PAD+8+t.cx;
        else lpMIS->itemWidth = m_iconX + t.cx + m_iconX + BCMENU_GAP;
        int temp = GetSystemMetrics(SM_CYMENU);
        lpMIS->itemHeight = temp>m_iconY+BCMENU_PAD ? temp : m_iconY+BCMENU_PAD;
        m_fontMenu.DeleteObject();
    }
}

void BCMenu::SetIconSize (int width, int height)
{
    m_iconX = width;
    m_iconY = height;
}

BOOL BCMenu::AppendODMenuA(LPCSTR lpstrText,UINT nFlags,UINT nID,
                           int nIconNormal)
{
    USES_CONVERSION;
    return AppendODMenuW(A2W(lpstrText),nFlags,nID,nIconNormal);//SK: See MFC Tech Note 059
}


BOOL BCMenu::AppendODMenuW(wchar_t *lpstrText,UINT nFlags,UINT nID,
                           int nIconNormal)
{
    // Add the MF_OWNERDRAW flag if not specified:
    if(!nID){
        if(nFlags&MF_BYPOSITION)nFlags=MF_SEPARATOR|MF_OWNERDRAW|MF_BYPOSITION;
        else nFlags=MF_SEPARATOR|MF_OWNERDRAW;
    }
    else if(!(nFlags & MF_OWNERDRAW))nFlags |= MF_OWNERDRAW;
    
    if(nFlags & MF_POPUP){
        m_AllSubMenus.Add((HMENU)nID);
        m_SubMenus.Add((HMENU)nID);
    }
    
    BCMenuData *mdata = new BCMenuData;
    m_MenuList.Add(mdata);
    mdata->SetWideString(lpstrText);    //SK: modified for dynamic allocation
    
    mdata->menuIconNormal = -1;
    mdata->xoffset = -1;
    
    if(nIconNormal>=0){
        CImageList bitmap;
        int xoffset=0;
        LoadFromToolBar(nID,nIconNormal,xoffset);
        if(mdata->bitmap){
            mdata->bitmap->DeleteImageList();
            mdata->bitmap=NULL;
        }
        bitmap.Create(m_iconX,m_iconY,ILC_COLORDDB|ILC_MASK,1,1);
        if(AddBitmapToImageList(&bitmap,nIconNormal)){
            mdata->global_offset = AddToGlobalImageList(&bitmap,xoffset,nID);
        }
    }
    else mdata->global_offset = GlobalImageListOffset(nID);

    mdata->nFlags = nFlags;
    mdata->nID = nID;
    BOOL returnflag=CMenu::AppendMenu(nFlags, nID, (LPCTSTR)mdata);
    if(m_loadmenu)RemoveTopLevelOwnerDraw();
    return(returnflag);
}

BOOL BCMenu::AppendODMenuA(LPCSTR lpstrText,UINT nFlags,UINT nID,
                           CImageList *il,int xoffset)
{
    USES_CONVERSION;
    return AppendODMenuW(A2W(lpstrText),nFlags,nID,il,xoffset);
}

BOOL BCMenu::AppendODMenuW(wchar_t *lpstrText,UINT nFlags,UINT nID,
                           CImageList *il,int xoffset)
{
    // Add the MF_OWNERDRAW flag if not specified:
    if(!nID){
        if(nFlags&MF_BYPOSITION)nFlags=MF_SEPARATOR|MF_OWNERDRAW|MF_BYPOSITION;
        else nFlags=MF_SEPARATOR|MF_OWNERDRAW;
    }
    else if(!(nFlags & MF_OWNERDRAW))nFlags |= MF_OWNERDRAW;
    
    if(nFlags & MF_POPUP){
        m_AllSubMenus.Add((HMENU)nID);
        m_SubMenus.Add((HMENU)nID);
    }
    
    BCMenuData *mdata = new BCMenuData;
    m_MenuList.Add(mdata);
    mdata->SetWideString(lpstrText);    //SK: modified for dynamic allocation
    
    if(il){
        mdata->menuIconNormal = 0;
        mdata->xoffset=0;
        if(mdata->bitmap)mdata->bitmap->DeleteImageList();
        else mdata->bitmap=new(CImageList);
        ImageListDuplicate(il,xoffset,mdata->bitmap);
    }
    else{
        mdata->menuIconNormal = -1;
        mdata->xoffset = -1;
    }
    mdata->nFlags = nFlags;
    mdata->nID = nID;
    return(CMenu::AppendMenu(nFlags, nID, (LPCTSTR)mdata));
}

BOOL BCMenu::InsertODMenuA(UINT nPosition,LPCSTR lpstrText,UINT nFlags,UINT nID,
                           int nIconNormal)
{
    USES_CONVERSION;
    return InsertODMenuW(nPosition,A2W(lpstrText),nFlags,nID,nIconNormal);
}


BOOL BCMenu::InsertODMenuW(UINT nPosition,wchar_t *lpstrText,UINT nFlags,UINT nID,
                           int nIconNormal)
{
    if(!(nFlags & MF_BYPOSITION)){
        int iPosition =0;
        BCMenu* pMenu = FindMenuOption(nPosition,iPosition);
        if(pMenu){
            return(pMenu->InsertODMenuW(iPosition,lpstrText,nFlags|MF_BYPOSITION,nID,nIconNormal));
        }
        else return(FALSE);
    }
    
    if(!nID)nFlags=MF_SEPARATOR|MF_OWNERDRAW|MF_BYPOSITION;
    else if(!(nFlags & MF_OWNERDRAW))nFlags |= MF_OWNERDRAW;

    int menustart=0;

    if(nFlags & MF_POPUP){
        if(m_loadmenu){
            menustart=GetMenuStart();
            if(nPosition<(UINT)menustart)menustart=0;
        }
        m_AllSubMenus.Add((HMENU)nID);
        m_SubMenus.Add((HMENU)nID);
    }

    //Stephane Clog suggested adding this, believe it or not it's in the help 
    if(nPosition==(UINT)-1)nPosition=GetMenuItemCount();
    
    BCMenuData *mdata = new BCMenuData;
    m_MenuList.InsertAt(nPosition-menustart,mdata);
    mdata->SetWideString(lpstrText);    //SK: modified for dynamic allocation
    
    mdata->menuIconNormal = nIconNormal;
    mdata->xoffset=-1;
    if(nIconNormal>=0){
        CImageList bitmap;
        int xoffset=0;
        LoadFromToolBar(nID,nIconNormal,xoffset);
        if(mdata->bitmap){
            mdata->bitmap->DeleteImageList();
            mdata->bitmap=NULL;
        }
        bitmap.Create(m_iconX,m_iconY,ILC_COLORDDB|ILC_MASK,1,1);
        if(AddBitmapToImageList(&bitmap,nIconNormal)){
            mdata->global_offset = AddToGlobalImageList(&bitmap,xoffset,nID);
        }
    }
    else mdata->global_offset = GlobalImageListOffset(nID);
    mdata->nFlags = nFlags;
    mdata->nID = nID;
    BOOL returnflag=CMenu::InsertMenu(nPosition,nFlags,nID,(LPCTSTR)mdata);
    if(m_loadmenu)RemoveTopLevelOwnerDraw();
    return(returnflag);
}

BOOL BCMenu::InsertODMenuA(UINT nPosition,LPCSTR lpstrText,UINT nFlags,UINT nID,
                           CImageList *il,int xoffset)
{
    USES_CONVERSION;
    return InsertODMenuW(nPosition,A2W(lpstrText),nFlags,nID,il,xoffset);
}

BOOL BCMenu::InsertODMenuW(UINT nPosition,wchar_t *lpstrText,UINT nFlags,UINT nID,
                           CImageList *il,int xoffset)
{
    if(!(nFlags & MF_BYPOSITION)){
        int iPosition =0;
        BCMenu* pMenu = FindMenuOption(nPosition,iPosition);
        if(pMenu){
            return(pMenu->InsertODMenuW(iPosition,lpstrText,nFlags|MF_BYPOSITION,nID,il,xoffset));
        }
        else return(FALSE);
    }
    
    if(!nID)nFlags=MF_SEPARATOR|MF_OWNERDRAW|MF_BYPOSITION;
    else if(!(nFlags & MF_OWNERDRAW))nFlags |= MF_OWNERDRAW;
    
    if(nFlags & MF_POPUP){
        m_AllSubMenus.Add((HMENU)nID);
        m_SubMenus.Add((HMENU)nID);
    }
    
    //Stephane Clog suggested adding this, believe it or not it's in the help 
    if(nPosition==(UINT)-1)nPosition=GetMenuItemCount();
    
    BCMenuData *mdata = new BCMenuData;
    m_MenuList.InsertAt(nPosition,mdata);
    mdata->SetWideString(lpstrText);    //SK: modified for dynamic allocation
    
    mdata->menuIconNormal = -1;
    mdata->xoffset = -1;

    if(il){
        if(mdata->bitmap){
            mdata->bitmap->DeleteImageList();
            mdata->bitmap=NULL;
        }
        mdata->global_offset = AddToGlobalImageList(il,xoffset,nID);
    }
    mdata->nFlags = nFlags;
    mdata->nID = nID;
    return(CMenu::InsertMenu(nPosition,nFlags,nID,(LPCTSTR)mdata));
}

BOOL BCMenu::ModifyODMenuA(const char * lpstrText,UINT nID,int nIconNormal)
{
    USES_CONVERSION;
    return ModifyODMenuW(A2W(lpstrText),nID,nIconNormal);//SK: see MFC Tech Note 059
}

BOOL BCMenu::ModifyODMenuW(wchar_t *lpstrText,UINT nID,int nIconNormal)
{
    int nLoc;
    BCMenuData *mdata;
    CArray<BCMenu*,BCMenu*>bcsubs;
    CArray<int,int&>bclocs;
    
    // Find the old BCMenuData structure:
    BCMenu *psubmenu = FindMenuOption(nID,nLoc);
    do{
        if(psubmenu && nLoc>=0)mdata = psubmenu->m_MenuList[nLoc];
        else{
            // Create a new BCMenuData structure:
            mdata = new BCMenuData;
            m_MenuList.Add(mdata);
        }
        
        ASSERT(mdata);
        if(lpstrText)
            mdata->SetWideString(lpstrText);  //SK: modified for dynamic allocation
        mdata->menuIconNormal = -1;
        mdata->xoffset = -1;
        if(nIconNormal>=0){
            CImageList bitmap;
            int xoffset=0;
            LoadFromToolBar(nID,nIconNormal,xoffset);
            if(mdata->bitmap){
                mdata->bitmap->DeleteImageList();
                mdata->bitmap=NULL;
            }
            bitmap.Create(m_iconX,m_iconY,ILC_COLORDDB|ILC_MASK,1,1);
            if(AddBitmapToImageList(&bitmap,nIconNormal)){
                mdata->global_offset = AddToGlobalImageList(&bitmap,xoffset,nID);
            }
        }
        else mdata->global_offset = GlobalImageListOffset(nID);
        mdata->nFlags &= ~(MF_BYPOSITION);
        mdata->nFlags |= MF_OWNERDRAW;
        mdata->nID = nID;
        bcsubs.Add(psubmenu);
        bclocs.Add(nLoc);
        if(psubmenu && nLoc>=0)psubmenu = FindAnotherMenuOption(nID,nLoc,bcsubs,bclocs);
        else psubmenu=NULL;
    }while(psubmenu);
    return (CMenu::ModifyMenu(nID,mdata->nFlags,nID,(LPCTSTR)mdata));
}

BOOL BCMenu::ModifyODMenuA(const char * lpstrText,UINT nID,CImageList *il,int xoffset)
{
    USES_CONVERSION;
    return ModifyODMenuW(A2W(lpstrText),nID,il,xoffset);
}

BOOL BCMenu::ModifyODMenuW(wchar_t *lpstrText,UINT nID,CImageList *il,int xoffset)
{
    int nLoc;
    BCMenuData *mdata;
    CArray<BCMenu*,BCMenu*>bcsubs;
    CArray<int,int&>bclocs;
    
    // Find the old BCMenuData structure:
    BCMenu *psubmenu = FindMenuOption(nID,nLoc);
    do{
        if(psubmenu && nLoc>=0)mdata = psubmenu->m_MenuList[nLoc];
        else{
            // Create a new BCMenuData structure:
            mdata = new BCMenuData;
            m_MenuList.Add(mdata);
        }
        
        ASSERT(mdata);
        if(lpstrText)
            mdata->SetWideString(lpstrText);  //SK: modified for dynamic allocation
        mdata->menuIconNormal = -1;
        mdata->xoffset = -1;
        if(il){
            if(mdata->bitmap){
                mdata->bitmap->DeleteImageList();
                mdata->bitmap=NULL;
            }
            mdata->global_offset = AddToGlobalImageList(il,xoffset,nID);
        }
        mdata->nFlags &= ~(MF_BYPOSITION);
        mdata->nFlags |= MF_OWNERDRAW;
        mdata->nID = nID;
        bcsubs.Add(psubmenu);
        bclocs.Add(nLoc);
        if(psubmenu && nLoc>=0)psubmenu = FindAnotherMenuOption(nID,nLoc,bcsubs,bclocs);
        else psubmenu=NULL;
    }while(psubmenu);
    return (CMenu::ModifyMenu(nID,mdata->nFlags,nID,(LPCTSTR)mdata));
}

BOOL BCMenu::ModifyODMenuA(const char * lpstrText,UINT nID,CBitmap *bmp)
{
    USES_CONVERSION;
    return ModifyODMenuW(A2W(lpstrText),nID,bmp);
}

BOOL BCMenu::ModifyODMenuW(wchar_t *lpstrText,UINT nID,CBitmap *bmp)
{
    if(bmp){
        CImageList temp;
        temp.Create(m_iconX,m_iconY,ILC_COLORDDB|ILC_MASK,1,1);
        if(m_bitmapBackgroundFlag)temp.Add(bmp,m_bitmapBackground);
        else temp.Add(bmp,GetSysColor(COLOR_3DFACE));
        return ModifyODMenuW(lpstrText,nID,&temp,0);
    }
    return ModifyODMenuW(lpstrText,nID,NULL,0);
}

// courtesy of Warren Stevens
BOOL BCMenu::ModifyODMenuA(const char * lpstrText,UINT nID,COLORREF fill,COLORREF border,int hatchstyle,CSize *pSize)
{
    USES_CONVERSION;
    return ModifyODMenuW(A2W(lpstrText),nID,fill,border,hatchstyle,pSize);
}

BOOL BCMenu::ModifyODMenuW(wchar_t *lpstrText,UINT nID,COLORREF fill,COLORREF border,int hatchstyle,CSize *pSize)
{
    CWnd *pWnd = AfxGetMainWnd();            // Get main window
    CDC *pDC = pWnd->GetDC();              // Get device context
    SIZE sz;
    if(!pSize){
        sz.cx = m_iconX;
        sz.cy = m_iconY;
    }
    else{
        sz.cx = pSize->cx;
        sz.cy = pSize->cy;
    }
    CSize bitmap_size(sz);
    CSize icon_size(m_iconX,m_iconY);
    CBitmap bmp;
    ColorBitmap(pDC,bmp,bitmap_size,icon_size,fill,border,hatchstyle);      
    pWnd->ReleaseDC(pDC);
    return ModifyODMenuW(lpstrText,nID,&bmp);
}


BOOL BCMenu::ModifyODMenuA(const char *lpstrText,const char *OptionText,
                           int nIconNormal)
{
    USES_CONVERSION;
    return ModifyODMenuW(A2W(lpstrText),A2W(OptionText),nIconNormal);//SK: see MFC  Tech Note 059
}

BOOL BCMenu::ModifyODMenuW(wchar_t *lpstrText,wchar_t *OptionText,
                           int nIconNormal)
{
    BCMenuData *mdata;
    
    // Find the old BCMenuData structure:
    CString junk=OptionText;
    mdata=FindMenuOption(OptionText);
    if(mdata){
        if(lpstrText)
            mdata->SetWideString(lpstrText);//SK: modified for dynamic allocation
        mdata->menuIconNormal = nIconNormal;
        mdata->xoffset=-1;
        if(nIconNormal>=0){
            mdata->xoffset=0;
            if(mdata->bitmap)mdata->bitmap->DeleteImageList();
            else mdata->bitmap=new(CImageList);
            mdata->bitmap->Create(m_iconX,m_iconY,ILC_COLORDDB|ILC_MASK,1,1);
            if(!AddBitmapToImageList(mdata->bitmap,nIconNormal)){
                mdata->bitmap->DeleteImageList();
                delete mdata->bitmap;
                mdata->bitmap=NULL;
                mdata->menuIconNormal = nIconNormal = -1;
                mdata->xoffset = -1;
            }
        }
        return(TRUE);
    }
    return(FALSE);
}

BCMenuData *BCMenu::NewODMenu(UINT pos,UINT nFlags,UINT nID,CString string)
{
    BCMenuData *mdata;
    
    mdata = new BCMenuData;
    mdata->menuIconNormal = -1;
    mdata->xoffset=-1;
#ifdef UNICODE
    mdata->SetWideString((LPCTSTR)string);//SK: modified for dynamic allocation
#else
    mdata->SetAnsiString(string);
#endif
    mdata->nFlags = nFlags;
    mdata->nID = nID;
    
//  if(nFlags & MF_POPUP)m_AllSubMenus.Add((HMENU)nID);
        
    if (nFlags&MF_OWNERDRAW){
        ASSERT(!(nFlags&MF_STRING));
        ModifyMenu(pos,nFlags,nID,(LPCTSTR)mdata);
    }
    else if (nFlags&MF_STRING){
        ASSERT(!(nFlags&MF_OWNERDRAW));
        ModifyMenu(pos,nFlags,nID,mdata->GetString());
    }
    else{
        ASSERT(nFlags&MF_SEPARATOR);
        ModifyMenu(pos,nFlags,nID);
    }
    
    return(mdata);
};

BOOL BCMenu::LoadToolbars(const UINT *arID,int n)
{
    ASSERT(arID);
    BOOL returnflag=TRUE;
    for(int i=0;i<n;++i){
        if(!LoadToolbar(arID[i]))returnflag=FALSE;
    }
    return(returnflag);
}

BOOL BCMenu::LoadToolbar(UINT nToolBar)
{
    UINT nID,nStyle;
    BOOL returnflag=FALSE;
    CToolBar bar;
    int xoffset=-1,xset;
    
    CWnd* pWnd = AfxGetMainWnd();
    if (pWnd == NULL)pWnd = CWnd::GetDesktopWindow();
    bar.Create(pWnd);
    if(bar.LoadToolBar(nToolBar)){
        CImageList imglist;
        imglist.Create(m_iconX,m_iconY,ILC_COLORDDB|ILC_MASK,1,1);
        if(AddBitmapToImageList(&imglist,nToolBar)){
            returnflag=TRUE;
            for(int i=0;i<bar.GetCount();++i){
                nID = bar.GetItemID(i); 
                if(nID && GetMenuState(nID, MF_BYCOMMAND)
                    !=0xFFFFFFFF){
                    xoffset=bar.CommandToIndex(nID);
                    if(xoffset>=0){
                        bar.GetButtonInfo(xoffset,nID,nStyle,xset);
                        if(xset>0)xoffset=xset;
                    }
                    ModifyODMenu(NULL,nID,&imglist,xoffset);
                }
            }
        }
    }
    return(returnflag);
}

BOOL BCMenu::LoadFromToolBar(UINT nID,UINT nToolBar,int& xoffset)
{
    int xset,offset;
    UINT nStyle;
    BOOL returnflag=FALSE;
    CToolBar bar;
    
    CWnd* pWnd = AfxGetMainWnd();
    if (pWnd == NULL)pWnd = CWnd::GetDesktopWindow();
    bar.Create(pWnd);
    if(bar.LoadToolBar(nToolBar)){
        offset=bar.CommandToIndex(nID);
        if(offset>=0){
            bar.GetButtonInfo(offset,nID,nStyle,xset);
            if(xset>0)xoffset=xset;
            returnflag=TRUE;
        }
    }
    return(returnflag);
}

// O.S.
BCMenuData *BCMenu::FindMenuItem(UINT nID)
{
    BCMenuData *pData = NULL;
    int i;
    
    for(i = 0; i <= m_MenuList.GetUpperBound(); i++){
        if (m_MenuList[i]->nID == nID){
            pData = m_MenuList[i];
            break;
        }
    }
    if (!pData){
        int loc;
        BCMenu *pMenu = FindMenuOption(nID, loc);
        ASSERT(pMenu != this);
        if (loc >= 0){
            return pMenu->FindMenuItem(nID);
        }
    }
    return pData;
}


BCMenu *BCMenu::FindAnotherMenuOption(int nId,int& nLoc,CArray<BCMenu*,BCMenu*>&bcsubs,
                                      CArray<int,int&>&bclocs)
{
    int i,numsubs,j;
    BCMenu *psubmenu,*pgoodmenu;
    BOOL foundflag;
    
    for(i=0;i<(int)(GetMenuItemCount());++i){
#ifdef _CPPRTTI 
        psubmenu=dynamic_cast<BCMenu *>(GetSubMenu(i));
#else
        psubmenu=(BCMenu *)GetSubMenu(i);
#endif
        if(psubmenu){
            pgoodmenu=psubmenu->FindAnotherMenuOption(nId,nLoc,bcsubs,bclocs);
            if(pgoodmenu)return(pgoodmenu);
        }
        else if(nId==(int)GetMenuItemID(i)){
            numsubs=bcsubs.GetSize();
            foundflag=TRUE;
            for(j=0;j<numsubs;++j){
                if(bcsubs[j]==this&&bclocs[j]==i){
                    foundflag=FALSE;
                    break;
                }
            }
            if(foundflag){
                nLoc=i;
                return(this);
            }
        }
    }
    nLoc = -1;
    return(NULL);
}