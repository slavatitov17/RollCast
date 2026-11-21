#include "about_view.h"	// Подключает интерфейс модального окна объяснений
#include "about_content.h"	// Подключает концепцию формулы и фрагменты кода
#include <commctrl.h>	// Предоставляет вкладки общей и расчетной справки
#include <richedit.h>	// Предоставляет форматированный текст с прокруткой
#include <algorithm>	// Ограничивает размеры и длину чтения
#include <cstring>	// Копирует очередную часть RTF
#include <stdexcept>	// Передает ошибки создания справочного окна
namespace {	// Ограничивает видимость реализации справки
struct AboutState { HWND tabs{}; HWND document{}; bool closed = false; };	// Хранит элементы и состояние модального окна
struct Stream { const std::string& data; std::size_t position = 0; };	// Передает документ в RichEdit без файлов
std::string rtfEscape(const std::wstring& text) {	// Преобразует русский текст в переносимый RTF
    std::string value;	// Накапливает управляющие и печатные символы
    for (const wchar_t c : text) {	// Обрабатывает каждый символ UTF-16
        if (c == L'\\' || c == L'{' || c == L'}') { value += '\\'; value += static_cast<char>(c); }	// Экранирует управляющие символы формата
        else if (c == L'\n') { value += "\\line "; }	// Сохраняет перенос строки внутри фрагмента кода
        else if (c >= 32 && c < 127) { value += static_cast<char>(c); }	// Копирует обычный латинский символ
        else { value += "\\u" + std::to_string(static_cast<short>(c)) + "?"; }	// Сохраняет кириллицу и математические знаки
    }	// Завершает текущий блок обработки
    return value;	// Возвращает безопасный текст RTF
}	// Завершает текущий блок обработки
std::string document(bool calculations) {	// Создает типографически различимые элементы справки
    std::string rtf = "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Segoe UI;}{\\f1 Cambria Math;}{\\f2 Consolas;}}{\\colortbl;\\red25\\green42\\blue55;\\red26\\green78\\blue115;\\red243\\green246\\blue248;}\\viewkind4\\uc1 ";	// Настраивает шрифты и сдержанные цвета
    int equation = 0;	// Нумерует формулы последовательно внутри расчетной вкладки
    for (const auto& block : aboutContent(calculations)) {	// Выводит материал в заданном порядке
        const auto text = rtfEscape(block.text);	// Преобразует содержание выбранного блока
        switch (block.kind) {	// Выбирает оформление по роли текста
        case HelpKind::heading: rtf += "\\pard\\ql\\sb260\\sa120\\f0\\fs30\\b\\cf2 " + text + "\\b0\\par "; break;	// Выделяет заголовки разделов и подразделов
        case HelpKind::paragraph: rtf += "\\pard\\ql\\sb20\\sa150\\sl300\\slmult1\\f0\\fs23\\cf1 " + text + "\\par "; break;	// Обеспечивает читаемый основной русский текст
        case HelpKind::formula: rtf += "\\pard\\qc\\sb170\\sa200\\f1\\fs28\\i\\cf1 " + text + "    (" + std::to_string(++equation) + ")\\i0\\par "; break;	// Центрирует курсивную формулу с ее номером
        case HelpKind::code: rtf += "\\pard\\ql\\li180\\ri100\\sb90\\sa180\\f2\\fs21\\highlight3\\cf1 " + text + "\\highlight0\\par "; break;	// Отделяет код моноширинным шрифтом и фоном
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    return rtf + "}";	// Закрывает корректный документ RTF
}	// Завершает текущий блок обработки
DWORD CALLBACK readStream(DWORD_PTR cookie, LPBYTE buffer, LONG length, LONG* read) {	// Передает RichEdit очередной фрагмент документа
    auto& stream = *reinterpret_cast<Stream*>(cookie);	// Получает состояние синхронной передачи
    const auto count = std::min(static_cast<std::size_t>(length), stream.data.size() - stream.position);	// Ограничивает чтение оставшимися данными
    std::memcpy(buffer, stream.data.data() + stream.position, count);	// Копирует только доступные байты RTF
    stream.position += count; *read = static_cast<LONG>(count); return 0;	// Продвигает поток и сообщает число байтов
}	// Завершает текущий блок обработки
void loadDocument(AboutState& state) {	// Показывает выбранную вкладку начиная с ее заголовка
    const auto rtf = document(TabCtrl_GetCurSel(state.tabs) == 1);	// Выбирает общие сведения либо расчетную теорию
    Stream input{rtf}; EDITSTREAM stream{};	// Подготавливает синхронный поток форматированного текста
    stream.dwCookie = reinterpret_cast<DWORD_PTR>(&input); stream.pfnCallback = readStream;	// Связывает RichEdit с локальным источником
    SendMessageW(state.document, WM_SETREDRAW, FALSE, 0);	// Предотвращает мерцание при замене вкладки
    SendMessageW(state.document, EM_STREAMIN, SF_RTF, reinterpret_cast<LPARAM>(&stream));	// Загружает заголовки курсивные формулы и код
    SendMessageW(state.document, EM_SETSEL, 0, 0); SendMessageW(state.document, WM_VSCROLL, SB_TOP, 0);	// Возвращает чтение к началу раздела
    SendMessageW(state.document, WM_SETREDRAW, TRUE, 0); InvalidateRect(state.document, nullptr, TRUE);	// Показывает целиком подготовленный документ
    if (stream.dwError) { throw std::runtime_error("Не удалось отобразить справку"); }	// Обнаруживает ошибку разбора форматированного текста
}	// Завершает текущий блок обработки
void arrange(HWND window, AboutState& state) {	// Масштабирует документ вместе с модальным окном
    RECT area{}; GetClientRect(window, &area);	// Получает внутреннюю область справки
    MoveWindow(state.tabs, 18, 14, area.right - 36, 29, TRUE);	// Оставляет вкладки постоянно доступными сверху
    MoveWindow(state.document, 18, 54, area.right - 36, area.bottom - 112, TRUE);	// Выделяет основной размер прокручиваемому документу
    MoveWindow(GetDlgItem(window, 2), area.right - 128, area.bottom - 45, 110, 29, TRUE);	// Закрепляет закрытие справа снизу
}	// Завершает текущий блок обработки
LRESULT CALLBACK aboutProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {	// Управляет вкладками и закрытием справки
    auto* state = reinterpret_cast<AboutState*>(GetWindowLongPtrW(window, GWLP_USERDATA));	// Читает принадлежащее окну состояние
    if (message == WM_NCCREATE) {	// Принимает состояние до создания элементов
        state = static_cast<AboutState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);	// Извлекает переданный экземпляр модальной справки
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));	// Сохраняет состояние на время окна
    }	// Завершает текущий блок обработки
    if (!state) { return DefWindowProcW(window, message, wParam, lParam); }	// Сохраняет стандартную обработку ранних сообщений
    try {	// Защищает модальный цикл от ошибок отображения
        switch (message) {	// Разбирает действия справочного окна
        case WM_CREATE: {	// Создает вкладки и форматированный документ
            state->tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(1), nullptr, nullptr);	// Создает переключатель разделов справки
            for (const auto* title : {L"Общее", L"Расчеты"}) { TCITEMW item{}; item.mask = TCIF_TEXT; item.pszText = const_cast<wchar_t*>(title); TabCtrl_InsertItem(state->tabs, TabCtrl_GetItemCount(state->tabs), &item); }	// Добавляет обе требуемые вкладки
            state->document = CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(3), nullptr, nullptr);	// Разрешает чтение выделение и вертикальную прокрутку
            if (!state->tabs || !state->document) { return -1; }	// Отказывается от неполностью созданной справки
            SendMessageW(state->document, EM_SETBKGNDCOLOR, 0, RGB(255, 255, 255));	// Сохраняет белый фон чтения
            SendMessageW(state->document, EM_EXLIMITTEXT, 0, 1000000);	// Размещает развернутое объяснение целиком
            SendMessageW(state->document, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(18, 18));	// Отделяет текст от краев документа
            const auto close = CreateWindowExW(0, L"BUTTON", L"Закрыть", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(2), nullptr, nullptr);	// Создает стандартное действие закрытия
            for (const HWND child : {state->tabs, close}) { SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE); }	// Согласует шрифт элементов управления
            arrange(window, *state); loadDocument(*state); return 0;	// Показывает концепцию первой вкладкой
        }	// Завершает текущий блок обработки
        case WM_SIZE: arrange(window, *state); return 0;	// Сохраняет прокрутку и кнопки при изменении размеров
        case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {760, 540}; return 0;	// Сохраняет читаемую минимальную область документа
        case WM_NOTIFY: if (reinterpret_cast<NMHDR*>(lParam)->idFrom == 1 && reinterpret_cast<NMHDR*>(lParam)->code == TCN_SELCHANGE) { loadDocument(*state); } return 0;	// Меняет содержимое только при выборе вкладки
        case WM_COMMAND: if (LOWORD(wParam) == 2 || LOWORD(wParam) == IDCANCEL) { DestroyWindow(window); } return 0;	// Закрывает справку кнопкой либо Escape
        case WM_CLOSE: DestroyWindow(window); return 0;	// Сохраняет стандартное закрытие системной кнопкой
        case WM_DESTROY: state->closed = true; return 0;	// Завершает только локальный модальный цикл
        default: break;	// Передает остальные сообщения системе
        }	// Завершает текущий блок обработки
    } catch (const std::exception&) { DestroyWindow(window); }	// Возвращает пользователя к рабочему окну при ошибке
    return DefWindowProcW(window, message, wParam, lParam);	// Выполняет стандартное поведение окна
}	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
void showAbout(HWND owner) {	// Показывает объяснение поверх затемненного рабочего экрана
    const auto library = LoadLibraryW(L"Msftedit.dll");	// Подключает штатный форматированный редактор Windows
    if (!library) { throw std::runtime_error("Недоступен системный компонент RichEdit"); }	// Объясняет недоступность встроенной справки
    const HINSTANCE instance = GetModuleHandleW(nullptr);	// Получает экземпляр текущей программы
    WNDCLASSW type{}; type.lpfnWndProc = aboutProcedure; type.hInstance = instance;	// Регистрирует отдельное справочное окно
    type.lpszClassName = L"RollCastAboutWindow"; type.hCursor = LoadCursorW(nullptr, IDC_ARROW);	// Назначает класс и обычный указатель
    type.hbrBackground = GetSysColorBrush(COLOR_BTNFACE); RegisterClassW(&type);	// Согласует оформление с основным интерфейсом
    WNDCLASSW shadeType{}; shadeType.lpfnWndProc = DefWindowProcW; shadeType.hInstance = instance;	// Создает затемняющий фон без обработки команд
    shadeType.lpszClassName = L"RollCastAboutShade"; shadeType.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));	// Ограничивает затемнение областью родителя
    RegisterClassW(&shadeType);	// Разрешает многократное открытие затемняющего окна
    RECT bounds{}; GetWindowRect(owner, &bounds);	// Получает размеры рабочего окна программы
    const HWND shade = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW, shadeType.lpszClassName, L"", WS_POPUP, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, owner, nullptr, instance, nullptr);	// Накрывает только RollCast полупрозрачным слоем
    if (!shade) { FreeLibrary(library); throw std::runtime_error("Не удалось создать фон справки"); }	// Проверяет создание затемняющего слоя
    SetLayeredWindowAttributes(shade, 0, 105, LWA_ALPHA); ShowWindow(shade, SW_SHOWNOACTIVATE);	// Сохраняет видимость рабочего контекста под оверлеем
    AboutState state; const int width = std::min(1100L, bounds.right - bounds.left - 80);	// Выбирает читаемую ширину справочного документа
    const int height = std::min(820L, bounds.bottom - bounds.top - 80);	// Оставляет видимым затемнение вокруг справки
    EnableWindow(owner, FALSE);	// Предотвращает изменение входов под модальным окном
    const HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, type.lpszClassName, L"О программе – RollCast", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN, bounds.left + (bounds.right - bounds.left - width) / 2, bounds.top + (bounds.bottom - bounds.top - height) / 2, width, height, shade, nullptr, instance, &state);	// Центрирует окно с двумя разделами
    if (dialog) {	// Запускает локальное чтение созданного документа
        ShowWindow(dialog, SW_SHOW); SetForegroundWindow(dialog); MSG message{};	// Передает фокус встроенной справке
        while (!state.closed) {	// Обрабатывает события до закрытия справки
            const BOOL next = GetMessageW(&message, nullptr, 0, 0);	// Сохраняет обработку фонового расчета родителя
            if (next <= 0) { if (next == 0) { PostQuitMessage(static_cast<int>(message.wParam)); } break; }	// Сохраняет запрос завершения всего приложения
            if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) { DestroyWindow(dialog); continue; }	// Закрывает справку клавишей Escape
            if (!IsDialogMessageW(dialog, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }	// Сохраняет прокрутку и клавиатурную навигацию
        }	// Завершает текущий блок обработки
        if (IsWindow(dialog)) { DestroyWindow(dialog); }	// Освобождает оставшееся окно при общем завершении
    }	// Завершает текущий блок обработки
    EnableWindow(owner, TRUE); DestroyWindow(shade); SetForegroundWindow(owner); FreeLibrary(library);	// Возвращает рабочее окно и освобождает ресурсы
    if (!dialog) { throw std::runtime_error("Не удалось открыть справку"); }	// Объясняет неудачное создание модального окна
}	// Завершает текущий блок обработки
