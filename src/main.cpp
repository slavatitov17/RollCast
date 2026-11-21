#include "about_view.h"	// Подключает встроенный разбор теории и кода
#include "cooling_view.h"																																	// Подключает быстрые графики и структуру результата
#include <commctrl.h>																																		// Предоставляет вкладки список и ползунок
#include <commdlg.h>																																		// Предоставляет диалоги открытия и сохранения
#include <gdiplus.h>																																		// Сохраняет изображения графиков в PNG
#include <windowsx.h>																																		// Извлекает координаты мыши из сообщений
#include <algorithm>																																		// Ограничивает размеры панелей и временные индексы
#include <cmath>																																			// Проверяет конечность введённых чисел
#include <charconv>																																			// Сохраняет точность отображаемых исходных чисел
#include <cwchar>																																			// Преобразует числовые поля интерфейса
#include <memory>																																			// Управляет временем жизни рассчитанных результатов
#include <thread>																																			// Выполняет расчёт вне оконного потока
#include <stdexcept>																																		// Передаёт ошибки проверки пользователю
namespace {																																					// Ограничивает видимость оконной реализации
constexpr UINT calculationDone = WM_APP + 1;																												// Задаёт сообщение завершения фоновой работы
constexpr int numberCount = 38;																																// Задаёт количество числовых полей проекта
constexpr int firstNumber = 100;																															// Задаёт первый идентификатор числового поля
constexpr int tabsId = 202;																																	// Задаёт идентификатор вкладок параметров
constexpr int sliderId = 205;																																// Задаёт идентификатор временного ползунка
constexpr int clockId = 206;																																// Задаёт идентификатор временной подписи
constexpr int chartPicker = 207;																															// Задаёт идентификатор выбора графиков
constexpr int firstExport = 300;																															// Задаёт первый идентификатор экспорта графика
constexpr int fillingEnd = 2000;																															// Отводит начало шкалы анимации заполнения
constexpr int timelineEnd = 10000;																															// Задаёт разрешение общей временной шкалы
struct Layout { RECT client; RECT parameters; RECT mold; RECT results; };																					// Хранит области единого рабочего экрана
Layout layout(HWND window) {																																// Рассчитывает компоновку по размеру клиентской области
    RECT client{};																																			// Принимает доступную область окна
    GetClientRect(window, &client);																															// Читает размеры без системной рамки
    const int width = std::clamp((client.right - 416) / 3, 240L, 400L);																						// Выделяет ширину для центральной схемы
    return {client, {16, 80, 376, client.bottom - 16}, {388, 80, 388 + width, client.bottom - 95}, {400 + width, 80, client.right - 16, client.bottom - 16}};	// Располагает параметры форму и результаты
}																																							// Завершает расчёт компоновки
struct Surface {																																			// Хранит повторно используемый буфер рисования
    HDC dc = nullptr;																																		// Хранит контекст буфера
    HBITMAP bitmap = nullptr;																																// Хранит растровую поверхность
    HGDIOBJ previous = nullptr;																																// Сохраняет исходный объект контекста
    int width = 0;																																			// Запоминает ширину буфера
    int height = 0;																																			// Запоминает высоту буфера
    ~Surface() { clear(); }																																	// Освобождает графические ресурсы
    void clear() {																																			// Уничтожает поверхность перед изменением размера
        if (dc && previous) { SelectObject(dc, previous); }																									// Возвращает исходный объект контекста
        if (bitmap) { DeleteObject(bitmap); }																												// Освобождает растровый буфер
        if (dc) { DeleteDC(dc); }																															// Освобождает графический контекст
        dc = nullptr; bitmap = nullptr; previous = nullptr;																									// Обнуляет освобождённые дескрипторы
    }																																						// Завершает очистку поверхности
    void ensure(HDC target, int w, int h) {																													// Меняет буфер только после изменения размеров
        if (dc && width == w && height == h) { return; }																									// Переиспользует подходящую поверхность
        clear();																																			// Освобождает прежнюю поверхность
        dc = CreateCompatibleDC(target);																													// Создаёт контекст рисования в памяти
        bitmap = CreateCompatibleBitmap(target, std::max(1, w), std::max(1, h));																			// Создаёт полноразмерный растровый буфер
        if (!dc || !bitmap) { clear(); throw std::runtime_error("Недостаточно графической памяти"); }														// Отклоняет неудачное создание буфера
        previous = SelectObject(dc, bitmap);																												// Выбирает новую поверхность
        width = w; height = h;																																// Запоминает действительный размер
    }																																						// Завершает подготовку буфера
};																																							// Завершает описание графической поверхности
struct Completed {																																			// Передаёт результат из фонового потока
    unsigned long long generation;																															// Различает актуальные и отменённые задания
    std::shared_ptr<Run> result;																															// Передаёт полностью рассчитанный проект
    std::shared_ptr<Run> baseline;																															// Передаёт восстановленную закреплённую базу
    std::wstring error;																																		// Передаёт диагностическое сообщение
};																																							// Завершает сообщение фонового расчёта
struct AppState {																																			// Хранит состояние оконного приложения
    Project inputs;																																			// Хранит последний принятый набор исходных данных
    std::shared_ptr<Run> result;																															// Хранит опубликованный неизменяемый результат
    std::shared_ptr<Run> baseline;																															// Хранит закреплённый результат сравнения
    std::optional<Project> baselineInputs;																													// Позволяет восстановить базу после перезапуска
    std::jthread worker;																																	// Управляет отменяемым фоновым расчётом
    unsigned long long generation = 0;																														// Нумерует расчётные задания
    bool running = false;																																	// Отмечает выполнение фоновой работы
    bool changing = false;																																	// Подавляет уведомления при заполнении полей
    bool unsaved = false;																																	// Отмечает несохранённые пользовательские изменения
    bool edited = false;																																	// Требует пересчёта после изменения входов
    int position = 0;																																		// Хранит положение общей временной шкалы
    int selection = 0;																																		// Начинает с отображения всех графиков
    int tab = 0;																																			// Начинает с геометрических параметров
    bool automatic = false;																																	// Управляет автоматической анимацией заполнения
    std::array<std::vector<HWND>, 2> groups;																												// Хранит элементы трёх вкладок
    ChartCache curves;																																		// Кэширует компактные температурные линии
    MoldHitMap hits;																																		// Хранит области наведения и полости металла
    RECT moldGeometry{};																																	// Запоминает область построенной геометрии
    bool rebuildMold = true;																																// Отмечает необходимость пересоздать области формы
    MoldPart hovered = MoldPart::none;																														// Хранит выделенную деталь формы
    POINT cursor{};																																			// Хранит положение подписи
    bool tracking = false;																																	// Отслеживает уход мыши со схемы
    Surface buffer;																																			// Переиспользует графический буфер окна
    std::filesystem::path directory;																														// Хранит каталог программы и локальной базы
    std::filesystem::path filename;																															// Хранит путь последнего файла проекта
};																																							// Завершает состояние приложения
std::wstring wide(const std::string& text) {																												// Преобразует UTF-8 в строки Windows
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);							// Определяет необходимый размер строки
    if (size == 0 && !text.empty()) { throw std::runtime_error("Некорректная кодировка текста"); }															// Проверяет допустимость UTF-8
    std::wstring result(size, 0);																															// Выделяет память строки Unicode
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);														// Преобразует исходный текст
    return result;																																			// Возвращает строку для Windows
}																																							// Завершает преобразование в Unicode
std::wstring text(HWND field) {																																// Читает полное содержимое поля
    const int size = GetWindowTextLengthW(field);																											// Узнаёт длину текста
    std::wstring value(size + 1, 0);																														// Выделяет место вместе с завершающим нулём
    value.resize(GetWindowTextW(field, value.data(), size + 1));																							// Получает фактическое содержимое поля
    return value;																																			// Возвращает введённую строку
}																																							// Завершает чтение текста
HWND control(HWND window, const wchar_t* type, const wchar_t* caption, DWORD style, int x, int y, int w, int h, int id = 0) {								// Создаёт стандартный элемент интерфейса
    const HWND child = CreateWindowExW(type[0] == L'E' ? WS_EX_CLIENTEDGE : 0, type, caption, WS_CHILD | WS_VISIBLE | style, x, y, w, h, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);	// Передаёт Windows параметры элемента
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);														// Использует системный шрифт
    return child;																																			// Возвращает дескриптор созданного элемента
}																															// Завершает создание элемента
void groupControl(HWND window, AppState& state, int tab, const wchar_t* type, const wchar_t* caption, int x, int y, int w, int h, int id = 0) {				// Добавляет элемент в группу вкладки
    const DWORD style = type[0] == L'E' ? ES_AUTOHSCROLL | WS_TABSTOP : 0;																					// Настраивает текстовый ввод
    state.groups[tab].push_back(control(window, type, caption, style, x, y, w, h, id));																		// Запоминает элемент выбранной страницы
}																																							// Завершает добавление элемента вкладки
std::array<double, numberCount> numbers(const Project& p) {	// Сопоставляет входы видимым числовым полям
    std::array<double, numberCount> values{};	// Подготавливает единый порядок редактируемых параметров
    for (std::size_t i = 0; i < sectionCount; ++i) { values[2 * i] = p.dimensions[i].diameter; values[2 * i + 1] = p.dimensions[i].height; }	// Связывает пары D и L с номерами
    const double geometry[]{p.chill, p.coating, p.barrelFlask, p.sand, p.flask, p.pipeDiameter, p.hours * 60, p.step, p.diameterStep};	// Перечисляет стенки и численные параметры
    std::copy(std::begin(geometry), std::end(geometry), values.begin() + 10);	// Переносит геометрию и настройки сетки
    for (std::size_t i = 0; i < 4; ++i) { values[19 + 3 * i] = p.materials[i].density; values[20 + 3 * i] = p.materials[i].heatCapacity; values[21 + 3 * i] = p.materials[i].conductivity; }	// Сохраняет три свойства каждого материала
    const double thermal[]{p.phase.latentHeat, p.phase.liquidus, p.phase.solidus, p.metalTemperature, p.formTemperature, p.airTemperature, p.externalTransfer};	// Перечисляет фазовые и температурные параметры
    std::copy(std::begin(thermal), std::end(thermal), values.begin() + 31);	// Заполняет оставшиеся поля материалов
    return values;	// Возвращает согласованный набор полей
}	// Завершает подготовку числового редактора
Project readProject(HWND window, const AppState& state) {	// Читает обе вкладки перед явным расчётом
    Project p = state.inputs; std::array<double, numberCount> values{};	// Подготавливает независимый кандидат входных данных
    for (int i = 0; i < numberCount; ++i) {	// Проверяет каждое редактируемое значение
        auto value = text(GetDlgItem(window, firstNumber + i));	// Получает полное содержимое числового поля
        std::replace(value.begin(), value.end(), L',', L'.');	// Разрешает десятичную запятую русского ввода
        wchar_t* end = nullptr; values[i] = std::wcstod(value.c_str(), &end);	// Разбирает десятичное числовое значение
        if (value.empty() || end != value.c_str() + value.size() || !std::isfinite(values[i])) { throw std::runtime_error("Заполните все числовые поля корректными значениями"); }	// Отклоняет неполные и нечисловые строки
    }	// Завершает разбор числовых полей
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Восстанавливает пять пар геометрических размеров
        const double d = values[2 * i], length = values[2 * i + 1];	// Выбирает соответствующие D и L
        if (d != std::floor(d) || length != std::floor(length) || d < 1 || d > 9999 || length < 1 || length > 9999) { throw std::runtime_error("D1–D5 и L1–L5 задаются целыми миллиметрами 1–9999"); }	// Проверяет целочисленные размеры перед преобразованием
        p.dimensions[i] = {static_cast<int>(d), static_cast<int>(length)};	// Применяет размеры к своему отделению
    }	// Завершает восстановление пяти цилиндров
    p.chill = values[10]; p.coating = values[11]; p.barrelFlask = values[12];	// Применяет стенки кокиля краски и опоки
    p.sand = values[13]; p.flask = values[14]; p.pipeDiameter = values[15];	// Применяет песчаную форму и литниковую трубу
    p.hours = values[16] / 60; p.step = values[17]; p.diameterStep = values[18];	// Переводит минуты и применяет численную сетку
    for (std::size_t i = 0; i < 4; ++i) { p.materials[i] = {values[19 + 3 * i], values[20 + 3 * i], values[21 + 3 * i]}; }	// Восстанавливает свойства четырёх материалов
    p.phase = {values[33], values[32], values[31]};	// Сохраняет порядок солидус ликвидус скрытая теплота
    p.metalTemperature = values[34]; p.formTemperature = values[35];	// Применяет начальные температуры металла и формы
    p.airTemperature = values[36]; p.externalTransfer = values[37];	// Применяет наружное конвективное граничное условие
    validateProject(p); return p;	// Возвращает только согласованный набор входов
}	// Завершает чтение обеих вкладок
void updateDerived(HWND window, AppState& state) {	// Обновляет только геометрические производные поля
    const bool changing = state.changing; state.changing = true;	// Исключает повторные события служебных полей
    try {	// Оставляет незавершённый числовой ввод допустимым
        const auto p = readProject(window, state); wchar_t value[80]{};	// Проверяет возможность вычисления геометрических габаритов
        std::swprintf(value, std::size(value), L"%.6g", outerDiameter(p)); SetDlgItemTextW(window, 210, value);	// Показывает вычисленный наружный диаметр D0
        std::swprintf(value, std::size(value), L"%.6g", totalLength(p)); SetDlgItemTextW(window, 211, value);	// Показывает сумму осевых размеров L0
    } catch (...) { SetDlgItemTextW(window, 210, L""); SetDlgItemTextW(window, 211, L""); }	// Очищает габариты при незавершённом вводе
    state.changing = changing;	// Восстанавливает обработку пользовательских изменений
}	// Завершает обновление производной геометрии
void writeProject(HWND window, AppState& state, const Project& p) {	// Заполняет обе вкладки одним согласованным проектом
    state.changing = true; state.inputs = p;	// Подавляет ложное редактирование при загрузке
    const auto values = numbers(p); char value[80]{};	// Подготавливает числовые значения всех полей
    for (int i = 0; i < numberCount; ++i) {	// Заполняет каждое поле без потери точности
        const auto formatted = std::to_chars(value, value + std::size(value), values[i]);	// Выбирает краткую точную запись числа
        SetDlgItemTextW(window, firstNumber + i, wide(std::string(value, formatted.ptr)).c_str());	// Показывает значение соответствующего параметра
    }	// Завершает заполнение двух вкладок
    state.changing = false; updateDerived(window, state);	// Обновляет габариты без теплового расчёта
}	// Завершает загрузку проекта в интерфейс
void showTab(AppState& state, int selected) {																												// Переключает группы параметров
    state.tab = selected;																																	// Запоминает выбранную вкладку
    for (int tab = 0; tab < 2; ++tab) {																														// Обходит три страницы редактора
        for (const HWND item : state.groups[tab]) { ShowWindow(item, tab == selected ? SW_SHOW : SW_HIDE); }												// Показывает только элементы текущей страницы
    }																																						// Завершает изменение видимости элементов
}																																							// Завершает переключение вкладки
void createControls(HWND window, AppState& state) {	// Создаёт компактный редактор и панель действий
    control(window, L"BUTTON", L"Сохранить в .roll", BS_PUSHBUTTON | WS_TABSTOP, 16, 10, 155, 31, 3);	// Ставит сохранение проекта первым действием
    control(window, L"BUTTON", L"Сохранить в .csv", BS_PUSHBUTTON | WS_TABSTOP, 181, 10, 155, 31, 4);	// Ставит выгрузку истории вторым действием
    control(window, L"BUTTON", L"Открыть из файла", BS_PUSHBUTTON | WS_TABSTOP, 346, 10, 155, 31, 2);	// Добавляет загрузку входов без автоматического расчёта
    control(window, L"BUTTON", L"Закрепить базу", BS_PUSHBUTTON | WS_TABSTOP, 511, 10, 160, 31, 5);	// Сохраняет сравнение с рассчитанным проектом
    control(window, L"BUTTON", L"О программе", BS_PUSHBUTTON | WS_TABSTOP, 0, 10, 130, 31, 6);	// Создаёт правую кнопку встроенной справки
    control(window, L"BUTTON", L"Рассчитать", BS_PUSHBUTTON | WS_TABSTOP, 28, 0, 336, 34, 1);	// Закрепляет расчёт внизу панели параметров
    const HWND tabs = control(window, WC_TABCONTROLW, L"", WS_TABSTOP, 24, 92, 344, 29, tabsId);	// Оставляет только две вкладки параметров
    const wchar_t* titles[]{L"Геометрия", L"Материалы"};	// Называет две предметные группы входов
    for (int i = 0; i < 2; ++i) { TCITEMW item{}; item.mask = TCIF_TEXT; item.pszText = const_cast<wchar_t*>(titles[i]); TabCtrl_InsertItem(tabs, i, &item); }	// Заполняет заголовки двух вкладок
    groupControl(window, state, 0, L"STATIC", L"Диаметры, мм", 28, 135, 150, 20);	// Подписывает поперечные размеры пяти отделений
    groupControl(window, state, 0, L"STATIC", L"Длины, мм", 198, 135, 150, 20);	// Подписывает осевые размеры пяти отделений
    for (int i = 0; i < 5; ++i) {	// Нумерует пары полей согласно схеме
        const int y = 161 + i * 30; wchar_t name[12]{};	// Определяет строку соответствующего отделения
        std::swprintf(name, std::size(name), L"D%d", i + 1); groupControl(window, state, 0, L"STATIC", name, 28, y + 4, 35, 20);	// Подписывает соответствующий диаметр
        groupControl(window, state, 0, L"EDIT", L"", 68, y, 100, 24, firstNumber + i * 2);	// Создаёт поле диаметра своего отделения
        std::swprintf(name, std::size(name), L"L%d", i + 1); groupControl(window, state, 0, L"STATIC", name, 198, y + 4, 35, 20);	// Подписывает соответствующую осевую длину
        groupControl(window, state, 0, L"EDIT", L"", 238, y, 110, 24, firstNumber + i * 2 + 1);	// Создаёт поле длины своего отделения
    }	// Завершает пять геометрических пар
    const wchar_t* walls[]{L"SK · Кокиль, мм", L"SC · Слой краски, мм", L"SOP · Опока бочки, мм", L"SFS · Стенка формы, мм", L"SOS · Опока шеек, мм", L"DS · Диаметр трубы, мм"};	// Выбирает параметры монолитной конструкции
    for (int i = 0; i < 6; ++i) {	// Создаёт шесть размеров окружающей оснастки
        groupControl(window, state, 0, L"STATIC", walls[i], 28, 330 + i * 28, 225, 22);	// Объясняет буквенное обозначение и единицы
        groupControl(window, state, 0, L"EDIT", L"", 264, 326 + i * 28, 85, 24, firstNumber + 10 + i);	// Передаёт толщину в расчёт либо геометрию
    }	// Завершает размеры стенок и трубы
    groupControl(window, state, 0, L"STATIC", L"D0 · Наружный диаметр, мм", 28, 507, 230, 22);	// Называет производный габарит формы
    groupControl(window, state, 0, L"STATIC", L"L0 · Общая длина, мм", 28, 537, 230, 22);	// Называет суммарный осевой размер
    for (int i = 0; i < 2; ++i) {	// Создаёт защищённые от редактирования габариты
        const HWND field = control(window, L"EDIT", L"", ES_READONLY, 264, 503 + i * 30, 85, 24, 210 + i);	// Показывает вычисляемое геометрическое значение
        state.groups[0].push_back(field);	// Связывает поле с вкладкой геометрии
    }	// Завершает производные геометрические поля
    const wchar_t* settings[]{L"Общее время, мин", L"Шаг по времени, с", L"Шаг по диаметру, мм"};	// Оставляет действующие настройки численной модели
    for (int i = 0; i < 3; ++i) {	// Создаёт понятные параметры дискретизации
        groupControl(window, state, 0, L"STATIC", settings[i], 28, 587 + i * 30, 225, 22);	// Подписывает горизонт и оба шага
        groupControl(window, state, 0, L"EDIT", L"", 264, 583 + i * 30, 85, 24, firstNumber + 16 + i);	// Создаёт редактируемое значение численной схемы
    }	// Завершает геометрию и настройки расчёта
    const wchar_t* headings[]{L"ρ, кг/м³", L"c, Дж/(кг·К)", L"λ, Вт/(м·К)"};	// Правильно указывает единицы свойств материалов
    for (int i = 0; i < 3; ++i) { groupControl(window, state, 1, L"STATIC", headings[i], 104 + i * 84, 144, 84, 24); }	// Подписывает плотность теплоёмкость и теплопроводность
    const wchar_t* materials[]{L"Металл", L"Краска", L"Кокиль\nи опока", L"Форма"};	// Исключает рабочий слой и обечайку
    for (int i = 0; i < 4; ++i) {	// Создаёт свойства действующих материалов
        groupControl(window, state, 1, L"STATIC", materials[i], 28, 184 + i * 40, 74, 36);	// Обозначает материал каждой строки
        for (int c = 0; c < 3; ++c) { groupControl(window, state, 1, L"EDIT", L"", 104 + c * 84, 180 + i * 40, 78, 24, firstNumber + 19 + 3 * i + c); }	// Создаёт поля трёх независимых свойств
    }	// Завершает таблицу четырёх материалов
    const wchar_t* thermal[]{L"Теплота кристаллизации, Дж/кг", L"Ликвидус, °C", L"Солидус, °C", L"Температура металла, °C", L"Температура формы, °C", L"Температура воздуха, °C", L"Теплоотдача, Вт/(м²·К)"};	// Объединяет фазовые данные и температурные условия
    for (int i = 0; i < 7; ++i) {	// Создаёт оставшиеся тепловые поля
        groupControl(window, state, 1, L"STATIC", thermal[i], 28, 370 + i * 40, 232, 28);	// Подписывает физическую величину и единицы
        groupControl(window, state, 1, L"EDIT", L"", 264, 366 + i * 40, 85, 24, firstNumber + 31 + i);	// Передаёт параметр тепловой модели
    }	// Завершает материалы и граничные условия
    control(window, TRACKBAR_CLASSW, L"", TBS_NOTICKS | WS_TABSTOP, 0, 0, 100, 25, sliderId);	// Создаёт общую круглую временную ручку
    SendDlgItemMessageW(window, sliderId, TBM_SETRANGE, TRUE, MAKELPARAM(0, timelineEnd));	// Сохраняет разрешение общей шкалы
    control(window, L"STATIC", L"00:00:00", 0, 0, 0, 85, 22, clockId);	// Оставляет нулевое время до расчёта
    control(window, L"STATIC", L"Выбранное отделение", 0, 0, 0, 160, 22, 208);	// Объясняет назначение списка графиков
    const HWND picker = control(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, 0, 250, 180, chartPicker);	// Предлагает только одно отделение за раз
    for (const auto* name : sectionNames) { SendMessageW(picker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name)); }	// Связывает пункты с номерами на форме
    SendMessageW(picker, CB_SETCURSEL, 0, 0);	// Выбирает прибыль по умолчанию
    control(window, L"BUTTON", L"Выгрузить", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 105, 28, firstExport);	// Возвращает стандартную текстовую кнопку выгрузки
    showTab(state, 0); writeProject(window, state, state.inputs);	// Заполняет исходные поля без запуска вычислений
}	// Завершает создание упрощённого рабочего экрана
std::size_t frameIndex(const AppState& state) {																												// Сопоставляет ползунок кадру охлаждения
    if (!state.result || state.position <= fillingEnd) { return 0; }																						// Сохраняет начальную температуру во время заливки
    const auto count = state.result->cooling.section(0).samples.size();																						// Читает количество согласованных кадров
    return static_cast<std::size_t>((state.position - fillingEnd) * static_cast<double>(count - 1) / (timelineEnd - fillingEnd));							// Выбирает ближайший предшествующий кадр
}																																							// Завершает выбор кадра
double fillProgress(const AppState& state) {																												// Определяет степень заполнения формы
    return std::min(1.0, state.position / static_cast<double>(fillingEnd));																					// Отделяет заливку от охлаждения
}																																							// Завершает определение стадии
void arrange(HWND window, AppState& state) {	// Размещает постоянно видимые элементы управления
    const auto areas = layout(window);	// Получает текущие размеры трёх панелей
    MoveWindow(GetDlgItem(window, 1), 28, areas.client.bottom - 58, 336, 34, TRUE);	// Ставит расчёт внизу панели параметров
    MoveWindow(GetDlgItem(window, 6), areas.client.right - 146, 10, 130, 31, TRUE);	// Выравнивает справку по правому краю окна
    MoveWindow(GetDlgItem(window, sliderId), areas.mold.left + 88, areas.client.bottom - 78, areas.mold.right - areas.mold.left - 92, 26, TRUE);	// Сохраняет шкалу под белой областью схемы
    MoveWindow(GetDlgItem(window, clockId), areas.mold.left + 4, areas.client.bottom - 75, 85, 22, TRUE);	// Ставит время слева от круглой ручки
    MoveWindow(GetDlgItem(window, 208), areas.results.left + 12, areas.results.top + 224, 158, 22, TRUE);	// Подписывает выбор конкретного отделения
    MoveWindow(GetDlgItem(window, chartPicker), areas.results.left + 175, areas.results.top + 218, areas.results.right - areas.results.left - 190, 180, TRUE);	// Размещает список под сводной таблицей
    const auto card = chartAreas(areas.results, state.selection)[state.selection];	// Находит карточку единственного выбранного графика
    MoveWindow(GetDlgItem(window, firstExport), card.right - 116, card.top + 7, 105, 28, TRUE);	// Ставит текстовую выгрузку в угол графика
    EnableWindow(GetDlgItem(window, firstExport), state.result && !state.running && !state.edited);	// Запрещает выгрузку отсутствующего или устаревшего результата
    EnableWindow(GetDlgItem(window, sliderId), state.result || state.running);	// Оставляет шкалу неподвижной до команды расчёта
    EnableWindow(GetDlgItem(window, 4), state.result && !state.running && !state.edited);	// Разрешает CSV только для актуальной истории
    EnableWindow(GetDlgItem(window, 1), !state.running);	// Не допускает повторного запуска текущего задания
    SetDlgItemTextW(window, 1, state.running ? L"Расчёт..." : L"Рассчитать");	// Показывает выполнение без отдельной строки обновлений
    SetDlgItemTextW(window, 5, state.baseline || state.baselineInputs ? L"Убрать базу" : L"Закрепить базу");	// Сохраняет понятное действие сравнения
}	// Завершает размещение элементов экрана
void updateTime(HWND window, AppState& state) {																												// Обновляет выбранный момент без пересчёта
    double seconds = fillProgress(state) * 60.0;																						// Отсчитывает время от начала заливки
    if (state.result && state.position > fillingEnd && !state.running) { seconds += state.result->cooling.section(0).samples[frameIndex(state)].seconds; }	// Добавляет время после заполнения
    const auto whole = static_cast<unsigned long long>(std::llround(seconds));																				// Округляет время до целых секунд
    wchar_t caption[40]{};																																	// Резервирует строку часов
    std::swprintf(caption, std::size(caption), L"%02llu:%02llu:%02llu", whole / 3600, whole / 60 % 60, whole % 60);											// Форматирует часы минуты секунды
    SetDlgItemTextW(window, clockId, caption);																												// Показывает время слева от шкалы
    InvalidateRect(GetDlgItem(window, sliderId), nullptr, TRUE);	// Очищает прежнее положение круглого маркера
    SendDlgItemMessageW(window, sliderId, TBM_SETPOS, TRUE, state.position);																				// Синхронизирует ручку шкалы
    const auto areas = layout(window);																														// Получает динамические панели
    InvalidateRect(window, &areas.mold, FALSE);																												// Перерисовывает состояние металла
    InvalidateRect(window, &areas.results, FALSE);																											// Обновляет таблицу и указатели времени
}																																							// Завершает смену момента
void calculate(HWND window, AppState& state, const Project& project) {																						// Запускает проверенный проект в фоновом потоке
    validateProject(project);																																// Проверяет входы до отмены предыдущей работы
    state.worker.request_stop();																															// Просит прежнее задание остановиться
    if (state.worker.joinable()) { state.worker.join(); }																									// Дожидается безопасного завершения прежнего задания
    const auto generation = ++state.generation;																												// Назначает номер нового задания
    state.inputs = project; state.running = true; state.edited = false;																						// Фиксирует исходные данные текущего задания
    state.position = 0; state.automatic = true; state.rebuildMold = true;																					// Начинает новую анимацию заполнения
    state.curves = {};																																		// Сбрасывает линии прежнего результата
    const auto restore = state.baseline ? std::optional<Project>{} : state.baselineInputs;																	// Определяет необходимость восстановления базы
    arrange(window, state); updateTime(window, state);																										// Обновляет доступность и начальный кадр
    SetTimer(window, 1, 50, nullptr);																														// Запускает плавное воспроизведение заливки
    state.worker = std::jthread([window, project, restore, generation](std::stop_token stop) {																// Передаёт потоку независимую копию входов
        auto completed = std::make_unique<Completed>();																										// Создаёт сообщение для оконного потока
        completed->generation = generation;																													// Отмечает принадлежность результата заданию
        try {																																				// Защищает оконный поток от расчётных исключений
            completed->result = std::make_shared<Run>();																									// Выделяет независимый текущий результат
            completed->result->project = project;																											// Сохраняет входы вместе с историей
            completed->result->cooling.calculate(project, stop);																							// Вычисляет пять отделений вне интерфейса
            if (restore && !stop.stop_requested()) {																										// Восстанавливает закреплённую базу при запуске
                completed->baseline = std::make_shared<Run>();																								// Выделяет независимый базовый результат
                completed->baseline->project = *restore;																									// Сохраняет исходные данные базы
                completed->baseline->cooling.calculate(*restore, stop);																						// Восстанавливает историю той же моделью
            }																																				// Завершает восстановление сравнения
        } catch (const std::exception& error) { completed->error = wide(error.what()); }																	// Передаёт причину неудачного расчёта
        if (!stop.stop_requested() && PostMessageW(window, calculationDone, 0, reinterpret_cast<LPARAM>(completed.get()))) { completed.release(); }			// Передаёт владение только доставляемому сообщению
    });																																						// Завершает создание фонового задания
}																																							// Завершает запуск вычислений
std::filesystem::path chooseFile(HWND window, const AppState& state, const wchar_t* filter, const wchar_t* extension, bool save) {							// Показывает системный файловый диалог
    wchar_t filename[32768]{};																																// Поддерживает длинные пути и русские имена
    OPENFILENAMEW dialog{};																																	// Обнуляет системную структуру диалога
    dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = window;																							// Привязывает диалог к главному окну
    dialog.lpstrFilter = filter; dialog.lpstrDefExt = extension;																							// Задаёт тип сохраняемого документа
    dialog.lpstrFile = filename; dialog.nMaxFile = std::size(filename);																						// Предоставляет буфер полного пути
    dialog.lpstrInitialDir = state.directory.c_str();																										// Начинает просмотр с папки программы
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);									// Проверяет путь и подтверждает перезапись
    const bool accepted = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);																		// Выполняет требуемый файловый диалог
    if (!accepted && CommDlgExtendedError()) { throw std::runtime_error("Не удалось открыть файловый диалог"); }											// Отличает ошибку от отмены
    return accepted ? std::filesystem::path(filename) : std::filesystem::path{};																			// Возвращает выбранный путь или отмену
}																																							// Завершает выбор файла
bool current(HWND window, AppState& state) {																												// Запрещает экспорт устаревшего расчёта
    if (state.running || !state.result || state.edited || !(readProject(window, state) == state.result->project)) {											// Проверяет соответствие истории редактору
        throw std::runtime_error("Сначала нажмите «Рассчитать» и дождитесь завершения");																				// Объясняет необходимость актуального расчёта
        return false;																																		// Отклоняет действие над устаревшими данными
    }																																						// Завершает проверку актуальности
    return true;																																			// Разрешает действие над текущим результатом
}																																							// Завершает проверку результата
bool discard(HWND window, const AppState& state) {																											// Защищает несохранённые изменения проекта
    return !state.unsaved || MessageBoxW(window, L"Есть несохранённые изменения. Продолжить без их сохранения?", L"RollCast", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;	// Запрашивает отказ от введённых изменений
}																																							// Завершает проверку замены проекта
void exportChart(HWND window, AppState& state, int section) {																								// Сохраняет выбранный график отдельным изображением
    if (!current(window, state)) { return; }																												// Требует завершённый актуальный результат
    const auto path = chooseFile(window, state, L"Изображение PNG\0*.png\0\0", L"png", true);																// Предлагает путь изображения
    if (path.empty()) { return; }																															// Учитывает отмену диалога
    const HDC target = GetDC(window);																														// Получает образец экранного контекста
    Surface image;																																			// Управляет буфером экспортируемого изображения
    try { image.ensure(target, 1100, 760); } catch (...) { ReleaseDC(window, target); throw; }																// Освобождает контекст при ошибке выделения
    ReleaseDC(window, target);																																// Возвращает экранный контекст системе
    const RECT area{0, 0, 1100, 760};																														// Задаёт независимое разрешение изображения
    FillRect(image.dc, &area, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));																			// Подготавливает белый фон
    SelectObject(image.dc, GetStockObject(DEFAULT_GUI_FONT));																								// Сохраняет читаемый системный шрифт
    CurveCache cache;																																		// Создаёт отдельный масштаб экспортируемых линий
    drawSingleChart(image.dc, area, state.result->project, state.result.get(), state.baseline.get(), section, frameIndex(state), cache, fillProgress(state));							// Рисует полный график с подписями
    const CLSID png{0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};															// Выбирает встроенный кодировщик PNG
    Gdiplus::Bitmap bitmap(image.bitmap, nullptr);																											// Оборачивает готовый растр для кодировщика
    if (bitmap.Save(path.c_str(), &png, nullptr) != Gdiplus::Ok) { throw std::runtime_error("Не удалось сохранить PNG"); }									// Проверяет успешность записи изображения
}																																							// Завершает экспорт отдельного графика
void action(HWND window, AppState& state, int id) {	// Выполняет команды верхней и левой панелей
    if (id == 1) { calculate(window, state, readProject(window, state)); }	// Запускает расчёт только по явной команде
    if (id == 2 && discard(window, state)) {	// Загружает другой проект с защитой изменений
        const auto path = chooseFile(window, state, L"Проект RollCast\0*.roll\0\0", L"roll", false);	// Предлагает системный диалог открытия
        if (!path.empty()) {	// Продолжает только после выбора файла
            const auto p = loadProject(path);	// Проверяет файл до изменения редактора
            state.worker.request_stop(); if (state.worker.joinable()) { state.worker.join(); }	// Отменяет прежнее вычисление перед заменой входов
            ++state.generation; state.running = false; state.automatic = false; KillTimer(window, 1);	// Исключает публикацию отменённого задания
            writeProject(window, state, p); state.result.reset(); state.curves = {};	// Загружает поля и очищает прежние результаты
            state.filename = path; state.unsaved = false; state.edited = false;	// Запоминает выбранный документ
            state.position = 0; state.rebuildMold = true;	// Возвращает схему к пустой форме
            arrange(window, state); updateTime(window, state);	// Оставляет проект ожидающим кнопки расчёта
        }	// Завершает загрузку выбранного файла
    }	// Завершает действие открытия
    if (id == 3) {	// Сохраняет текущие входы без автоматического расчёта
        const auto p = readProject(window, state);	// Проверяет обе вкладки редактора
        const auto path = chooseFile(window, state, L"Проект RollCast\0*.roll\0\0", L"roll", true);	// Получает название от пользователя при сохранении
        if (!path.empty()) {	// Учитывает отмену системного диалога
            saveProject(path, p, !state.running && state.result && state.result->project == p ? state.result.get() : nullptr);	// Добавляет сводку только соответствующего результата
            state.filename = path; state.unsaved = false;	// Отмечает успешно сохранённые входные данные
        }	// Завершает запись выбранного документа
    }	// Завершает действие сохранения проекта
    if (id == 4 && current(window, state)) {	// Сохраняет полную историю пяти отделений
        const auto path = chooseFile(window, state, L"Таблица CSV\0*.csv\0\0", L"csv", true);	// Предлагает пользовательское название таблицы
        if (!path.empty()) { exportHistory(path, *state.result); }	// Записывает все рассчитанные кадры
    }	// Завершает выгрузку CSV
    if (id == 5) {	// Закрепляет либо снимает независимое сравнение
        if (state.running) { throw std::runtime_error("Дождитесь завершения расчёта"); }	// Защищает восстанавливаемую базу от изменения
        const auto path = state.directory / L".rollcast-base-v2.roll";	// Отделяет базу новой ветки от прежней
        if (state.baseline || state.baselineInputs) {	// Снимает существующее закрепление
            std::filesystem::remove(path); state.baseline.reset(); state.baselineInputs.reset();	// Удаляет только собственный документ базы
        } else if (current(window, state)) {	// Закрепляет текущую согласованную историю
            saveProject(path, state.result->project, state.result.get());	// Сохраняет входы для следующего запуска
            state.baseline = state.result; state.baselineInputs = state.result->project;	// Удерживает неизменяемый базовый результат
        }	// Завершает изменение состояния сравнения
        state.curves = {}; arrange(window, state);	// Обновляет подпись кнопки и кэш кривых
        const auto area = layout(window).results; InvalidateRect(window, &area, FALSE);	// Обновляет только панель результатов
    }	// Завершает закрепление базы
    if (id == 6) { showAbout(window); }	// Открывает теорию и концепцию поверх рабочего окна
    if (id == firstExport) { exportChart(window, state, state.selection); }	// Выгружает единственное выбранное отделение
}	// Завершает обработку явных пользовательских действий
void paint(HWND window, AppState& state) {																													// Рисует только запрошенную часть рабочего экрана
    PAINTSTRUCT painting{}; const HDC target = BeginPaint(window, &painting);																				// Получает область требуемого обновления
    const auto areas = layout(window);																														// Получает расположение всех панелей
    state.buffer.ensure(target, areas.client.right, areas.client.bottom);																					// Переиспользует растровый буфер до изменения окна
    const HDC dc = state.buffer.dc; const int saved = SaveDC(dc);																							// Сохраняет настройки повторного контекста
    SelectClipRgn(dc, nullptr);																																// Восстанавливает полную область внутреннего буфера
    FillRect(dc, &painting.rcPaint, GetSysColorBrush(COLOR_BTNFACE));																						// Очищает только изменившуюся часть
    SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));																										// Выбирает системный шрифт интерфейса
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));																			// Настраивает читаемые подписи
    const wchar_t* headings[]{L"Параметры", L"Форма для заливки валка", L"Кристаллизация металла"};															// Называет три постоянно видимые панели
    const RECT panels[]{areas.parameters, areas.mold, areas.results};																						// Сопоставляет заголовки панелям
    RECT intersection{};																																	// Принимает пересечение с обновляемой областью
    for (int i = 0; i < 3; ++i) {																															// Обновляет фон и рамки затронутых панелей
        TextOutW(dc, panels[i].left, 56, headings[i], static_cast<int>(std::wcslen(headings[i])));															// Рисует заголовок над линией рамки
        if (IntersectRect(&intersection, &painting.rcPaint, &panels[i])) {																					// Проверяет необходимость обновления панели
            FillRect(dc, &panels[i], GetSysColorBrush(i == 1 ? COLOR_WINDOW : COLOR_BTNFACE));																// Сохраняет белый фон только под схемой
            FrameRect(dc, &panels[i], GetSysColorBrush(COLOR_3DSHADOW));																					// Отделяет панель тонкой рамкой
        }																																					// Завершает проверку изменившейся области
    }																																						// Завершает подготовку панелей
    if (IntersectRect(&intersection, &painting.rcPaint, &areas.mold)) {																						// Обновляет форму отдельно от графиков
        MoldFrame frame; frame.filling = fillProgress(state);																								// Задаёт текущую стадию заполнения
        frame.filled = filledSections(state.inputs, frame.filling); frame.phase = state.inputs.phase;														// Выбирает уровни металла и фазовые границы
        if (state.result && !state.running) {																												// Подключает готовые температуры после расчёта
            for (std::size_t i = 0; i < sectionCount; ++i) { frame.samples[i] = &state.result->cooling.section(i).samples[frameIndex(state)]; }						// Передаёт профили выбранного кадра
        }																																					// Завершает подготовку температуры схемы
        drawMold(dc, areas.mold, state.inputs, state.hits, state.hovered, state.cursor, frame, state.rebuildMold);								// Рисует форму без пересоздания постоянных областей
        state.rebuildMold = false;																															// Сохраняет геометрические области для следующего кадра
        drawMoldLabel(dc, areas.mold, state.hovered, state.cursor);																							// Показывает название выбранной детали
    }																																						// Завершает отдельное обновление схемы
    if (IntersectRect(&intersection, &painting.rcPaint, &areas.results)) {																					// Пропускает графики при обычном наведении
        drawMoldDashboard(dc, areas.results, state.inputs, !state.running ? state.result.get() : nullptr, !state.running && state.result ? state.baseline.get() : nullptr, frameIndex(state), fillProgress(state), state.selection, state.curves);	// Оставляет численные результаты пустыми до явного расчёта
    }																																						// Завершает обновление результатов
    const wchar_t* legend[]{L"Твёрдая", L"Двухфазная", L"Жидкая"};																							// Объясняет цвета металла под формой
    const COLORREF colors[]{RGB(45, 112, 181), RGB(242, 177, 46), RGB(228, 76, 57)};																		// Согласует цвета с фазами разреза
    for (int i = 0; i < 3; ++i) {																															// Рисует три обозначения вне белой области
        const int x = areas.mold.left + 3 + i * (areas.mold.right - areas.mold.left) / 3;																	// Распределяет обозначения под формой
        const int y = areas.client.bottom - 33;																												// Ставит легенду ниже временной шкалы
        const RECT square{x, y + 3, x + 11, y + 14};																										// Задаёт цветной образец
        const HBRUSH brush = CreateSolidBrush(colors[i]); FillRect(dc, &square, brush); DeleteObject(brush);												// Рисует и освобождает образец цвета
        TextOutW(dc, x + 15, y, legend[i], static_cast<int>(std::wcslen(legend[i])));																		// Подписывает состояние металла
    }																																						// Завершает легенду трёх зон
    BitBlt(target, painting.rcPaint.left, painting.rcPaint.top, painting.rcPaint.right - painting.rcPaint.left, painting.rcPaint.bottom - painting.rcPaint.top, dc, painting.rcPaint.left, painting.rcPaint.top, SRCCOPY);	// Переносит только обновлённую область
    RestoreDC(dc, saved); EndPaint(window, &painting);																										// Завершает кадр и восстанавливает контекст
}																																							// Завершает частичную отрисовку
LRESULT sliderDraw(NMCUSTOMDRAW* draw) {																													// Рисует круглую ручку на серой линии
    if (draw->dwDrawStage == CDDS_PREPAINT) { return CDRF_NOTIFYITEMDRAW; }																					// Запрашивает рисование частей ползунка
    if (draw->dwDrawStage != CDDS_ITEMPREPAINT) { return CDRF_DODEFAULT; }																					// Сохраняет стандартную обработку прочих стадий
    RECT area = draw->rc;																																	// Получает область отдельной части
    if (draw->dwItemSpec == TBCD_CHANNEL) {																													// Заменяет канал простой серой линией
        area.top = (area.top + area.bottom) / 2 - 2; area.bottom = area.top + 4;																			// Задаёт толщину линии шкалы
        FillRect(draw->hdc, &area, GetSysColorBrush(COLOR_3DSHADOW));																						// Рисует нейтральную серую линию
        return CDRF_SKIPDEFAULT;																															// Подавляет объёмный стандартный канал
    }																																						// Завершает оформление линии
    if (draw->dwItemSpec == TBCD_THUMB) {																													// Заменяет ручку круглым маркером
        const int x = (area.left + area.right) / 2; const int y = (area.top + area.bottom) / 2;																// Находит центр ручки
        const HBRUSH brush = CreateSolidBrush(RGB(45, 112, 181));																							// Выбирает цвет активного маркера
        const auto oldBrush = SelectObject(draw->hdc, brush); const auto oldPen = SelectObject(draw->hdc, GetStockObject(NULL_PEN));						// Сохраняет исходные объекты
        const int radius = std::max<int>(2, std::min(area.right - area.left, area.bottom - area.top) / 2 - 1);	// Ограничивает маркер областью системной ручки
        Ellipse(draw->hdc, x - radius, y - radius, x + radius + 1, y + radius + 1);																										// Рисует круглую ручку шкалы
        SelectObject(draw->hdc, oldBrush); SelectObject(draw->hdc, oldPen); DeleteObject(brush);															// Восстанавливает объекты и освобождает кисть
        return CDRF_SKIPDEFAULT;																															// Подавляет стандартную прямоугольную ручку
    }																																						// Завершает оформление ручки
    return CDRF_DODEFAULT;																																	// Оставляет остальную обработку системе
}																																							// Завершает специальное рисование ползунка
LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {																	// Обрабатывает действия пользователя главного окна
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));																	// Получает принадлежащее окну состояние
    if (message == WM_NCCREATE) {																															// Связывает состояние до создания дочерних элементов
        state = static_cast<AppState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);															// Извлекает переданный экземпляр приложения
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));																		// Запоминает состояние на время жизни окна
    }																																						// Завершает начальную привязку состояния
    if (!state) { return DefWindowProcW(window, message, wParam, lParam); }																					// Передаёт ранние сообщения стандартному обработчику
    try {																																					// Сохраняет интерфейс работоспособным после ошибок ввода
        switch (message) {																																	// Выбирает обработчик оконного сообщения
        case WM_CREATE:																																		// Создаёт рабочий экран и запускает первый расчёт
            createControls(window, *state); arrange(window, *state);																						// Создаёт и размещает элементы управления
            return 0;																								// Запускает пример без блокирования окна
        case WM_GETMINMAXINFO:																																// Сохраняет видимость всех трёх панелей
            reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {1240, 800}; return 0;																	// Ограничивает минимальный размер рабочего окна
        case WM_SIZE:																																		// Перестраивает расположение после изменения размеров
            state->rebuildMold = true; state->curves = {};																									// Сбрасывает зависящие от размеров данные
            arrange(window, *state); InvalidateRect(window, nullptr, FALSE); return 0;																		// Обновляет расположение и полный кадр
        case WM_ERASEBKGND: return 1;																														// Подавляет мерцание при двойной буферизации
        case WM_PAINT: paint(window, *state); return 0;																										// Обновляет только повреждённые области окна
        case WM_COMMAND: {																																	// Обрабатывает редактор список и панель действий
            const int id = LOWORD(wParam); const int code = HIWORD(wParam);																					// Разбирает источник пользовательского действия
            if (!state->changing && code == EN_CHANGE && id >= firstNumber && id < firstNumber + numberCount) {	// Отмечает только пользовательское редактирование входов
                state->unsaved = true; state->edited = true;	// Различает редактор и рассчитанную историю
                updateDerived(window, *state); arrange(window, *state);	// Обновляет габариты без тепловых вычислений
            } else if (id == chartPicker && code == CBN_SELCHANGE) {																						// Меняет набор показываемых графиков
                state->selection = static_cast<int>(SendDlgItemMessageW(window, chartPicker, CB_GETCURSEL, 0, 0));											// Запоминает выбранное отделение
                arrange(window, *state); const auto area = layout(window).results; InvalidateRect(window, &area, FALSE);									// Перерисовывает только панель результатов
            } else if (code == BN_CLICKED && ((id >= 1 && id <= 6) || id == firstExport)) { action(window, *state, id); }			// Выполняет действие выбранной кнопки
            return 0;																																		// Завершает обработку команды
        }																																					// Завершает команды интерфейса
        case WM_NOTIFY: {																																	// Обрабатывает вкладки и оформление шкалы
            const auto* notice = reinterpret_cast<NMHDR*>(lParam);																							// Определяет источник уведомления
            if (notice->idFrom == tabsId && notice->code == TCN_SELCHANGE) { showTab(*state, TabCtrl_GetCurSel(notice->hwndFrom)); }						// Показывает параметры выбранной страницы
            if (notice->idFrom == sliderId && notice->code == NM_CUSTOMDRAW) { return sliderDraw(reinterpret_cast<NMCUSTOMDRAW*>(lParam)); }				// Рисует круглый маркер времени
            return 0;																																		// Завершает прочие уведомления
        }																																					// Завершает обработку вкладок
        case WM_HSCROLL:																																	// Показывает ранее рассчитанные кадры без вычислений
            if (reinterpret_cast<HWND>(lParam) == GetDlgItem(window, sliderId)) {																			// Проверяет источник смены положения
                state->automatic = false; KillTimer(window, 1);																								// Передаёт управление воспроизведением пользователю
                state->position = static_cast<int>(SendDlgItemMessageW(window, sliderId, TBM_GETPOS, 0, 0));												// Получает положение круглой ручки
                if (state->running) { state->position = std::min(fillingEnd, state->position); }															// Ограничивает кадры до завершения расчёта
                updateTime(window, *state);																													// Обновляет схему и показатели выбранного момента
            }																																				// Завершает изменение положения
            return 0;																																		// Завершает обработку шкалы
        case WM_TIMER:																																		// Воспроизводит заполнение формы после запуска расчёта
            if (wParam == 1 && state->automatic) {																											// Обрабатывает только таймер анимации
                state->position = std::min(fillingEnd, state->position + 20);																				// Продвигает металл по пути заполнения
                updateTime(window, *state);																													// Показывает следующий кадр заливки
                if (state->position == fillingEnd) { state->automatic = false; KillTimer(window, 1); }														// Останавливается у начала охлаждения
            }																																				// Завершает обработку воспроизведения
            return 0;																																		// Завершает сообщение таймера
        case calculationDone: {																																// Принимает полностью готовую историю
            std::unique_ptr<Completed> completed(reinterpret_cast<Completed*>(lParam));																		// Принимает владение сообщением потока
            if (completed->generation != state->generation) { return 0; }																					// Отбрасывает отменённый результат
            state->running = false;																															// Возвращает доступность экспортных действий
            if (completed->error.empty()) {																													// Публикует только согласованный полный результат
                state->result = std::move(completed->result);																								// Заменяет текущую историю одним действием
                if (completed->baseline) { state->baseline = std::move(completed->baseline); }																// Восстанавливает закреплённую историю
            } else { state->result.reset(); MessageBoxW(window, completed->error.c_str(), L"Ошибка расчёта", MB_OK | MB_ICONERROR); }	// Объясняет неудачу без скрытых результатов
            state->curves = {}; arrange(window, *state); updateTime(window, *state);																		// Обновляет линии и выбранный кадр
            return 0;																																		// Завершает приём результата
        }																																					// Завершает обработку завершения вычислений
        case WM_MOUSEMOVE: {																																// Обновляет подсказку без перерисовки графиков
            const POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};																					// Получает положение указателя
            const auto areas = layout(window);																												// Определяет область схемы
            const MoldPart part = PtInRect(&areas.mold, cursor) ? state->hits.partAt(cursor) : MoldPart::none;												// Находит деталь под курсором
            if (part != state->hovered || (part != MoldPart::none && (std::abs(cursor.x - state->cursor.x) > 3 || std::abs(cursor.y - state->cursor.y) > 3))) {	// Ограничивает лишние обновления подписи
                state->hovered = part; state->cursor = cursor;																								// Сохраняет новое выделение
                InvalidateRect(window, &areas.mold, FALSE);																									// Обновляет исключительно схему формы
            }																																				// Завершает проверку движения
            if (!state->tracking) { TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, window, 0}; TrackMouseEvent(&tracking); state->tracking = true; }	// Запрашивает уведомление об уходе мыши
            return 0;																																		// Завершает обновление подсказки
        }																													// Завершает обработку наведения
        case WM_MOUSELEAVE: {																																// Снимает выделение после ухода указателя
            state->hovered = MoldPart::none; state->tracking = false;																						// Очищает состояние наведения
            const auto area = layout(window).mold; InvalidateRect(window, &area, FALSE); return 0;															// Обновляет только форму
        }																																					// Завершает снятие подсказки
        case WM_DESTROY: {																																	// Завершает расчёт перед закрытием приложения
            state->worker.request_stop(); if (state->worker.joinable()) { state->worker.join(); }															// Дожидается остановки фонового потока
            MSG pending{}; while (PeekMessageW(&pending, window, calculationDone, calculationDone, PM_REMOVE)) { delete reinterpret_cast<Completed*>(pending.lParam); }	// Освобождает недоставленные результаты
            KillTimer(window, 1); PostQuitMessage(0); return 0;																								// Завершает таймер и оконный цикл
        }																																					// Завершает закрытие приложения
        default: break;																																		// Передаёт прочие сообщения стандартной процедуре
        }																																					// Завершает выбор оконного сообщения
    } catch (const std::exception& error) { MessageBoxW(window, wide(error.what()).c_str(), L"RollCast", MB_OK | MB_ICONWARNING); }																	// Показывает ошибку без аварийного завершения
    return DefWindowProcW(window, message, wParam, lParam);																									// Выполняет стандартную обработку Windows
}																															// Завершает оконную процедуру
}																																							// Завершает пространство внутренней реализации
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {																							// Запускает оконное приложение Windows
    Gdiplus::GdiplusStartupInput graphics; ULONG_PTR token = 0;																								// Подготавливает системный кодировщик изображений
    if (Gdiplus::GdiplusStartup(&token, &graphics, nullptr) != Gdiplus::Ok) { return 1; }																	// Проверяет доступность GDI+
    INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_WIN95_CLASSES};										// Выбирает системные классы элементов управления
    InitCommonControlsEx(&controls);																														// Регистрирует вкладки список шкалу и подсказки
    WNDCLASSW windowClass{}; windowClass.lpfnWndProc = windowProcedure;																						// Назначает процедуру главного окна
    windowClass.hInstance = instance; windowClass.lpszClassName = L"RollCastMainWindow";																	// Задаёт класс окна приложения
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);																									// Использует стандартный указатель
    windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);																							// Задаёт серый фон рабочего экрана
    if (!RegisterClassW(&windowClass)) { Gdiplus::GdiplusShutdown(token); return 1; }																		// Проверяет регистрацию главного окна
    AppState state; wchar_t executable[32768]{};																											// Создаёт независимое состояние текущего запуска
    GetModuleFileNameW(nullptr, executable, std::size(executable));																							// Получает путь исполняемого файла
    state.directory = std::filesystem::path(executable).parent_path();																						// Выбирает папку хранения локальной базы
    try {																																					// Восстанавливает только корректные исходные данные базы
        const auto base = state.directory / L".rollcast-base-v2.roll";																							// Определяет путь закреплённого проекта
        if (std::filesystem::exists(base)) { state.baselineInputs = loadProject(base); }																	// Загружает базу для фонового восстановления
    } catch (...) { state.baselineInputs.reset(); }																											// Позволяет запуск при повреждённой базе
    const HWND window = CreateWindowExW(0, windowClass.lpszClassName, L"RollCast", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900, nullptr, nullptr, instance, &state);	// Создаёт единый рабочий экран
    if (!window) { Gdiplus::GdiplusShutdown(token); return 1; }																								// Проверяет успешность создания окна
    ShowWindow(window, SW_SHOWMAXIMIZED); UpdateWindow(window);																								// Открывает окно на весь доступный экран
    MSG message{};																																			// Принимает сообщения интерфейса
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {																										// Обрабатывает действия до закрытия окна
        if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN && (message.hwnd == window || GetClassLongPtrW(message.hwnd, GCW_ATOM) == 0x0081)) {	// Применяет параметры клавишей Enter в редакторе
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(1, BN_CLICKED), 0); continue;																		// Запускает расчёт введённых параметров
        }																																					// Завершает обработку быстрого подтверждения
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }												// Сохраняет клавиатурную навигацию по элементам
    }																																						// Завершает цикл сообщений
    Gdiplus::GdiplusShutdown(token); return static_cast<int>(message.wParam);																				// Освобождает кодировщик после закрытия окна
}																																							// Завершает входную функцию
