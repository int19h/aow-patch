#include "stdafx.h"
#include "resource.h"

HINSTANCE hInst;

DWORD CALLBACK CopyProgressRoutine(
	LARGE_INTEGER TotalFileSize,
	LARGE_INTEGER TotalBytesTransferred,
	LARGE_INTEGER StreamSize,
	LARGE_INTEGER StreamBytesTransferred,
	DWORD dwStreamNumber,
	DWORD dwCallbackReason,
	HANDLE hSourceFile,
	HANDLE hDestinationFile,
	LPVOID lpData)
{
	HWND hDlg = (HWND)lpData;
	SendMessage(GetDlgItem(hDlg, IDC_PROGRESS), PBM_SETPOS, WPARAM(TotalBytesTransferred.QuadPart * 100 / TotalFileSize.QuadPart), 0);
	return PROGRESS_CONTINUE;
}

CString GetErrorText(DWORD err)
{
	CString result;
	LPWSTR errText;
	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
		err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errText, 0, nullptr))
	{
		result = errText;
		LocalFree(errText);
	}
	else
	{
		result.Format(L"error code 0x%lX", err);
	}
	return result;
}

HRESULT MemEq(const void* p1, const void* p2, size_t size)
{
	HRESULT hr;
	__try
	{
		return memcmp(p1, p2, size) ? S_FALSE : S_OK;
	}
	__except (GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR
		? (hr = HRESULT_FROM_NT((GetExceptionInformation())->ExceptionRecord->ExceptionInformation[2]), EXCEPTION_EXECUTE_HANDLER)
		: EXCEPTION_CONTINUE_SEARCH)
	{
		return FAILED(hr) ? hr : E_FAIL;
	}
}

HRESULT MemCopy(void* dst, const void* src, size_t size)
{
	HRESULT hr;
	__try
	{
		memcpy(dst, src, size);
		return S_OK;
	}
	__except (GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR
		? (hr = HRESULT_FROM_NT((GetExceptionInformation())->ExceptionRecord->ExceptionInformation[2]), EXCEPTION_EXECUTE_HANDLER)
		: EXCEPTION_CONTINUE_SEARCH)
	{
		return hr;
	}
}

