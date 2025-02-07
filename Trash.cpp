#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include "framework.h"
#include "resource.h"

#define MAX_LOADSTRING 100

static HICON SHGetIcon(PCIDLIST_ABSOLUTE path) {
	SHFILEINFOW info = { 0 };
	if (SHGetFileInfoW(reinterpret_cast<PCWSTR>(path), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_PIDL))
		return info.hIcon;
	else
		return nullptr;
}

static LPWSTR FormatSize(WCHAR buffer[64], __int64 size) {
	auto format = [](WCHAR buffer[64], __int64 size, LPCWSTR suffix) {
		swprintf_s(buffer, 64, L"%I64d %s", size, suffix);
		return buffer;
	};

	const PCWSTR SUFFIXES[] = {
		L"bytes",
		L"KB",
		L"MB",
		L"GB",
		L"TB",
	};
	for (auto suffix : SUFFIXES) {
		if (size < 10000)
			return format(buffer, size, suffix);
		size = (size + 1023) / 1024;
	}
	return format(buffer, size, SUFFIXES[std::size(SUFFIXES) - 1]);
}

static std::wostream& Append(std::wostream& os, PCWSTR name, __int64 bytes, __int64 count) {
	WCHAR strSize[64];
	os << name << L": ";
	if (count > 0) {
		FormatSize(strSize, bytes);
		os << strSize << L" (" << count << " files)";
	} else {
		os << L"-";
	}
	return os;
}

static std::wstring LoadString(HINSTANCE hInstance, UINT uID) {
	LPCWSTR raw;
	int len = LoadStringW(hInstance, uID, reinterpret_cast<LPWSTR>(&raw), 0);
	return std::wstring(raw, len);
}


int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow
) {
	__int64 trashBytes = 0;
	__int64 trashCount = 0;
	std::wstringstream content;

	const size_t MAX_DRIVES = 8 * sizeof(DWORD);
	DWORD drives = GetLogicalDrives();
	for (size_t i = 0; i < MAX_DRIVES; i++) {
		if (drives & (1 << i)) {
			SHQUERYRBINFO info = { sizeof(SHQUERYRBINFO) };
			WCHAR drive = static_cast<wchar_t>(L'A' + i);
			WCHAR root[] = { drive, L':', L'\\', L'\0' };
			if SUCCEEDED(SHQueryRecycleBinW(root, &info)) {
				if (i > 0)
					content << std::endl;
				trashBytes += info.i64Size;
				trashCount += info.i64NumItems;
				root[1] = L'\0';
				Append(content, root, info.i64Size, info.i64NumItems);
			}
		}
	}

	TASKDIALOGCONFIG dlg = { sizeof(TASKDIALOGCONFIG) };
	dlg.hwndParent = nullptr;
	dlg.hInstance = hInstance;
	dlg.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
	dlg.pszWindowTitle = MAKEINTRESOURCEW(IDS_CAPTION);

	PIDLIST_ABSOLUTE trash;
	if SUCCEEDED(SHGetSpecialFolderLocation(nullptr, CSIDL_BITBUCKET, &trash)) {
		if (auto icon = SHGetIcon(trash)) {
			dlg.dwFlags |= TDF_USE_HICON_MAIN;
			dlg.hMainIcon = icon;
		}
		ILFree(trash);
	}

	std::wstring strTitle, strContent;

	const TASKDIALOG_BUTTON BUTTONS[] = {
		{ IDOK, MAKEINTRESOURCEW(IDS_MAKE_EMPTY) },
		{ IDRETRY, MAKEINTRESOURCEW(IDS_OPEN_TRASH) },
		{ IDCONTINUE, MAKEINTRESOURCEW(IDS_CLEANMGR) },
		{ IDCLOSE, MAKEINTRESOURCEW(IDS_CLOSE) }
	};
	const auto SKIP_IF_EMPTY = 2;

	dlg.pButtons = BUTTONS;
	dlg.cButtons = std::size(BUTTONS);

	if (trashCount > 0) {
		std::wstringstream title;
		WCHAR total[64];
		LoadStringW(hInstance, IDS_TOTAL, total, (int)std::size(total));
		Append(title, total, trashBytes, trashCount);
		strTitle = title.str();
		dlg.pszMainInstruction = strTitle.c_str();
		strContent = content.str();
		dlg.pszContent = strContent.c_str();
	} else {
		dlg.pszMainInstruction = MAKEINTRESOURCEW(IDS_TRASH_IS_EMPTY);
		dlg.pButtons += SKIP_IF_EMPTY;
		dlg.cButtons -= SKIP_IF_EMPTY;
	}
	dlg.nDefaultButton = IDCLOSE;

	int button = IDCANCEL;
	if SUCCEEDED(TaskDialogIndirect(&dlg, &button, nullptr, nullptr)) {
		switch (button) {
		case IDOK:
			SHEmptyRecycleBinW(nullptr, nullptr, 0);
			break;
		case IDRETRY:
			ShellExecuteW(nullptr, L"open", L"explorer.exe", L"/root,::{645FF040-5081-101B-9F08-00AA002F954E}", 0, SW_SHOWNORMAL);
			break;
		case IDCONTINUE:
			ShellExecuteW(nullptr, L"open", L"cleanmgr.exe", L"/D C", 0, SW_SHOWNORMAL);
			break;
		}
	}

	return 0;
}
