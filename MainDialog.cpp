#ifndef _WIN32_IE			// Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

// Windows Header Files:
#include <windows.h>
#include <Windowsx.h>
#include <commctrl.h>
#include <Shellapi.h>
#include <Shlwapi.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include "resource.h"
#include "Hyperlinks.h"
#include <fstream>
#include "ScrobSubmitter.h"
#include "EncodingUtils.h"
using namespace std;

#define TRAYICONID	1//				ID number for the Notify Icon
#define SWM_TRAYMSG	WM_APP//		the message ID sent to our window

#define SWM_SHOW	WM_APP + 1//	show the window
#define SWM_HIDE	WM_APP + 2//	hide the window
#define SWM_EXIT	WM_APP + 3//	close the window

// Global Variables:
HINSTANCE		hInst;	// current instance
NOTIFYICONDATA	niData;	// notify icon data
ScrobSubmitter lastfm;
bool playing = false; // this is changed by the thread
bool radioOn = false; // this is changed by the DlgProc based on playing var
//ofstream logFile;

// Forward declarations of functions included in this code module:
BOOL				InitInstance(HINSTANCE, int);
BOOL				OnInitDialog(HWND hWnd);
void				ShowContextMenu(HWND hWnd);
ULONGLONG			GetDllVersion(LPCTSTR lpszDllName);

INT_PTR CALLBACK	DlgProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

std::string WStringToString(const std::wstring& s)
{    
    const wchar_t *wstr = s.c_str();
    char *str = new char[s.size()*4];

    int a = EncodingUtils::UnicodeToUtf8(wstr, s.size(), str, s.size()*4);
    str[a] = '\0';
    string ret = str;
    
    delete [] str;
    return ret;
}


void status_callback(int  reqId, bool error, std::string msg, void* userData)
{
    //logFile << msg << endl;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    int nLen = GetWindowTextLength(hwnd);
    WCHAR *szTitle = (WCHAR*)malloc(sizeof(WCHAR)*nLen + 1);
    GetWindowText(hwnd, szTitle, nLen);

    wstring a;
    for(int i = 0; i < nLen; i++)
    {
        a += szTitle[i];
    }

    free(szTitle);
    if(a.find(L"RMF") != wstring::npos)
    {
        *((wstring *)lParam) = a;
        return false;
    }
    return true;
}

DWORD WINAPI MiastoMuzykiScrobble(LPVOID iValue)
{         
    lastfm.Init("mms", status_callback, NULL);    

    wstring rmf_title, tt, artist;    
    size_t temp = 0;

    while(true)
    {
        tt = L"";
        // enumerate all desktop windows and find those with "RMF" title:
        EnumDesktopWindows(NULL, EnumWindowsProc, (LPARAM)&tt);

        // if we have found something new then check, whether this is a new song:
        if(tt != rmf_title)
        {
            if(playing)
            {
                lastfm.Stop();
                playing = false;
            }

            rmf_title = tt;
            // the rmf_title syntax: RMF something - Artist: Title - Browser Name
            //logFile << "MiastoMuzyki title changed: " << rmf_title << endl;            

            temp = rmf_title.find(L" - "); // get rid of "RMF something - "
            if(temp != string::npos)
            {
                tt = rmf_title.substr(temp+3); // tt starts with "Artist"
                temp = tt.find(L": "); // get the artist
                if(temp != string::npos)
                {
                    artist = tt.substr(0, temp);
                    tt = tt.substr(temp+2); // get rid of "Artist", tt starts with "Title"       

                    while((temp = artist.find(L" - ")) != string::npos)
                        artist = artist.substr(temp+3);
                    //logFile << "artist: " << artist <<  endl;
                                 
                    temp = tt.find(L" - ");  // get the title
                    if(temp != string::npos)
                    {
                        //logFile << "title: " << tt.substr(0, temp) << endl;
                        lastfm.Start(WStringToString(artist), WStringToString(tt.substr(0, temp)), "", "", 200, "");
                        playing = true;
                    }
                }
            }            
        }
        Sleep(1000);
    }    
    
	return 0;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	MSG msg;
	HACCEL hAccelTable;

    //logFile.open("mm.log", ios::out);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) return FALSE;
	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_STEALTHDIALOG);

    // Start the scrobbling thread:
    HANDLE hThread;
 
    hThread = CreateThread(NULL, 0, MiastoMuzykiScrobble, NULL, 0, 0);
    if(hThread == NULL)
    {
        DWORD dwError = GetLastError();
        return -1;
    }

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)||
			!IsDialogMessage(msg.hwnd,&msg) ) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
    lastfm.Stop();
    lastfm.Term();
    //logFile.close();
	return (int) msg.wParam;
}

