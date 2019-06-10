// DropTarget.cpp : implementation file
//

#include "stdafx.h"
#include "cloaker.h"
#include "DropTarget.h"
#include "PasswordDlg.h"
#include "PasswordConfirmDlg.h"
#include "afxdialogex.h"
#include <string.h>

// for functions rust lib references
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "ws2_32.lib")

// rust library function definitions
extern "C" void* makeConfig(CHAR, CHAR*, CHAR*);
extern "C" CHAR* start(void*);
extern "C" void destroyConfig(void*);
extern "C" void destroyCString(CHAR*);

// number of files that can be dragged at once
#define FILELIMIT 1000

enum Mode {
	Encrypt = 0,
	Decrypt = 1
};

// CDropTarget dialog

IMPLEMENT_DYNAMIC(CDropTarget, CDialogEx)

CDropTarget::CDropTarget(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_DROPTARGET, pParent)
{
	Create(IDD_DROPTARGET, pParent);
}

CDropTarget::~CDropTarget()
{
}

void CDropTarget::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDropTarget, CDialogEx)
	ON_WM_DROPFILES()
END_MESSAGE_MAP()


// CDropTarget message handlers


void CDropTarget::OnDropFiles(HDROP hDropInfo)
{
	// initialize rust pointers
	void* config = nullptr;
	CHAR* ret_val = nullptr;
	CString cs_ret = CString(_T(""));

	// and password-related objects
	CString password = CString(_T(""));
	CString confirmPassword = CString(_T(""));
	CPasswordDlg* pwBox = new CPasswordDlg;
	CPasswordConfirmDlg* pwConfirmBox = new CPasswordConfirmDlg;

	// and filename-related objects
	WCHAR filenameBuf[MAX_PATH];
	UINT numFiles = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, NULL);
	UINT copied = DragQueryFile(hDropInfo, 0, filenameBuf, MAX_PATH);

	// get mode
	CHAR mode = GetParent()->IsDlgButtonChecked(IDC_ENCRYPT) ? Encrypt : Decrypt;

	// change text
	this->GetDlgItem(IDC_DROPTEXT)->ShowWindow(SW_HIDE);
	this->GetDlgItem(IDC_DROPTEXT2)->ShowWindow(SW_HIDE);
	this->GetDlgItem(IDC_DROPTEXT3)->ShowWindow(SW_SHOW);

	// make sure only 1 file is being decrypted
	if (numFiles > 1) {
		if (mode == Encrypt) {
			MessageBox(L"To avoid leaving unencrypted partial files in case of program failure, only one file can be encrypted at a time. To encrypt multiple files, please wrap them in a .zip file or similar archive/compression format first.", MB_OK);
			goto CleanUp;
		} else if (mode == Decrypt) {
			MessageBox(L"Only one file at a time can be decrypted.", L"Error", MB_OK);
			goto CleanUp;
		}
	}
	// no folders
	if (GetFileAttributes(filenameBuf) == FILE_ATTRIBUTE_DIRECTORY) {
		MessageBox(L"Must select file. To encrypt a folder, please wrap it a .zip file or similar archive/compression format first.");
		goto CleanUp;
	}

PasswordPrompts:
	pwBox->m_password = "";
	pwConfirmBox->m_password = "";

	INT_PTR ret = pwBox->DoModal();
	if (ret != IDOK) {
		goto CleanUp;
	}
	password = pwBox->m_password;

	if (mode == Encrypt) {
		ret = pwConfirmBox->DoModal();
		if (ret != IDOK) {
			goto CleanUp;
		}
		confirmPassword = pwConfirmBox->m_password;
		if (password.Compare(confirmPassword) != 0) {
			if (IDOK == MessageBox(L"Would you like to re-enter?", L"Passwords do not match", MB_OKCANCEL)) {
				goto PasswordPrompts;
			} else {
				goto CleanUp;
			}
		}
	}

	// convert password and filename to utf8 before handing to rust
	const size_t pwSize = (pwBox->m_password.GetLength() + 1) * 2;
	char *pw = new char[pwSize];
	size_t convertedChars = 0;
	wcstombs_s(&convertedChars, pw, pwSize, pwBox->m_password, _TRUNCATE);

	// new buf for filename needs to have 2 bytes for every wchar in case
	size_t fnSize = MAX_PATH * 2;
	char *fn = new char[fnSize];
	wcstombs_s(&convertedChars, fn, fnSize, filenameBuf, _TRUNCATE);

	// pointer to rust struct
	config = makeConfig(mode, pw, fn);
	ret_val = start(config);
	delete pw;
	delete fn;
	if (ret_val == nullptr) {
		MessageBox(L"Could not start transfer, possibly due to malformed password or filename.", L"Error", MB_OK);
		goto CleanUp;
	}
	cs_ret = CString(ret_val);
	MessageBox(cs_ret, L"", MB_OK);

CleanUp:
	this->GetDlgItem(IDC_DROPTEXT)->ShowWindow(SW_SHOW);
	this->GetDlgItem(IDC_DROPTEXT2)->ShowWindow(SW_SHOW);
	this->GetDlgItem(IDC_DROPTEXT3)->ShowWindow(SW_HIDE);
	delete pwBox;
	delete pwConfirmBox;
	destroyConfig(config);
	destroyCString(ret_val);

	CDialogEx::OnDropFiles(hDropInfo);
}
