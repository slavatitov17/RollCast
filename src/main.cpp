#include "cooling_view.h"																								// Подключает разрез и график охлаждения
#include <windowsx.h>																									// Извлекает координаты указателя из оконного сообщения
#include <cwchar>																										// Предоставляет безопасный разбор целых размеров
namespace {																												// Ограничивает видимость имён этим файлом
constexpr wchar_t windowClassName[] = L"RollCastMainWindow";															// Указывает имя класса главного окна
constexpr int firstDimension = 100;																						// Задаёт начальный идентификатор полей размеров
constexpr int applyDimensions = 108;																					// Задаёт идентификатор кнопки применения размеров
constexpr int coolingButton = 110;																						// Задаёт кнопку переключения расчётного графика
constexpr int statusText = 109;																							// Задаёт идентификатор строки проверки
constexpr int panelWidth = 256;																							// Задаёт ширину панели ввода
constexpr int split = 284;																								// Отделяет панель ввода от разреза
constexpr const wchar_t* sectionNames[]{																				// Перечисляет отделения в порядке модели
    L"Нижняя шейка", L"Бочка", L"Верхняя шейка", L"Прибыль"																// Задаёт названия строк панели ввода
};																														// Завершает список названий
struct AppState {																										// Хранит состояние текущего окна
    MoldDimensions dimensions = demoDimensions;																			// Начинает с условных демонстрационных размеров
    MoldHitMap hits;																									// Хранит экранные области деталей формы
    MoldPart hovered = MoldPart::none;																					// Начинает без выделенного элемента
    POINT cursor{};																										// Хранит позицию для подписи элемента
    bool trackingMouse = false;																							// Отслеживает выход курсора из окна
    bool dimensionsAccepted = false;																					// Отмечает успешное применение размеров
    bool showCooling = false;																							// Начинает с отображения формы
    int coolingDiameter = 0;																							// Запоминает диаметр последнего расчёта
    CoolingResult cooling{};																							// Хранит рассчитанные кривые охлаждения
};																														// Завершает описание состояния окна
HWND control(HWND window, const wchar_t* type, const wchar_t* text,														// Описывает тип, текст и родителя элемента
             DWORD style, int x, int y, int width, int height, int id = 0) {											// Принимает размер, положение и идентификатор
    const DWORD frame = type[0] == L'E' ? WS_EX_CLIENTEDGE : 0;															// Добавляет системную рамку полям ввода
    const HWND child = CreateWindowExW(frame, type, text, WS_CHILD | WS_VISIBLE | style,								// Создаёт стандартный дочерний элемент
        x, y, width, height, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),									// Передаёт координаты и идентификатор элемента
        nullptr, nullptr);																								// Использует системный класс без дополнительных данных
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);					// Назначает системный шрифт
    return child;																										// Передаёт дескриптор созданного элемента
}																														// Завершает создание элемента
void createControls(HWND window) {																						// Создаёт панель ввода четырёх отделений
    control(window, L"STATIC", L"Учебные исходные размеры", 0, 28, 64, 235, 22, statusText);							// Подписывает исходные данные и выводит результат применения
    control(window, L"STATIC", L"Диаметр", 0, 127, 102, 67, 19);														// Подписывает столбец диаметров
    control(window, L"STATIC", L"Высота", 0, 199, 102, 66, 19);															// Подписывает столбец высот
    for (int i = 0; i < static_cast<int>(demoDimensions.size()); ++i) {													// Добавляет строку для каждого отделения
        const int y = 128 + i * 36;																						// Размещает строки с равными интервалами
        control(window, L"STATIC", sectionNames[i], 0, 28, y + 4, 96, 19);												// Подписывает строку названием отделения
        const auto& size = demoDimensions[i];																			// Берёт начальные значения диаметра и высоты
        wchar_t diameter[8]{};																							// Резервирует место для текста диаметра
        wchar_t height[8]{};																							// Резервирует место для текста высоты
        std::swprintf(diameter, 8, L"%d", size.diameter);																// Преобразует начальный диаметр в текст
        std::swprintf(height, 8, L"%d", size.height);																	// Преобразует начальную высоту в текст
        constexpr DWORD inputStyle = ES_NUMBER | ES_RIGHT | ES_AUTOHSCROLL | WS_TABSTOP;								// Настраивает числовые поля ввода
        const HWND diameterInput = control(window, L"EDIT", diameter, inputStyle,										// Создаёт поле диаметра
            128, y, 63, 24, firstDimension + i * 2);																	// Назначает полю диаметр текущей строки
        const HWND heightInput = control(window, L"EDIT", height, inputStyle,											// Создаёт поле высоты
            200, y, 63, 24, firstDimension + i * 2 + 1);																// Назначает полю высоту текущей строки
        SendMessageW(diameterInput, EM_SETLIMITTEXT, 4, 0);																// Ограничивает ввод диаметра четырьмя цифрами
        SendMessageW(heightInput, EM_SETLIMITTEXT, 4, 0);																// Ограничивает ввод высоты четырьмя цифрами
    }																													// Завершает заполнение строк
    control(window, L"BUTTON", L"Применить", BS_PUSHBUTTON | WS_TABSTOP,												// Добавляет кнопку принятия введённых размеров
        28, 279, 113, 30, applyDimensions);																				// Назначает кнопке положение и идентификатор
    control(window, L"BUTTON", L"Охлаждение бочки", BS_PUSHBUTTON | WS_TABSTOP,											// Добавляет переход к температурному графику
        28, 338, 157, 30, coolingButton);																				// Назначает положение и идентификатор кнопки
}																														// Завершает построение панели
int readDimension(HWND window, int id) {																				// Читает один размер из поля ввода
    wchar_t text[6]{};																									// Принимает четыре цифры и конец строки
    const HWND field = GetDlgItem(window, id);																			// Находит поле по идентификатору
    const int length = GetWindowTextLengthW(field);																		// Узнаёт фактическую длину ввода
    if (length < 1 || length > 4 || GetWindowTextW(field, text, 6) != length) {											// Проверяет наличие и длину значения
        return 0;																										// Обозначает недопустимое значение
    }																													// Завершает проверку длины
    int value = 0;																										// Накапливает значение по цифрам
    for (int i = 0; i < length; ++i) {																					// Обходит все символы поля
        if (text[i] < L'0' || text[i] > L'9') {																			// Требует обычную десятичную цифру
            return 0;																									// Отклоняет посторонние символы
        }																												// Завершает проверку символа
        value = value * 10 + (text[i] - L'0');																			// Добавляет цифру к целому значению
    }																													// Завершает чтение числа
    return value;																										// Возвращает размер в миллиметрах
}																														// Завершает чтение одного поля
void refreshCooling(AppState& state) {																					// Пересчитывает график при изменении диаметра бочки
    const int diameter = state.dimensions[1].diameter;																	// Получает применённый диаметр бочки
    if (state.coolingDiameter != diameter) {																			// Проверяет соответствие ранее рассчитанных температур
        state.cooling = demonstrationCooling(diameter);																	// Запускает радиальный расчёт охлаждения бочки
        state.coolingDiameter = diameter;																				// Запоминает диаметр показанных кривых
    }																													// Завершает проверку необходимости перерасчёта
}																														// Завершает подготовку графика
void redrawView(HWND window) {																							// Запрашивает перерисовку правой панели и заголовка
    RECT view{};																										// Подготавливает прямоугольник клиентской области
    GetClientRect(window, &view);																						// Измеряет текущую область приложения
    view.left = split;																									// Ограничивает перерисовку правой частью окна
    InvalidateRect(window, &view, FALSE);																				// Перерисовывает правую панель без отдельной очистки
}																														// Завершает запрос на обновление
void applyControls(HWND window, AppState& state) {																		// Применяет размеры после проверки всех полей
    MoldDimensions edited{};																							// Собирает размеры без изменения текущего разреза
    for (int i = 0; i < static_cast<int>(edited.size()); ++i) {															// Считывает все четыре отделения
        edited[i].diameter = readDimension(window, firstDimension + i * 2);												// Считывает диаметр текущего отделения
        edited[i].height = readDimension(window, firstDimension + i * 2 + 1);											// Считывает высоту текущего отделения
        if (edited[i].diameter == 0 || edited[i].height == 0) {															// Обнаруживает пустой или недопустимый размер
            state.dimensionsAccepted = false;																			// Возвращает обычный цвет сообщения проверки
            SetDlgItemTextW(window, statusText, L"Укажите целые размеры от 1 до 9999 мм");								// Показывает допустимый ввод
            return;																										// Сохраняет ранее применённые размеры
        }																												// Завершает проверку строки
    }																													// Завершает чтение всех отделений
    state.dimensions = edited;																							// Применяет весь набор размеров одновременно
    state.dimensionsAccepted = true;																					// Включает зелёное подтверждение применения
    SetDlgItemTextW(window, statusText, L"✓  Размеры применены");														// Выводит галочку перед сообщением об успехе
    state.hovered = MoldPart::none;																						// Снимает подсветку при изменении геометрии
    state.hits.clear();																									// Удаляет старые области деталей формы
    if (state.showCooling) {																							// Проверяет видимость температурного графика
        refreshCooling(state);																							// Обновляет график для нового диаметра бочки
    }																													// Завершает проверку текущего вида
    redrawView(window);																									// Отображает новую геометрию или температуры
}																														// Завершает применение параметров
void drawContent(HDC device, const RECT& client, AppState& state) {														// Рисует холст и разрез формы
    FillRect(device, &client, GetSysColorBrush(COLOR_3DFACE));															// Закрашивает рабочую область системным фоном
    const RECT panel{16, 52, 16 + panelWidth, client.bottom - 16};														// Задаёт границы панели под полями
    FrameRect(device, &panel, GetSysColorBrush(COLOR_3DSHADOW));														// Обводит панель ввода
    const RECT area{split, 52, client.right - 16, client.bottom - 16};													// Задаёт границы правого холста
    FillRect(device, &area, GetSysColorBrush(COLOR_WINDOW));															// Закрашивает холст белым цветом
    FrameRect(device, &area, GetSysColorBrush(COLOR_3DSHADOW));															// Обводит холст разреза
    SelectObject(device, GetStockObject(DEFAULT_GUI_FONT));																// Выбирает системный шрифт заголовка
    SetBkMode(device, TRANSPARENT);																						// Сохраняет фон под заголовком
    SetTextColor(device, GetSysColor(COLOR_WINDOWTEXT));																// Использует системный цвет текста
    constexpr wchar_t panelHeading[] = L"Размеры отделений, мм";														// Задаёт заголовок над рамкой панели
    TextOutW(device, 16, 25, panelHeading, static_cast<int>(std::size(panelHeading)) - 1);								// Выводит название панели размеров
    const wchar_t* heading = state.showCooling ? L"Охлаждение бочки" : L"Форма в сборе для заливки валка";				// Выбирает заголовок текущего вида
    TextOutW(device, split, 25, heading, static_cast<int>(std::wcslen(heading)));										// Выводит заголовок над правой рамкой
    if (state.showCooling) {																							// Проверяет какой вид открыт справа
        drawCooling(device, area, state.cooling, state.coolingDiameter);												// Строит две температурные кривые
    } else {																											// Обрабатывает основной вид формы
        drawMold(device, area, state.dimensions, state.hits, state.hovered, state.cursor);								// Рисует форму и подсказку
    }																													// Завершает выбор содержимого панели
}																														// Завершает рисование рабочего вида
void paintWindow(HWND window, AppState& state) {																		// Рисует кадр без промежуточного мерцания
    PAINTSTRUCT paint{};																								// Хранит сведения об области перерисовки
    const HDC target = BeginPaint(window, &paint);																		// Получает контекст окна для рисования
    RECT client{};																										// Хранит размер рабочего поля окна
    GetClientRect(window, &client);																						// Получает действующие размеры клиентской области
    const HDC buffer = CreateCompatibleDC(target);																		// Создаёт контекст для рисования в памяти
    const HBITMAP image = buffer ? CreateCompatibleBitmap(target, client.right, client.bottom) : nullptr;				// Создаёт снимок рабочего поля
    if (image) {																										// Проверяет наличие буфера полного кадра
        const HGDIOBJ old = SelectObject(buffer, image);																// Устанавливает изображение в буфер
        drawContent(buffer, client, state);																				// Рисует законченный кадр в памяти
        BitBlt(target, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY);										// Переносит кадр в окно одним вызовом
        SelectObject(buffer, old);																						// Восстанавливает исходное изображение буфера
        DeleteObject(image);																							// Освобождает завершённый кадр
    } else {																											// Обрабатывает нехватку ресурсов для буфера
        drawContent(target, client, state);																				// Рисует непосредственно в окно
    }																													// Завершает выбор способа рисования
    if (buffer) {																										// Проверяет наличие контекста в памяти
        DeleteDC(buffer);																								// Освобождает контекст буфера
    }																													// Завершает освобождение буфера
    EndPaint(window, &paint);																							// Завершает обработку сообщения рисования
}																														// Завершает отрисовку кадра
LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)								// Принимает сообщения главного окна
{																														// Начинает обработку сообщения
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));								// Получает состояние конкретного окна
    switch (message) {																									// Выбирает действие по типу сообщения
        case WM_NCCREATE: {																								// Обрабатывает ранний этап создания окна
            const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lParam);										// Получает параметры создания окна
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));				// Закрепляет состояние за окном
            break;																										// Передаёт Windows инициализацию заголовка и рамки
        }																												// Завершает привязку состояния
        case WM_CREATE:																									// Получает сообщение о создании главного окна
            createControls(window);																						// Добавляет поля размеров и кнопку
            return 0;																									// Подтверждает создание интерфейса
        case WM_GETMINMAXINFO: {																						// Ограничивает минимальный размер окна
            auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);														// Получает параметры масштабирования Windows
            limits->ptMinTrackSize = {840, 530};																		// Оставляет место для панели и разреза
            return 0;																									// Применяет ограничения размера
        }																												// Завершает настройку размера
        case WM_COMMAND:																								// Получает команду от дочернего элемента
            if (LOWORD(wParam) == applyDimensions && HIWORD(wParam) == BN_CLICKED) {									// Распознаёт кнопку применения размеров
                applyControls(window, *state);																			// Проверяет и применяет введённый набор
                return 0;																								// Подтверждает обработку нажатия
            }																											// Завершает проверку команды
            if (LOWORD(wParam) == coolingButton && HIWORD(wParam) == BN_CLICKED) {										// Распознаёт переключение вида
                state->showCooling = !state->showCooling;																// Переключает разрез и график справа
                state->hits.clear();																					// Удаляет карту предыдущего вида
                state->hovered = MoldPart::none;																		// Сбрасывает выделение деталей формы
                if (state->showCooling) {																				// Проверяет переход к графику
                    refreshCooling(*state);																				// Получает температуры для отображения
                }																										// Завершает подготовку графика
                SetDlgItemTextW(window, coolingButton, state->showCooling ? L"Показать форму" : L"Охлаждение бочки");	// Обновляет подпись переключателя
                redrawView(window);																						// Отображает выбранный вид
                return 0;																								// Завершает обработку кнопки
            }																											// Завершает обработку переключения вида
            break;																										// Передаёт остальные команды Windows
        case WM_CTLCOLORSTATIC: {																						// Позволяет окрасить строку состояния
            if (state->dimensionsAccepted && reinterpret_cast<HWND>(lParam) == GetDlgItem(window, statusText)) {		// Выбирает подтверждение применения
                const auto context = reinterpret_cast<HDC>(wParam);														// Получает контекст сообщения состояния
                SetTextColor(context, RGB(22, 107, 54));																// Окрашивает текст и галочку в зелёный
                SetBkColor(context, GetSysColor(COLOR_3DFACE));															// Сохраняет системный фон панели
                return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_3DFACE));										// Передаёт цвет фона системному элементу
            }																											// Завершает настройку цвета строки
            break;																										// Оставляет обычные подписи системного цвета
        }																												// Завершает обработку цвета элементов
        case WM_MOUSEMOVE: {																							// Отслеживает указатель над деталями формы
            if (state->showCooling) {																					// Проверяет отображение температурного графика
                break;																									// Пропускает поиск детали вне вида формы
            }																											// Завершает проверку текущего вида
            if (!state->trackingMouse) {																				// Проверяет наличие регистрации выхода курсора
                TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};												// Запрашивает сообщение об уходе указателя
                state->trackingMouse = TrackMouseEvent(&track) != FALSE;												// Запоминает успешную регистрацию ухода
            }																											// Завершает включение слежения за курсором
            const POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};												// Получает положение указателя относительно окна
            const MoldPart part = state->hits.partAt(cursor);															// Ищет верхнюю деталь под указателем
            if (part != MoldPart::none || state->hovered != MoldPart::none) {											// Проверяет изменение видимого выделения
                state->hovered = part;																					// Меняет активную деталь и приглушение остальных
                state->cursor = cursor;																					// Запоминает место для всплывающей подписи
                redrawView(window);																						// Перемещает подпись и обновляет подсветку
            }																											// Завершает отрисовку при наведении
            return 0;																									// Подтверждает обработку движения курсора
        }																												// Завершает обработку наведения
        case WM_MOUSELEAVE:																								// Получает сообщение об уходе указателя
            state->trackingMouse = false;																				// Разрешает зарегистрировать следующий вход
            if (state->hovered != MoldPart::none) {																		// Проверяет наличие выделенной детали
                state->hovered = MoldPart::none;																		// Убирает выделение и подпись
                redrawView(window);																						// Восстанавливает обычный вид элементов
            }																											// Завершает снятие подсветки
            return 0;																									// Подтверждает обработку ухода указателя
        case WM_PAINT:																									// Получает запрос на отрисовку
            paintWindow(window, *state);																				// Отображает разрез из текущего состояния
            return 0;																									// Подтверждает перерисовку окна
        case WM_ERASEBKGND:																								// Пропускает отдельную очистку фона перед кадром
            return 1;																									// Сообщает Windows, что фон обработан
        case WM_DESTROY:																								// Проверяет, уничтожено ли главное окно
            PostQuitMessage(0);																							// Завершает цикл сообщений с кодом успеха
            return 0;																									// Подтверждает обработку сообщения
    }																													// Завершает выбор оконного сообщения
    return DefWindowProcW(window, message, wParam, lParam);																// Передаёт остальные сообщения стандартному обработчику Windows
}																														// Завершает оконную процедуру
}																														// Закрывает область локальных имён
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)															// Объявляет точку входа приложения Unicode
{																														// Начинает запуск приложения
    WNDCLASSW windowClass{};																							// Создаёт описание окна с нулевыми начальными полями
    windowClass.style = CS_HREDRAW | CS_VREDRAW;																		// Запрашивает перерисовку при изменении размеров
    windowClass.lpfnWndProc = windowProcedure;																			// Связывает класс окна с обработчиком сообщений
    windowClass.hInstance = instance;																					// Связывает класс с текущим экземпляром приложения
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);																// Загружает системный указатель в виде стрелки
    windowClass.lpszClassName = windowClassName;																		// Задаёт имя для регистрации и создания окна
    if (RegisterClassW(&windowClass) == 0) {																			// Проверяет успешность регистрации класса в Windows
        MessageBoxW(nullptr, L"Не удалось зарегистрировать класс окна.", L"RollCast", MB_OK | MB_ICONERROR);			// Показывает ошибку запуска
        return 1;																										// Возвращает код ошибки регистрации
    }																													// Завершает проверку регистрации
    AppState state;																										// Хранит размеры на протяжении работы окна
    const HWND window = CreateWindowExW(																				// Создаёт главное окно приложения
        0, windowClassName, L"RollCast",																				// Использует обычные параметры окна и название приложения
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,																			// Сохраняет рамку и исключает поля из рисования
        CW_USEDEFAULT, CW_USEDEFAULT, 1240, 800,																		// Задаёт начальный размер и системную позицию
        nullptr, nullptr, instance, &state);																			// Передаёт окну экземпляр приложения и состояние
    if (window == nullptr) {																							// Проверяет, получен ли дескриптор окна
        MessageBoxW(nullptr, L"Не удалось создать главное окно.", L"RollCast", MB_OK | MB_ICONERROR);					// Показывает ошибку создания окна
        return 1;																										// Возвращает код ошибки создания окна
    }																													// Завершает проверку создания окна
    ShowWindow(window, SW_SHOWMAXIMIZED);																				// Разворачивает окно с сохранением системных кнопок
    MSG message{};																										// Хранит очередное сообщение Windows
    BOOL status = 0;																									// Хранит результат чтения сообщения из очереди
    while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {														// Ожидает сообщения до команды завершения или ошибки
        const int focusedId = GetDlgCtrlID(message.hwnd);																// Определяет идентификатор элемента с фокусом
        const bool editing = focusedId >= firstDimension && focusedId <= applyDimensions;								// Распознаёт поля размеров и кнопку применения
        if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN && (editing || message.hwnd == window)) {		// Проверяет команду Enter
            applyControls(window, state);																				// Применяет параметры тем же действием что кнопка
            continue;																									// Завершает обработку Enter без повторного нажатия
        }																												// Завершает обработку клавиши применения
        if (!IsDialogMessageW(window, &message)) {																		// Обрабатывает переход между полями
            TranslateMessage(&message);																					// Преобразует нажатия клавиш в символьные сообщения
            DispatchMessageW(&message);																					// Отправляет сообщение оконной процедуре
        }																												// Завершает обработку сообщения
    }																													// Завершает цикл после команды выхода или ошибки
    if (status == -1) {																									// Проверяет, произошла ли ошибка чтения очереди
        MessageBoxW(nullptr, L"Не удалось прочитать сообщение Windows.", L"RollCast", MB_OK | MB_ICONERROR);			// Сообщает об ошибке очереди
        return 1;																										// Возвращает код ошибки цикла сообщений
    }																													// Завершает проверку результата цикла
    return static_cast<int>(message.wParam);																			// Возвращает код из сообщения о завершении
}																														// Завершает работу приложения