//	Initialize the window and tray icon
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	// prepare for XP style controls
	InitCommonControls();

	 // store instance handle and create dialog
	hInst = hInstance;
	HWND hWnd = CreateDialog( hInstance, MAKEINTRESOURCE(IDD_DLG_DIALOG),
		NULL, (DLGPROC)DlgProc );
	if (!hWnd) return FALSE;

	// Fill the NOTIFYICONDATA structure and call Shell_NotifyIcon

	// zero the structure - note:	Some Windows funtions require this but
	//								I can't be bothered which ones do and
	//								which ones don't.
	ZeroMemory(&niData,sizeof(NOTIFYICONDATA));

	// get Shell32 version number and set the size of the structure
	//		note:	the MSDN documentation about this is a little
	//				dubious and I'm not at all sure if the method
	//				bellow is correct
	ULONGLONG ullVersion = GetDllVersion(_T("Shell32.dll"));
	if(ullVersion >= MAKEDLLVERULL(5, 0,0,0))
		niData.cbSize = sizeof(NOTIFYICONDATA);
	else niData.cbSize = NOTIFYICONDATA_V2_SIZE;

	// the ID number can be anything you choose
	niData.uID = TRAYICONID;

	// state which structure members are valid
	niData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

	// load the icon
	niData.hIcon = (HICON)LoadImage(hInstance,MAKEINTRESOURCE(IDI_MMICON),
		IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR);

	// the window to send messages to and the message to send
	//		note:	the message value should be in the
	//				range of WM_APP through 0xBFFF
	niData.hWnd = hWnd;
    niData.uCallbackMessage = SWM_TRAYMSG;

	// tooltip message
    lstrcpyn(niData.szTip, _T("Lalalalalalalalala, muzyka gra!"), sizeof(niData.szTip)/sizeof(TCHAR));

	Shell_NotifyIcon(NIM_ADD,&niData);

	// free icon handle
	if(niData.hIcon && DestroyIcon(niData.hIcon))
		niData.hIcon = NULL;

	// call ShowWindow here to make the dialog initially visible

	return TRUE;
}

BOOL OnInitDialog(HWND hWnd)
{
	HMENU hMenu = GetSystemMenu(hWnd,FALSE);
	if (hMenu)
	{
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu, MF_STRING, IDM_ABOUT, _T("O..."));
	}
	HICON hIcon = (HICON)LoadImage(hInst,
		MAKEINTRESOURCE(IDI_MMICON),
		IMAGE_ICON, 0,0, LR_SHARED|LR_DEFAULTSIZE);
	SendMessage(hWnd,WM_SETICON,ICON_BIG,(LPARAM)hIcon);
	SendMessage(hWnd,WM_SETICON,ICON_SMALL,(LPARAM)hIcon);

	return TRUE;
}

// Name says it all
void ShowContextMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();
	if(hMenu)
	{
		if( IsWindowVisible(hWnd) )
			InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_HIDE, _T("Schowaj"));
		else
			InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_SHOW, _T("Pokaø"));
        InsertMenu(hMenu, -1, MF_BYPOSITION, IDM_ABOUT, _T("O niniejszej wtyczce..."));
		InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_EXIT, _T("Wyjdü"));

		// note:	must set window to the foreground or the
		//			menu won't disappear when it should
		SetForegroundWindow(hWnd);

		TrackPopupMenu(hMenu, TPM_BOTTOMALIGN,
			pt.x, pt.y, 0, hWnd, NULL );
		DestroyMenu(hMenu);
	}
}

