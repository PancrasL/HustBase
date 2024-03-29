// HustBase.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "HustBase.h"

#include "MainFrm.h"
#include "HustBaseDoc.h"
#include "HustBaseView.h"
#include "TreeList.h"

#include "IX_Manager.h"
#include "PF_Manager.h"
#include "RM_Manager.h"
#include "SYS_Manager.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CHustBaseApp

BEGIN_MESSAGE_MAP(CHustBaseApp, CWinApp)
	//{{AFX_MSG_MAP(CHustBaseApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_CREATEDB, OnCreateDB)
	ON_COMMAND(ID_OPENDB, OnOpenDB)
	ON_COMMAND(ID_DROPDB, OnDropDb)
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHustBaseApp construction

CHustBaseApp::CHustBaseApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CHustBaseApp object

CHustBaseApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CHustBaseApp initialization
bool CHustBaseApp::pathvalue = false;

BOOL CHustBaseApp::InitInstance()
{
	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	// Change the registry key under which our settings are stored.
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization.
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CHustBaseDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CHustBaseView));
	AddDocTemplate(pDocTemplate);

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Dispatch commands specified on the command line
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	// The one and only window has been initialized, so show and update it.
	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();

	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	// Dialog Data
		//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CHustBaseApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CHustBaseApp message handlers

void CHustBaseApp::OnCreateDB()
{
	CFileDialog dialog(TRUE);
	dialog.m_ofn.lpstrTitle = "请选择数据库的位置,并在对话框中输入数据库名";
	if (dialog.DoModal() == IDOK)
	{
		CString filePath, fileName, actualFilePath;
		filePath = dialog.GetPathName();
		fileName = dialog.GetFileTitle();
		int actualPathLength = filePath.GetLength() - fileName.GetLength() - 1; //减1是为了去掉‘/’
		actualFilePath = filePath.Left(actualPathLength);  //获得不包含即将创建的表文件夹的文件夹地址
		//将CString类型数据转换为char*类型输入
		char *filePathC = (char *)malloc(filePath.GetLength() + 1);
		strcpy(filePathC, actualFilePath);
		char *fileNameC = (char *)malloc(fileName.GetLength() + 1);
		strcpy(fileNameC, fileName);
		CreateDB(filePathC, fileNameC);
	}
}

void CHustBaseApp::OnOpenDB()
{
	//关联打开数据库按钮，此处应提示用户输入数据库所在位置，并调用OpenDB函数改变当前数据库路径，并在界面左侧的控件中显示数据库中的表、列信息。
	LPITEMIDLIST rootLoation;
	BROWSEINFO bi;
	RC rc;
	TCHAR dbName[MAX_PATH];

	SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOP, &rootLoation);
	if (rootLoation == NULL) {
		return;
	}
	ZeroMemory(&bi, sizeof(bi));
	bi.pidlRoot = rootLoation; // 文件夹对话框之根目录，默认是桌面
	LPITEMIDLIST targetLocation = SHBrowseForFolder(&bi);
	if (targetLocation != NULL) {
		SHGetPathFromIDList(targetLocation, dbName);
	}
	rc = OpenDB(dbName);
	if (rc != SUCCESS) {
		AfxMessageBox("打开的不是数据库");
		return;
	}
	CHustBaseApp::pathvalue = true;
	CHustBaseDoc *pDoc;
	pDoc = CHustBaseDoc::GetDoc();
	pDoc->m_pTreeView->PopulateTree();
}

void CHustBaseApp::OnDropDb()
{
	//关联删除数据库按钮，此处应提示用户输入数据库所在位置，并调用DropDB函数删除数据库的内容。
	LPITEMIDLIST rootLoation;
	BROWSEINFO bi;
	RC rc;
	TCHAR targetPath[MAX_PATH];
	char *dbName;

	SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOP, &rootLoation);
	if (rootLoation == NULL) {
		return;
	}
	ZeroMemory(&bi, sizeof(bi));
	bi.pidlRoot = rootLoation; // 文件夹对话框之根目录，默认是桌面
	LPITEMIDLIST targetLocation = SHBrowseForFolder(&bi);
	if (targetLocation != NULL) {
		SHGetPathFromIDList(targetLocation, targetPath);
	}
	CHustBaseApp::pathvalue = false;
	dbName = targetPath;
	rc = DropDB(dbName);
	if (rc != SUCCESS) {
		AfxMessageBox("删除的不是数据库");
		return;
	}
}