void DoPatch(void* pvDlg)
{
	static const unsigned char pattern[] = {
		0x66, 0x8B, 0x16, 0x8D,
		0x74, 0x16, 0x03, 0x81,
		0xE6, 0xFC, 0xFF, 0xFF,
		0x0F, 0x48, 0x75, 0xF0,
	};
	static const unsigned char replacement[] = {
		0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90,
	};
	static_assert(sizeof pattern == sizeof replacement, "Pattern and replacement must have the same size");

	HRESULT hr;
	HWND hDlg = (HWND)pvDlg;

	CString path;
	GetDlgItemText(hDlg, IDC_EDIT_PATH, path.GetBuffer(MAX_PATH), MAX_PATH);
	path.ReleaseBuffer();

	CAtlFile srcFile;
	CAtlFileMapping<char> srcMap;
	if (SUCCEEDED(hr = srcFile.Create(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING)))
	{
		ULONGLONG srcSize = 0;
		if (SUCCEEDED(hr = srcFile.GetSize(srcSize)))
		{
			if (srcSize != (DWORD)srcSize || srcSize < sizeof pattern)
			{
				hr = E_FAIL;
			}
			else
			{
				hr = srcMap.MapFile(srcFile, 0, 0, PAGE_READONLY, FILE_MAP_READ);
			}
		}
	}
	if (FAILED(hr))
	{
		CString errMsg;
		errMsg.Format(L"Error opening original file \"%s\":\r\n\r\n%s", path, (LPCWSTR)GetErrorText(hr));
		MessageBox(hDlg, errMsg, nullptr, MB_ICONERROR | MB_OK);
		return;
	}
	
	struct EnableDisableControls
	{
		HWND hDlg;

		EnableDisableControls(HWND hDlg) :
			hDlg(hDlg)
		{
			Edit_SetReadOnly(GetDlgItem(hDlg, IDC_EDIT_PATH), TRUE);
			EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_BROWSE), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDCANCEL), FALSE);
			ShowWindow(GetDlgItem(hDlg, IDC_BUTTON_ABOUT), SW_HIDE);
			ShowWindow(GetDlgItem(hDlg, IDC_STATIC_OPER), SW_SHOW);
			ShowWindow(GetDlgItem(hDlg, IDC_PROGRESS), SW_SHOW);
		}

		~EnableDisableControls()
		{
			EnableWindow(GetDlgItem(hDlg, IDCANCEL), TRUE);
			Edit_SetReadOnly(GetDlgItem(hDlg, IDC_EDIT_PATH), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_BROWSE), TRUE);
			EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
			ShowWindow(GetDlgItem(hDlg, IDC_STATIC_OPER), SW_HIDE);
			ShowWindow(GetDlgItem(hDlg, IDC_PROGRESS), SW_HIDE);
			ShowWindow(GetDlgItem(hDlg, IDC_BUTTON_ABOUT), SW_SHOW);
		}
	} enableDisableControls(hDlg);

	SetDlgItemText(hDlg, IDC_STATIC_OPER, L"Validating original file ...");
	HWND hwndProgress = GetDlgItem(hDlg, IDC_PROGRESS);
	SendMessage(hwndProgress, PBM_SETPOS, 0, 0);

	unsigned int patternCount = 0, replacementCount = 0;
	const char* srcFirst = srcMap;
	const char* srcLast = srcFirst + srcMap.GetMappingSize() - sizeof pattern;
	WPARAM lastPercent = 0;
	for (const char* p = srcFirst; p <= srcLast; ++p)
	{
		auto percent = WPARAM((p - srcFirst) * 100.0 / (srcLast - srcFirst));
		if (percent != lastPercent) {
			SendMessage(hwndProgress, PBM_SETPOS, lastPercent = percent, 0);
		}

		if (SUCCEEDED(hr = MemEq(p, pattern, sizeof pattern)))
		{
			patternCount += (hr == S_FALSE ? 0 : 1);
			if (SUCCEEDED(hr = MemEq(p, replacement, sizeof replacement)))
			{
				replacementCount += (hr == S_FALSE ? 0 : 1);
			}
		}
		if (FAILED(hr))
		{
			CString errMsg;
			errMsg.Format(L"Error reading from original file \"%s\":\r\n\r\n%s", path, (LPCWSTR)GetErrorText(hr));
			MessageBox(hDlg, errMsg, nullptr, MB_ICONERROR | MB_OK);
			return;
		}
	}

	if (patternCount == 0 && replacementCount == 3)
	{
		MessageBox(hDlg, L"The patch is already applied.", nullptr, MB_ICONINFORMATION | MB_OK);
		return;
	}
	if (patternCount != 3)
	{
		MessageBox(hDlg, L"The patch is not applicable to this version.", nullptr, MB_ICONERROR | MB_OK);
		return;
	}

	SetDlgItemText(hDlg, IDC_STATIC_OPER, L"Creating backup copy ...");
	//SetWindowLongPtr(hwndProgress, GWL_STYLE, GetWindowLongPtr(hwndProgress, GWL_STYLE) & ~PBS_MARQUEE);
	//SendMessage(hwndProgress, PBM_SETMARQUEE, FALSE, 0);
	SendMessage(hwndProgress, PBM_SETPOS, 0, 0);

	CString bakPath = path + L".bak";
	for (int i = 0; ++i;)
	{
	copy:
		if (CopyFileEx(path, bakPath, CopyProgressRoutine, hDlg, nullptr, COPY_FILE_FAIL_IF_EXISTS))
		{
			break;
		}
		else
		{
			DWORD err = GetLastError();
			if (err == ERROR_FILE_EXISTS)
			{
				bakPath.Format(L"%s.bak-%d", (LPCWSTR)path, i);
			}
			else
			{
				CString errMsg;
				errMsg.Format(L"Error backing up \"%s\" as \"%s\":\r\n\r\n%s", path, bakPath, (LPCWSTR)GetErrorText(err));
				if (MessageBox(hDlg, errMsg, nullptr, MB_ICONERROR | MB_RETRYCANCEL) == IDRETRY)
				{
					goto copy;
				}
				return;
			}
		}
	}

	SetDlgItemText(hDlg, IDC_STATIC_OPER, L"Applying patch ...");
	//SetWindowLongPtr(hwndProgress, GWL_STYLE, GetWindowLongPtr(hwndProgress, GWL_STYLE) | PBS_MARQUEE);
	//SendMessage(hwndProgress, PBM_SETMARQUEE, TRUE, 0);
	SendMessage(hwndProgress, PBM_SETPOS, 0, 0);

	CAtlTemporaryFile dstFile;
	CAtlFileMapping<char> dstMap;
	if (SUCCEEDED(hr = dstFile.Create(nullptr, GENERIC_READ | GENERIC_WRITE)))
	{
		hr = dstMap.MapFile(dstFile, srcMap.GetMappingSize(), 0, PAGE_READWRITE, FILE_MAP_READ | FILE_MAP_WRITE);
	}
	if (FAILED(hr))
	{
		CString errMsg;
		errMsg.Format(L"Error creating destination file \"%s\":\r\n\r\n%s", dstFile.TempFileName(), (LPCWSTR)GetErrorText(hr));
		MessageBox(hDlg, errMsg, nullptr, MB_ICONERROR | MB_OK);
		return;
	}

	if (FAILED(hr = MemCopy(dstMap.GetData(), srcMap.GetData(), srcMap.GetMappingSize())))
	{
		CString errMsg;
		errMsg.Format(L"Error writing to destination file \"%s\":\r\n\r\n%s", dstFile.TempFileName(), (LPCWSTR)GetErrorText(hr));
		MessageBox(hDlg, errMsg, nullptr, MB_ICONERROR | MB_OK);
		return;
	}

	srcMap.Unmap();
	srcFile.Close();

	char* dstFirst = dstMap;
	char* dstLast = dstFirst + dstMap.GetMappingSize() - sizeof pattern;
	for (char* p = dstFirst; p <= dstLast; ++p)
	{
		auto percent = WPARAM((p - dstFirst) * 100.0 / (dstLast - dstFirst));
		if (percent != lastPercent) {
			SendMessage(hwndProgress, PBM_SETPOS, lastPercent = percent, 0);
		}

		if (SUCCEEDED(hr = MemEq(p, pattern, sizeof pattern)) && hr != S_FALSE)
		{
			hr = MemCopy(p, replacement, sizeof replacement);
		}
		if (FAILED(hr))
		{
			CString errMsg;
			errMsg.Format(L"Error writing to destination file \"%s\":\r\n\r\n%s", dstFile.TempFileName(), (LPCWSTR)GetErrorText(hr));
			MessageBox(hDlg, errMsg, nullptr, MB_ICONERROR | MB_OK);
			return;
		}
	}

	FlushViewOfFile(dstMap, 0);
	dstMap.Unmap();
	if (FAILED(hr = dstFile.Close(path)))
	{
		CString errMsg;
		errMsg.Format(L"Failed to apply patch:\r\n\r\n%s", (LPCWSTR)GetErrorText(GetLastError()));
		MessageBox(hDlg, errMsg, nullptr, MB_ICONERROR | MB_OK);
		return;
	}

	ShowWindow(hDlg, SW_HIDE);
	MessageBox(hDlg, L"The patch has been applied successfully.", L"Success", MB_ICONINFORMATION | MB_OK);
	EndDialog(hDlg, IDOK);
}