// Get dll version number
ULONGLONG GetDllVersion(LPCTSTR lpszDllName)
{
    ULONGLONG ullVersion = 0;
	HINSTANCE hinstDll;
    hinstDll = LoadLibrary(lpszDllName);
    if(hinstDll)
    {
        DLLGETVERSIONPROC pDllGetVersion;
        pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");
        if(pDllGetVersion)
        {
            DLLVERSIONINFO dvi;
            HRESULT hr;
            ZeroMemory(&dvi, sizeof(dvi));
            dvi.cbSize = sizeof(dvi);
            hr = (*pDllGetVersion)(&dvi);
            if(SUCCEEDED(hr))
				ullVersion = MAKEDLLVERULL(dvi.dwMajorVersion, dvi.dwMinorVersion,0,0);
        }
        FreeLibrary(hinstDll);
    }
    return ullVersion;
}

// Message handler for the app
INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;

	if(playing != radioOn)
	{
		radioOn = !radioOn;
		if(radioOn)
		{
			ShowWindow(GetDlgItem(hWnd, IDC_STATIC_OK), SW_SHOW);
			ShowWindow(GetDlgItem(hWnd, IDC_STATIC_NOTFOUND), SW_HIDE);
		}
		else
		{
			ShowWindow(GetDlgItem(hWnd, IDC_STATIC_OK), SW_HIDE);
			ShowWindow(GetDlgItem(hWnd, IDC_STATIC_NOTFOUND), SW_SHOW);
		}

	}

	switch (message) 
	{
	case SWM_TRAYMSG:
		switch(lParam)
		{
		case WM_LBUTTONDBLCLK:
			ShowWindow(hWnd, SW_RESTORE);
			break;
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			ShowContextMenu(hWnd);
		}
		break;
	case WM_SYSCOMMAND:
		if((wParam & 0xFFF0) == SC_MINIMIZE)
		{
			ShowWindow(hWnd, SW_HIDE);
			return 1;
		}
		else if(wParam == IDM_ABOUT)
			DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam); 

		switch (wmId)
		{
		case SWM_SHOW:
			ShowWindow(hWnd, SW_RESTORE);
			break;
		case SWM_HIDE:
		case IDOK:
			ShowWindow(hWnd, SW_HIDE);
			break;
		case SWM_EXIT:
			DestroyWindow(hWnd);
			break;
		case IDM_ABOUT:
			DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
			break;
		case IDC_STATIC_MMLINK:
			{
				ShellExecute(hWnd, L"open",
					L"http://www.miastomuzyki.pl/",
					NULL, NULL, SW_SHOWNORMAL);
				return TRUE;
			}; break;
		case IDB_ONABOUT:
			{
				DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
			}; break;
		}
		return 1;
	case WM_INITDIALOG:
		ConvertStaticToHyperlink(hWnd, IDC_STATIC_MMLINK);
		return OnInitDialog(hWnd);
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		niData.uFlags = 0;
		Shell_NotifyIcon(NIM_DELETE,&niData);
		PostQuitMessage(0);
		break;
	}
	return 0;
}

// Message handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
        ConvertStaticToHyperlink(hDlg, IDC_STATIC_JOURNAL);
        ConvertStaticToHyperlink(hDlg, IDC_STATIC_INSTALL_LASTFM);		
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}     
        else switch (LOWORD(wParam))
        {
        case IDC_STATIC_JOURNAL:
			{
				ShellExecute(hDlg, L"open",
					L"http://www.last.fm/user/micio/journal",
						 NULL, NULL, SW_SHOWNORMAL);
				return TRUE;
			}; break;
        case IDC_STATIC_INSTALL_LASTFM:
			{
				ShellExecute(hDlg, L"open",
					L"http://www.last.fm/download/",
						 NULL, NULL, SW_SHOWNORMAL);
				return TRUE;
			}; break;		
        }
		break;
	}
	return FALSE;
}
