#include <windows.h> // Объявляет оконные функции Win32 и типы GDI.
namespace { // Ограничивает видимость имён этим файлом.
constexpr wchar_t windowClassName[] = L"RollCastMainWindow"; // Указывает имя класса главного окна.
LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) // Принимает сообщения главного окна.
{ // Начинает обработку сообщения.
    if (message == WM_DESTROY) { // Проверяет, уничтожено ли главное окно.
        PostQuitMessage(0); // Завершает цикл сообщений с кодом успеха.
        return 0; // Подтверждает обработку сообщения.
    } // Завершает обработку уничтожения окна.
    return DefWindowProcW(window, message, wParam, lParam); // Передаёт остальные сообщения стандартному обработчику Windows.
} // Завершает оконную процедуру.
} // Закрывает область локальных имён.
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) // Объявляет точку входа приложения Unicode.
{ // Начинает запуск приложения.
    WNDCLASSW windowClass{}; // Создаёт описание окна с нулевыми начальными полями.
    windowClass.lpfnWndProc = windowProcedure; // Связывает класс окна с обработчиком сообщений.
    windowClass.hInstance = instance; // Связывает класс с текущим экземпляром приложения.
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW); // Загружает системный указатель в виде стрелки.
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH)); // Назначает системную светло-серую кисть рабочей области.
    windowClass.lpszClassName = windowClassName; // Задаёт имя для регистрации и создания окна.
    if (RegisterClassW(&windowClass) == 0) { // Проверяет успешность регистрации класса в Windows.
        MessageBoxW(nullptr, L"Не удалось зарегистрировать класс окна.", L"RollCast", MB_OK | MB_ICONERROR); // Показывает ошибку запуска.
        return 1; // Возвращает код ошибки регистрации.
    } // Завершает проверку регистрации.
    const HWND window = CreateWindowExW( // Создаёт главное окно приложения.
        0, // Использует обычные расширенные параметры окна.
        windowClassName, // Выбирает зарегистрированный оконный класс.
        L"RollCast", // Задаёт заголовок окна.
        WS_OVERLAPPEDWINDOW, // Включает системную рамку и управление размером.
        CW_USEDEFAULT, CW_USEDEFAULT, // Оставляет выбор позиции операционной системе.
        960, 640, // Задаёт начальные ширину и высоту в пикселях.
        nullptr, nullptr, // Создаёт самостоятельное окно без меню.
        instance, nullptr); // Передаёт экземпляр приложения без дополнительных данных.
    if (window == nullptr) { // Проверяет, получен ли дескриптор окна.
        MessageBoxW(nullptr, L"Не удалось создать главное окно.", L"RollCast", MB_OK | MB_ICONERROR); // Показывает ошибку создания окна.
        return 1; // Возвращает код ошибки создания окна.
    } // Завершает проверку создания окна.
    ShowWindow(window, showCommand); // Показывает окно в режиме запуска Windows.
    MSG message{}; // Хранит очередное сообщение Windows.
    BOOL status = 0; // Хранит результат чтения сообщения из очереди.
    while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) { // Ожидает сообщения до команды завершения или ошибки.
        TranslateMessage(&message); // Преобразует нажатия клавиш в символьные сообщения.
        DispatchMessageW(&message); // Отправляет сообщение оконной процедуре.
    } // Завершает цикл после команды выхода или ошибки.
    if (status == -1) { // Проверяет, произошла ли ошибка чтения очереди.
        MessageBoxW(nullptr, L"Не удалось прочитать сообщение Windows.", L"RollCast", MB_OK | MB_ICONERROR); // Сообщает об ошибке очереди.
        return 1; // Возвращает код ошибки цикла сообщений.
    } // Завершает проверку результата цикла.
    return static_cast<int>(message.wParam); // Возвращает код из сообщения о завершении.
} // Завершает работу приложения.
