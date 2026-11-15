#include "mold_view.h"																							// Подключает размеры и отрисовку литейной формы
#include <cwchar>																								// Предоставляет безопасный разбор целых размеров
namespace {																										// Ограничивает видимость имён этим файлом
constexpr wchar_t windowClassName[] = L"RollCastMainWindow";													// Указывает имя класса главного окна
constexpr int firstDimension = 100;																				// Задаёт начальный идентификатор полей размеров
constexpr int applyDimensions = 108;																			// Задаёт идентификатор кнопки применения размеров
constexpr int statusText = 109;																					// Задаёт идентификатор строки проверки
constexpr int panelWidth = 256;																					// Задаёт ширину панели ввода
constexpr int split = 284;																						// Отделяет панель ввода от разреза
constexpr const wchar_t* sectionNames[]{																		// Перечисляет отделения в порядке модели
    L"Нижняя шейка", L"Бочка", L"Верхняя шейка", L"Прибыль"														// Задаёт названия строк панели ввода
};																												// Завершает список названий
struct AppState {																								// Хранит состояние текущего окна
    MoldDimensions dimensions = demoDimensions;																	// Начинает с условных демонстрационных размеров
};																												// Завершает описание состояния окна
HWND control(HWND window, const wchar_t* type, const wchar_t* text,												// Описывает тип, текст и родителя элемента
             DWORD style, int x, int y, int width, int height, int id = 0) {									// Принимает размер, положение и идентификатор
    const DWORD frame = type[0] == L'E' ? WS_EX_CLIENTEDGE : 0;													// Добавляет системную рамку полям ввода
    const HWND child = CreateWindowExW(frame, type, text, WS_CHILD | WS_VISIBLE | style,						// Создаёт стандартный дочерний элемент
        x, y, width, height, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),							// Передаёт координаты и идентификатор элемента
        nullptr, nullptr);																						// Использует системный класс без дополнительных данных
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);			// Назначает системный шрифт
    return child;																								// Передаёт дескриптор созданного элемента
}																												// Завершает создание элемента
void createControls(HWND window) {																				// Создаёт панель ввода четырёх отделений
    control(window, L"STATIC", L"Размеры отделений, мм", 0, 28, 64, 210, 20);									// Подписывает панель размеров
    control(window, L"STATIC", L"Диаметр", 0, 127, 102, 67, 19);												// Подписывает столбец диаметров
    control(window, L"STATIC", L"Высота", 0, 199, 102, 66, 19);													// Подписывает столбец высот
    for (int i = 0; i < static_cast<int>(demoDimensions.size()); ++i) {											// Добавляет строку для каждого отделения
        const int y = 128 + i * 36;																				// Размещает строки с равными интервалами
        control(window, L"STATIC", sectionNames[i], 0, 28, y + 4, 96, 19);										// Подписывает строку названием отделения
        const auto& size = demoDimensions[i];																	// Берёт начальные значения диаметра и высоты
        wchar_t diameter[8]{};																					// Резервирует место для текста диаметра
        wchar_t height[8]{};																					// Резервирует место для текста высоты
        std::swprintf(diameter, 8, L"%d", size.diameter);														// Преобразует начальный диаметр в текст
        std::swprintf(height, 8, L"%d", size.height);															// Преобразует начальную высоту в текст
        constexpr DWORD inputStyle = ES_NUMBER | ES_RIGHT | ES_AUTOHSCROLL | WS_TABSTOP;						// Настраивает числовые поля ввода
        const HWND diameterInput = control(window, L"EDIT", diameter, inputStyle,								// Создаёт поле диаметра
            128, y, 63, 24, firstDimension + i * 2);															// Назначает полю диаметр текущей строки
        const HWND heightInput = control(window, L"EDIT", height, inputStyle,									// Создаёт поле высоты
            200, y, 63, 24, firstDimension + i * 2 + 1);														// Назначает полю высоту текущей строки
        SendMessageW(diameterInput, EM_SETLIMITTEXT, 4, 0);														// Ограничивает ввод диаметра четырьмя цифрами
        SendMessageW(heightInput, EM_SETLIMITTEXT, 4, 0);														// Ограничивает ввод высоты четырьмя цифрами
    }																											// Завершает заполнение строк
    control(window, L"BUTTON", L"Применить", BS_PUSHBUTTON | WS_TABSTOP,										// Добавляет кнопку принятия введённых размеров
        28, 279, 113, 30, applyDimensions);																		// Назначает кнопке положение и идентификатор
    control(window, L"STATIC", L"Учебные исходные размеры", 0, 28, 323, 235, 40, statusText);					// Показывает происхождение начальных размеров
}																												// Завершает построение панели
int readDimension(HWND window, int id) {																		// Читает один размер из поля ввода
    wchar_t text[6]{};																							// Принимает четыре цифры и конец строки
    const HWND field = GetDlgItem(window, id);																	// Находит поле по идентификатору
    const int length = GetWindowTextLengthW(field);																// Узнаёт фактическую длину ввода
    if (length < 1 || length > 4 || GetWindowTextW(field, text, 6) != length) {									// Проверяет наличие и длину значения
        return 0;																								// Обозначает недопустимое значение
    }																											// Завершает проверку длины
    int value = 0;																								// Накапливает значение по цифрам
    for (int i = 0; i < length; ++i) {																			// Обходит все символы поля
        if (text[i] < L'0' || text[i] > L'9') {																	// Требует обычную десятичную цифру
            return 0;																							// Отклоняет посторонние символы
        }																										// Завершает проверку символа
        value = value * 10 + (text[i] - L'0');																	// Добавляет цифру к целому значению
    }																											// Завершает чтение числа
    return value;																								// Возвращает размер в миллиметрах
}																												// Завершает чтение одного поля
void applyControls(HWND window, AppState& state) {																// Применяет размеры после проверки всех полей
    MoldDimensions edited{};																					// Собирает размеры без изменения текущего разреза
    for (int i = 0; i < static_cast<int>(edited.size()); ++i) {													// Считывает все четыре отделения
        edited[i].diameter = readDimension(window, firstDimension + i * 2);										// Считывает диаметр текущего отделения
        edited[i].height = readDimension(window, firstDimension + i * 2 + 1);									// Считывает высоту текущего отделения
        if (edited[i].diameter == 0 || edited[i].height == 0) {													// Обнаруживает пустой или недопустимый размер
            SetDlgItemTextW(window, statusText, L"Укажите целые размеры от 1 до 9999 мм");						// Показывает допустимый ввод
            return;																								// Сохраняет ранее применённые размеры
        }																										// Завершает проверку строки
    }																											// Завершает чтение всех отделений
    state.dimensions = edited;																					// Применяет весь набор размеров одновременно
    SetDlgItemTextW(window, statusText, L"Размеры применены");													// Подтверждает обновление разреза
    RECT view{};																								// Подготавливает область перерисовки
    GetClientRect(window, &view);																				// Берёт текущие границы клиентской области
    view.left = split;																							// Ограничивает перерисовку областью разреза
    InvalidateRect(window, &view, FALSE);																		// Запрашивает отрисовку новой геометрии
}																												// Завершает применение параметров
void drawContent(HDC device, const RECT& client, const MoldDimensions& dimensions) {							// Рисует холст и разрез формы
    FillRect(device, &client, GetSysColorBrush(COLOR_3DFACE));													// Закрашивает рабочую область системным фоном
    const RECT panel{16, 52, 16 + panelWidth, client.bottom - 16};												// Задаёт границы панели под полями
    FrameRect(device, &panel, GetSysColorBrush(COLOR_3DSHADOW));												// Обводит панель ввода
    const RECT area{split, 52, client.right - 16, client.bottom - 16};											// Задаёт границы правого холста
    FillRect(device, &area, GetSysColorBrush(COLOR_WINDOW));													// Закрашивает холст белым цветом
    FrameRect(device, &area, GetSysColorBrush(COLOR_3DSHADOW));													// Обводит холст разреза
    SelectObject(device, GetStockObject(DEFAULT_GUI_FONT));														// Выбирает системный шрифт заголовка
    SetBkMode(device, TRANSPARENT);																				// Сохраняет фон под заголовком
    SetTextColor(device, GetSysColor(COLOR_WINDOWTEXT));														// Использует системный цвет текста
    constexpr wchar_t heading[] = L"Разрез литейной формы";														// Задаёт название текущего вида
    TextOutW(device, split, 25, heading, static_cast<int>(std::size(heading)) - 1);								// Подписывает область разреза
    drawMold(device, area, dimensions);																			// Рисует разрез по применённым размерам
}																												// Завершает рисование рабочего вида
void paintWindow(HWND window, const MoldDimensions& dimensions) {												// Рисует кадр без промежуточного мерцания
    PAINTSTRUCT paint{};																						// Хранит сведения об области перерисовки
    const HDC target = BeginPaint(window, &paint);																// Получает контекст окна для рисования
    RECT client{};																								// Хранит размер рабочего поля окна
    GetClientRect(window, &client);																				// Получает действующие размеры клиентской области
    const HDC buffer = CreateCompatibleDC(target);																// Создаёт контекст для рисования в памяти
    const HBITMAP image = buffer ? CreateCompatibleBitmap(target, client.right, client.bottom) : nullptr;		// Создаёт снимок рабочего поля
    if (image) {																								// Проверяет наличие буфера полного кадра
        const HGDIOBJ old = SelectObject(buffer, image);														// Устанавливает изображение в буфер
        drawContent(buffer, client, dimensions);																// Рисует законченный кадр в памяти
        BitBlt(target, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY);								// Переносит кадр в окно одним вызовом
        SelectObject(buffer, old);																				// Восстанавливает исходное изображение буфера
        DeleteObject(image);																					// Освобождает завершённый кадр
    } else {																									// Обрабатывает нехватку ресурсов для буфера
        drawContent(target, client, dimensions);																// Рисует непосредственно в окно
    }																											// Завершает выбор способа рисования
    if (buffer) {																								// Проверяет наличие контекста в памяти
        DeleteDC(buffer);																						// Освобождает контекст буфера
    }																											// Завершает освобождение буфера
    EndPaint(window, &paint);																					// Завершает обработку сообщения рисования
}																												// Завершает отрисовку кадра
LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)						// Принимает сообщения главного окна
{																												// Начинает обработку сообщения
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));						// Получает состояние конкретного окна
    switch (message) {																							// Выбирает действие по типу сообщения
        case WM_NCCREATE: {																						// Обрабатывает ранний этап создания окна
            const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lParam);								// Получает параметры создания окна
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));		// Закрепляет состояние за окном
            break;																								// Передаёт Windows инициализацию заголовка и рамки
        }																										// Завершает привязку состояния
        case WM_CREATE:																							// Получает сообщение о создании главного окна
            createControls(window);																				// Добавляет поля размеров и кнопку
            return 0;																							// Подтверждает создание интерфейса
        case WM_GETMINMAXINFO: {																				// Ограничивает минимальный размер окна
            auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);												// Получает параметры масштабирования Windows
            limits->ptMinTrackSize = {840, 530};																// Оставляет место для панели и разреза
            return 0;																							// Применяет ограничения размера
        }																										// Завершает настройку размера
        case WM_COMMAND:																						// Получает команду от дочернего элемента
            if (LOWORD(wParam) == applyDimensions && HIWORD(wParam) == BN_CLICKED) {							// Распознаёт кнопку применения размеров
                applyControls(window, *state);																	// Проверяет и применяет введённый набор
                return 0;																						// Подтверждает обработку нажатия
            }																									// Завершает проверку команды
            break;																								// Передаёт остальные команды Windows
        case WM_PAINT:																							// Получает запрос на отрисовку
            paintWindow(window, state->dimensions);																// Отображает разрез из текущего состояния
            return 0;																							// Подтверждает перерисовку окна
        case WM_ERASEBKGND:																						// Пропускает отдельную очистку фона перед кадром
            return 1;																							// Сообщает Windows, что фон обработан
        case WM_DESTROY:																						// Проверяет, уничтожено ли главное окно
            PostQuitMessage(0);																					// Завершает цикл сообщений с кодом успеха
            return 0;																							// Подтверждает обработку сообщения
    }																											// Завершает выбор оконного сообщения
    return DefWindowProcW(window, message, wParam, lParam);														// Передаёт остальные сообщения стандартному обработчику Windows
}																												// Завершает оконную процедуру
}																												// Закрывает область локальных имён
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)										// Объявляет точку входа приложения Unicode
{																												// Начинает запуск приложения
    WNDCLASSW windowClass{};																					// Создаёт описание окна с нулевыми начальными полями
    windowClass.style = CS_HREDRAW | CS_VREDRAW;																// Запрашивает перерисовку при изменении размеров
    windowClass.lpfnWndProc = windowProcedure;																	// Связывает класс окна с обработчиком сообщений
    windowClass.hInstance = instance;																			// Связывает класс с текущим экземпляром приложения
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);														// Загружает системный указатель в виде стрелки
    windowClass.lpszClassName = windowClassName;																// Задаёт имя для регистрации и создания окна
    if (RegisterClassW(&windowClass) == 0) {																	// Проверяет успешность регистрации класса в Windows
        MessageBoxW(nullptr, L"Не удалось зарегистрировать класс окна.", L"RollCast", MB_OK | MB_ICONERROR);	// Показывает ошибку запуска
        return 1;																								// Возвращает код ошибки регистрации
    }																											// Завершает проверку регистрации
    AppState state;																								// Хранит размеры на протяжении работы окна
    const HWND window = CreateWindowExW(																		// Создаёт главное окно приложения
        0, windowClassName, L"RollCast",																		// Использует обычные параметры окна и название приложения
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,																	// Сохраняет рамку и исключает поля из рисования
        CW_USEDEFAULT, CW_USEDEFAULT, 1240, 800,																// Задаёт начальный размер и системную позицию
        nullptr, nullptr, instance, &state);																	// Передаёт окну экземпляр приложения и состояние
    if (window == nullptr) {																					// Проверяет, получен ли дескриптор окна
        MessageBoxW(nullptr, L"Не удалось создать главное окно.", L"RollCast", MB_OK | MB_ICONERROR);			// Показывает ошибку создания окна
        return 1;																								// Возвращает код ошибки создания окна
    }																											// Завершает проверку создания окна
    ShowWindow(window, showCommand);																			// Показывает окно в режиме запуска Windows
    MSG message{};																								// Хранит очередное сообщение Windows
    BOOL status = 0;																							// Хранит результат чтения сообщения из очереди
    while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {												// Ожидает сообщения до команды завершения или ошибки
        if (!IsDialogMessageW(window, &message)) {																// Сначала обрабатывает переход между полями
            TranslateMessage(&message);																			// Преобразует нажатия клавиш в символьные сообщения
            DispatchMessageW(&message);																			// Отправляет сообщение оконной процедуре
        }																										// Завершает обработку сообщения
    }																											// Завершает цикл после команды выхода или ошибки
    if (status == -1) {																							// Проверяет, произошла ли ошибка чтения очереди
        MessageBoxW(nullptr, L"Не удалось прочитать сообщение Windows.", L"RollCast", MB_OK | MB_ICONERROR);	// Сообщает об ошибке очереди
        return 1;																								// Возвращает код ошибки цикла сообщений
    }																											// Завершает проверку результата цикла
    return static_cast<int>(message.wParam);																	// Возвращает код из сообщения о завершении
}																												// Завершает работу приложения