void EnableDisablePatchButton(HWND hDlg)
{
	CString path;
	GetDlgItemText(hDlg, IDC_EDIT_PATH, path.GetBuffer(MAX_PATH), MAX_PATH);
	path.ReleaseBuffer();
	BOOL fEnable = (GetFileAttributes(path) != 0xFFFFFFFF);
	EnableWindow(GetDlgItem(hDlg, IDOK), fEnable);
}

INT_PTR CALLBACK DialogFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_AOWPATCH));
		SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

		HKEY hKey = nullptr;
		if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Triumph Studios\\Age of Wonders\\General", &hKey) == ERROR_SUCCESS && hKey)
		{
			CString path;
			DWORD cbPath = MAX_PATH;
			DWORD keyType;
			LPWSTR pathBuf = path.GetBuffer(MAX_PATH);
			if (RegQueryValueEx(hKey, L"Root Directory", nullptr, &keyType, (LPBYTE)pathBuf, &cbPath) == ERROR_SUCCESS && keyType == REG_SZ && cbPath)
			{
				DWORD pathLen = cbPath / 2;
				if (pathLen && !pathBuf[pathLen - 1])
				{
					--pathLen;
				}

				path.ReleaseBuffer(pathLen);

				path.TrimRight(L'\\');
				path += L"\\Ilpack.dpl";
				SetDlgItemText(hDlg, IDC_EDIT_PATH, path);
			}
			RegCloseKey(hKey);
		}

		EnableDisablePatchButton(hDlg);
		return (INT_PTR)TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		case IDC_EDIT_PATH:
			if (HIWORD(wParam) == EN_CHANGE)
			{
				EnableDisablePatchButton(hDlg);
			}
			break;
		case IDC_BUTTON_BROWSE:
		{
			CString path;
			LPWSTR szPath = path.GetBuffer(MAX_PATH);
			GetDlgItemText(hDlg, IDC_EDIT_PATH, szPath, MAX_PATH);

			OPENFILENAME ofn = {};
			ofn.lStructSize = sizeof ofn;
			ofn.hwndOwner = hDlg;
			ofn.lpstrFilter = L"Ilpack.dpl\0Ilpack.dpl\0";
			ofn.lpstrFile = szPath;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
			if (GetOpenFileName(&ofn))
			{
				path.ReleaseBuffer();
				SetDlgItemText(hDlg, IDC_EDIT_PATH, szPath);
			}
		}
		break;
		case IDC_BUTTON_ABOUT:
			MessageBox(hDlg,
				L"This UNOFFICIAL patch tentatively fixes the \"Exception during MapViewer.ShowScene\" error.\r\n\r\n"
				L"Please report any issues at http://github.com/int19h/aow-patch/issues \r\n\r\n"
				L"The source code for this app is embedded into the binary as a Win32 resource with the name \"source.zip\".",
				L"About",
				MB_ICONINFORMATION | MB_OK);
			break;
		case IDOK:
			_beginthread(DoPatch, 0, hDlg);
			break;
		}
	}

	return (INT_PTR)FALSE;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof icc;
	icc.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&icc);

	DialogBoxW(hInst = hInstance, MAKEINTRESOURCE(IDD_MAIN), HWND_DESKTOP, DialogFunc);
	return 0;
}
