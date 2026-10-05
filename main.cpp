#include <windows.h>
#include <shellapi.h>
#include <string>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001

HWND hwnd = nullptr;
NOTIFYICONDATAW nid = { 0 };
WCHAR currentLangText[8] = L"";
bool isEnglish = true;

// Helper to map LANGID to Locale Name string
PCWSTR GetLocaleNameFromLangId(LANGID langId) {
    static WCHAR name[LOCALE_NAME_MAX_LENGTH];
    if (LCIDToLocaleName(MAKELCID(langId, SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0) > 0) {
        return name;
    }
    return LOCALE_NAME_USER_DEFAULT;
}

// Function to check the active keyboard language
void UpdateLanguageStatus(HWND hWindow) {
    HWND hForeground = GetForegroundWindow();
    DWORD_PTR threadId = GetWindowThreadProcessId(hForeground, NULL);
    HKL hkl = GetKeyboardLayout(static_cast<DWORD>(threadId));
    
    LANGID langId = LOWORD(hkl);
    LANGID primaryLang = PRIMARYLANGID(langId);

    if (primaryLang == LANG_ENGLISH) {
        if (!isEnglish) {
            isEnglish = true;
            ShowWindow(hWindow, SW_HIDE);
        }
    } else {
        isEnglish = false;
        WCHAR localeName[LOCALE_NAME_MAX_LENGTH];
        if (GetLocaleInfoEx(GetLocaleNameFromLangId(langId), LOCALE_SISO639LANGNAME, localeName, LOCALE_NAME_MAX_LENGTH) > 0) {
            wcscpy_s(currentLangText, localeName);
            for (int i = 0; currentLangText[i] != L'\0'; ++i) {
                currentLangText[i] = towupper(currentLangText[i]);
            }
        } else {
            wcscpy_s(currentLangText, L"LANG");
        }

        GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
        if (GetGUIThreadInfo(threadId, &gti) && gti.hwndFocus) {
            POINT pt = { 0, 0 };
            if (gti.rcCaret.left | gti.rcCaret.top) {
                pt.x = gti.rcCaret.right;
                pt.y = gti.rcCaret.bottom + 15;
                ClientToScreen(gti.hwndFocus, &pt);
            } else {
                GetCursorPos(&pt);
                pt.x += 15;
                pt.y += 15;
            }
            SetWindowPos(hWindow, HWND_TOPMOST, pt.x, pt.y, 65, 32, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        } else {
            POINT pt;
            GetCursorPos(&pt);
            SetWindowPos(hWindow, HWND_TOPMOST, pt.x + 15, pt.y + 15, 65, 32, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        }

        ShowWindow(hWindow, SW_SHOWNOACTIVATE);
        InvalidateRect(hWindow, NULL, TRUE);
    }
}

// Window Procedure
LRESULT CALLBACK WndProc(HWND hWindow, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        SetTimer(hWindow, 1, 100, NULL);

        // Setup System Tray Icon with a clean system stock icon (OIC_SAMPLE or Shell extract)
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = hWindow;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        
        // Load a clean system icon (using LoadImage to grab a standard information/keyboard-like icon or fallback)
        nid.hIcon = (HICON)LoadImageW(nullptr, IDI_INFORMATION, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
        
        wcscpy_s(nid.szTip, L"Language Indicator (Active)");
        Shell_NotifyIconW(NIM_ADD, &nid);
        break;
    }

    case WM_TIMER:
        UpdateLanguageStatus(hWindow);
        break;

    case WM_TRAYICON: {
        if (lp == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Quit Language Indicator");

            SetForegroundWindow(hWindow);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWindow, nullptr);
            DestroyMenu(hMenu);
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == ID_TRAY_EXIT) {
            DestroyWindow(hWindow);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWindow, &ps);

        if (!isEnglish) {
            RECT rect;
            GetClientRect(hWindow, &rect);

            HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 245));
            FillRect(hdc, &rect, bgBrush);
            DeleteObject(bgBrush);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(20, 20, 20));

            LOGFONTW lf = { 0 };
            lf.lfHeight = 18;
            lf.lfWeight = FW_BOLD;
            wcscpy_s(lf.lfFaceName, L"Segoe UI");
            HFONT hFont = CreateFontIndirectW(&lf);
            HGDIOBJ oldFont = SelectObject(hdc, hFont);

            DrawTextW(hdc, currentLangText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, oldFont);
            DeleteObject(hFont);
        }
        EndPaint(hWindow, &ps);
        break;
    }

    case WM_DESTROY:
        KillTimer(hWindow, 1);
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWindow, msg, wp, lp);
    }
    return 0;
}

// Entry Point
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"LangOverlayClass";
    RegisterClassExW(&wc);

    hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        L"LangOverlayClass",
        L"Language Overlay",
        WS_POPUP,
        0, 0, 65, 32,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) return 0;

    SetLayeredWindowAttributes(hwnd, 0, 220, LWA_ALPHA);
    ShowWindow(hwnd, SW_HIDE);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
