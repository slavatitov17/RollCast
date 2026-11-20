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
constexpr int numberCount = 39;																																// Задаёт количество числовых полей проекта
constexpr int firstNumber = 100;																															// Задаёт первый идентификатор числового поля
constexpr int projectName = 200;																															// Задаёт идентификатор названия расчёта
constexpr int sourceName = 201;																																// Задаёт идентификатор происхождения данных
constexpr int tabsId = 202;																																	// Задаёт идентификатор вкладок параметров
constexpr int statusId = 203;																																// Задаёт идентификатор строки состояния
constexpr int motionId = 204;																																// Задаёт идентификатор модели движения
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
    int selection = 4;																																		// Начинает с отображения всех графиков
    int tab = 0;																																			// Начинает с геометрических параметров
    bool automatic = false;																																	// Управляет автоматической анимацией заполнения
    std::array<std::vector<HWND>, 3> groups;																												// Хранит элементы трёх вкладок
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
std::string utf8(const std::wstring& text) {																												// Преобразует пользовательский текст в UTF-8
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);								// Определяет размер сохраняемой строки
    std::string result(size, 0);																															// Выделяет память UTF-8
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);										// Выполняет преобразование
    return result;																																			// Возвращает переносимый текст проекта
}																																							// Завершает преобразование пользовательского текста
std::wstring text(HWND field) {																																// Читает полное содержимое поля
    const int size = GetWindowTextLengthW(field);																											// Узнаёт длину текста
    std::wstring value(size + 1, 0);																														// Выделяет место вместе с завершающим нулём
    value.resize(GetWindowTextW(field, value.data(), size + 1));																							// Получает фактическое содержимое поля
    return value;																																			// Возвращает введённую строку
}																																							// Завершает чтение текста
void status(HWND window, const wchar_t* value) {																											// Обновляет строку состояния без пересоздания окна
    SetDlgItemTextW(window, statusId, value);																												// Показывает текущее состояние операции
}																																							// Завершает обновление состояния
HWND control(HWND window, const wchar_t* type, const wchar_t* caption, DWORD style, int x, int y, int w, int h, int id = 0) {								// Создаёт стандартный элемент интерфейса
    const HWND child = CreateWindowExW(type[0] == L'E' ? WS_EX_CLIENTEDGE : 0, type, caption, WS_CHILD | WS_VISIBLE | style, x, y, w, h, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);	// Передаёт Windows параметры элемента
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);														// Использует системный шрифт
    return child;																																			// Возвращает дескриптор созданного элемента
}																															// Завершает создание элемента
void groupControl(HWND window, AppState& state, int tab, const wchar_t* type, const wchar_t* caption, int x, int y, int w, int h, int id = 0) {				// Добавляет элемент в группу вкладки
    const DWORD style = type[0] == L'E' ? ES_AUTOHSCROLL | WS_TABSTOP : 0;																					// Настраивает текстовый ввод
    state.groups[tab].push_back(control(window, type, caption, style, x, y, w, h, id));																		// Запоминает элемент выбранной страницы
}																																							// Завершает добавление элемента вкладки
std::array<double, numberCount> numbers(const Project& p) {																									// Сопоставляет проект числовым полям интерфейса
    std::array<double, numberCount> values{};																												// Подготавливает плоский набор значений
    for (std::size_t i = 0; i < 4; ++i) {																													// Сохраняет геометрию по отделениям
        values[i * 4] = p.dimensions[i].diameter;																											// Сохраняет диаметр отделения
        values[i * 4 + 1] = p.dimensions[i].height;																											// Сохраняет осевую длину
        values[i * 4 + 2] = p.sand[i];																														// Сохраняет толщину песка
        values[i * 4 + 3] = p.flask[i];																														// Сохраняет толщину опоки
    }																																						// Завершает геометрические поля
    for (std::size_t i = 0; i < 3; ++i) {																													// Сохраняет свойства трёх материалов
        values[16 + i * 3] = p.materials[i].density;																										// Сохраняет плотность материала
        values[17 + i * 3] = p.materials[i].heatCapacity;																									// Сохраняет удельную теплоёмкость
        values[18 + i * 3] = p.materials[i].conductivity;																									// Сохраняет теплопроводность
    }																																						// Завершает свойства материалов
    const double rest[]{p.solidDensity, p.phase.solidus, p.phase.liquidus, p.phase.latentHeat, p.metalTemperature, p.formTemperature, p.airTemperature, p.metalContact, p.formContact, p.externalTransfer, p.hours, p.step, static_cast<double>(p.cells), p.fillingSeconds};	// Перечисляет фазовые и временные параметры
    std::copy(std::begin(rest), std::end(rest), values.begin() + 25);																						// Заполняет оставшиеся числовые поля
    return values;																																			// Возвращает согласованный набор полей
}																																							// Завершает извлечение значений проекта
Project readProject(HWND window, const AppState& state) {																									// Проверяет все вкладки перед запуском операции
    Project p = state.inputs;																																// Подготавливает независимый кандидат проекта
    std::array<double, numberCount> values{};																												// Собирает числовые значения всех вкладок
    for (int i = 0; i < numberCount; ++i) {																													// Читает каждое числовое поле
        auto value = text(GetDlgItem(window, firstNumber + i));																								// Получает полную строку
        std::replace(value.begin(), value.end(), L',', L'.');																								// Разрешает десятичную запятую русского ввода
        wchar_t* end = nullptr;																																// Принимает конец разобранного числа
        values[i] = std::wcstod(value.c_str(), &end);																										// Разбирает десятичное значение
        if (value.empty() || end != value.c_str() + value.size() || !std::isfinite(values[i])) {															// Отклоняет неполные и нечисловые строки
            throw std::runtime_error("Заполните все числовые поля корректными значениями");																	// Объясняет причину отказа
        }																																					// Завершает проверку числовой строки
    }																																						// Завершает чтение числовых значений
    for (std::size_t i = 0; i < 4; ++i) {																													// Восстанавливает геометрию отделений
        if (values[i * 4] != std::floor(values[i * 4]) || values[i * 4 + 1] != std::floor(values[i * 4 + 1]) || values[i * 4] < 1 || values[i * 4] > 9999 || values[i * 4 + 1] < 1 || values[i * 4 + 1] > 9999) {	// Требует целые размеры в поддерживаемом диапазоне
            throw std::runtime_error("Диаметры и длины задаются целыми миллиметрами 1–9999");																// Объясняет формат геометрических размеров
        }																																					// Завершает проверку целочисленных размеров
        p.dimensions[i] = {static_cast<int>(values[i * 4]), static_cast<int>(values[i * 4 + 1])};															// Применяет размеры соответствующего отделения
        p.sand[i] = values[i * 4 + 2];																														// Применяет толщину песчаного слоя
        p.flask[i] = values[i * 4 + 3];																														// Применяет толщину металлической опоки
    }																																						// Завершает восстановление геометрии
    for (std::size_t i = 0; i < 3; ++i) { p.materials[i] = {values[16 + i * 3], values[17 + i * 3], values[18 + i * 3]}; }									// Восстанавливает три свойства каждого материала
    p.solidDensity = values[25];																															// Применяет необязательную плотность твёрдой фазы
    p.phase = {values[26], values[27], values[28]};																											// Применяет интервал и скрытую теплоту превращения
    p.metalTemperature = values[29]; p.formTemperature = values[30]; p.airTemperature = values[31];															// Применяет начальные температуры
    p.metalContact = values[32]; p.formContact = values[33]; p.externalTransfer = values[34];																// Применяет коэффициенты теплообмена
    p.hours = values[35]; p.step = values[36];																												// Применяет длительность и шаг охлаждения
    if (values[37] != std::floor(values[37]) || values[37] < 8 || values[37] > 256) { throw std::runtime_error("Число ячеек должно быть целым от 8 до 256"); }	// Проверяет число радиальных ячеек
    p.cells = static_cast<int>(values[37]); p.fillingSeconds = values[38];																					// Применяет сетку и длительность заполнения
    p.name = utf8(text(GetDlgItem(window, projectName)));																									// Читает название расчёта
    p.source = utf8(text(GetDlgItem(window, sourceName)));																									// Читает происхождение исходных данных
    p.circulation = SendDlgItemMessageW(window, motionId, BM_GETCHECK, 0, 0) == BST_CHECKED;																// Читает выбор модели движения
    validateProject(p);																																		// Проверяет физическую согласованность всех параметров
    return p;																																				// Возвращает полностью проверенный проект
}																																							// Завершает чтение проекта из интерфейса
void writeProject(HWND window, AppState& state, const Project& p) {																							// Заполняет все вкладки одним набором данных
    state.changing = true;																																	// Подавляет ложные уведомления о редактировании
    state.inputs = p;																																		// Сохраняет принятый набор проекта
    const auto values = numbers(p);																															// Преобразует параметры в порядок полей
    char value[80]{};																																		// Резервирует строку числового значения
    for (int i = 0; i < numberCount; ++i) {																													// Заполняет числовые элементы всех страниц
        const auto formatted = std::to_chars(value, value + std::size(value), values[i]);																	// Выбирает краткую запись без потери точности
        SetDlgItemTextW(window, firstNumber + i, wide(std::string(value, formatted.ptr)).c_str());															// Устанавливает точное значение нужного поля
    }																																						// Завершает заполнение числовых полей
    SetDlgItemTextW(window, projectName, wide(p.name).c_str());																								// Показывает название проекта
    SetDlgItemTextW(window, sourceName, wide(p.source).c_str());																							// Показывает происхождение исходных данных
    SendDlgItemMessageW(window, motionId, BM_SETCHECK, p.circulation ? BST_CHECKED : BST_UNCHECKED, 0);														// Восстанавливает модель движения
    state.changing = false;																																	// Возвращает обработку пользовательских изменений
}																																							// Завершает заполнение редактора
void showTab(AppState& state, int selected) {																												// Переключает группы параметров
    state.tab = selected;																																	// Запоминает выбранную вкладку
    for (int tab = 0; tab < 3; ++tab) {																														// Обходит три страницы редактора
        for (const HWND item : state.groups[tab]) { ShowWindow(item, tab == selected ? SW_SHOW : SW_HIDE); }												// Показывает только элементы текущей страницы
    }																																						// Завершает изменение видимости элементов
}																																							// Завершает переключение вкладки
void createControls(HWND window, AppState& state) {																											// Создаёт панель действий и редактор проекта
    const wchar_t* actions[]{L"Рассчитать", L"Открыть...", L"Сохранить...", L"CSV...", L"Закрепить базу", L"Демо"};											// Называет шесть действий проекта
    const int widths[]{118, 100, 112, 76, 148, 76};																											// Задаёт размеры кнопок действий
    int x = 16;																																				// Начинает панель у левого края
    for (int i = 0; i < 6; ++i) {																															// Создаёт верхние кнопки
        control(window, L"BUTTON", actions[i], BS_PUSHBUTTON | WS_TABSTOP, x, 10, widths[i], 31, i + 1);													// Размещает действие на общей панели
        x += widths[i] + 10;																																// Оставляет промежуток перед следующей кнопкой
    }																																						// Завершает панель действий
    const HWND tabs = control(window, WC_TABCONTROLW, L"", WS_TABSTOP, 24, 92, 344, 29, tabsId);															// Создаёт три вкладки параметров
    const wchar_t* titles[]{L"Геометрия", L"Материалы", L"Условия"};																						// Называет страницы параметров
    for (int i = 0; i < 3; ++i) { TCITEMW item{}; item.mask = TCIF_TEXT; item.pszText = const_cast<wchar_t*>(titles[i]); TabCtrl_InsertItem(tabs, i, &item); }	// Добавляет заголовки вкладок
    groupControl(window, state, 0, L"STATIC", L"Название расчёта", 28, 140, 320, 19);																		// Подписывает название проекта
    groupControl(window, state, 0, L"EDIT", L"", 28, 162, 328, 24, projectName);																			// Создаёт поле названия
    groupControl(window, state, 0, L"STATIC", L"Источник и статус исходных данных", 28, 204, 328, 19);														// Подписывает происхождение входов
    groupControl(window, state, 0, L"EDIT", L"", 28, 226, 328, 24, sourceName);																				// Создаёт поле происхождения данных
    groupControl(window, state, 0, L"STATIC", L"Размеры участков, мм", 28, 269, 320, 20);																	// Обозначает единицы геометрических параметров
    const wchar_t* columns[]{L"Диам.", L"Длина", L"Песок", L"Опока"};																						// Называет геометрические столбцы
    for (int c = 0; c < 4; ++c) { groupControl(window, state, 0, L"STATIC", columns[c], 118 + c * 60, 294, 55, 20); }										// Подписывает четыре столбца размеров
    for (int section = 3; section >= 0; --section) {																										// Показывает отделения сверху вниз
        const int y = 320 + (3 - section) * 33;																												// Располагает геометрические строки
        groupControl(window, state, 0, L"STATIC", sectionNames[section], 28, y + 4, 90, 20);																// Подписывает отделение формы
        for (int c = 0; c < 4; ++c) { groupControl(window, state, 0, L"EDIT", L"", 118 + c * 60, y, 55, 24, firstNumber + section * 4 + c); }				// Создаёт четыре размера нужного отделения
    }																																						// Завершает таблицу геометрии
    groupControl(window, state, 0, L"STATIC", L"Длительность заливки, с", 28, 470, 220, 20);																// Подписывает время иллюстративного заполнения
    groupControl(window, state, 0, L"EDIT", L"", 264, 466, 85, 24, firstNumber + 38);																		// Создаёт поле длительности заливки
    const HWND motion = control(window, L"BUTTON", L"Циркуляция расплава", BS_AUTOCHECKBOX | WS_TABSTOP, 28, 515, 250, 22, motionId);						// Создаёт выбор модели движения
    state.groups[0].push_back(motion);																														// Относит переключатель к геометрической странице
    groupControl(window, state, 0, L"STATIC", L"Охлаждение через боковые стенки.\nВремя расчёта 0 – конец заполнения.", 28, 560, 320, 45);					// Поясняет начало теплового отсчёта
    groupControl(window, state, 1, L"STATIC", L"Постоянные свойства материалов", 28, 140, 320, 20);															// Называет таблицу теплофизических свойств
    const wchar_t* properties[]{L"ρ, кг/м³", L"c, Дж/(кг·К)", L"λ, Вт/(м·К)"};																				// Задаёт единицы трёх свойств
    for (int c = 0; c < 3; ++c) { groupControl(window, state, 1, L"STATIC", properties[c], 104 + c * 84, 175, 84, 20); }									// Подписывает свойства материалов
    const wchar_t* materials[]{L"Расплав", L"Песок", L"Опока"};																								// Называет три материала модели
    for (int material = 0; material < 3; ++material) {																										// Создаёт строки физических свойств
        groupControl(window, state, 1, L"STATIC", materials[material], 28, 208 + material * 34, 72, 20);													// Подписывает материал
        for (int c = 0; c < 3; ++c) { groupControl(window, state, 1, L"EDIT", L"", 104 + c * 84, 204 + material * 34, 78, 24, firstNumber + 16 + material * 3 + c); }	// Создаёт поля плотности теплоёмкости и проводимости
    }																																						// Завершает строки материалов
    const wchar_t* phases[]{L"Плотность твёрдой фазы, кг/м³", L"Солидус, °C", L"Ликвидус, °C", L"Теплота кристаллизации, Дж/кг"};							// Называет дополнительные фазовые свойства
    for (int i = 0; i < 4; ++i) {																															// Создаёт параметры фазового превращения
        groupControl(window, state, 1, L"STATIC", phases[i], 28, 330 + i * 40, 230, 24);																	// Подписывает фазовый параметр
        groupControl(window, state, 1, L"EDIT", L"", 264, 326 + i * 40, 85, 24, firstNumber + 25 + i);														// Размещает поле свойства фазы
    }																																						// Завершает фазовые параметры
    groupControl(window, state, 1, L"STATIC", L"Плотность твёрдой фазы 0 отключает\nдополнительную оценку усадки в CSV.", 28, 530, 320, 48);				// Объясняет смысл необязательной оценки
    const wchar_t* conditions[]{L"Начальная температура металла, °C", L"Начальная температура формы, °C", L"Температура воздуха, °C", L"Контакт металл–песок, Вт/(м²·К)", L"Контакт песок–опока, Вт/(м²·К)", L"Наружная теплоотдача, Вт/(м²·К)", L"Длительность расчёта, ч", L"Максимальный шаг времени, с", L"Ячеек по радиусу металла"};	// Называет начальные и граничные условия
    for (int i = 0; i < 9; ++i) {																															// Создаёт поля условий расчёта
        groupControl(window, state, 2, L"STATIC", conditions[i], 28, 153 + i * 42, 232, 33);																// Подписывает физический параметр
        groupControl(window, state, 2, L"EDIT", L"", 264, 152 + i * 42, 85, 24, firstNumber + 29 + i);														// Размещает поле условия расчёта
    }																																						// Завершает начальные и граничные условия
    groupControl(window, state, 2, L"STATIC", L"Контакт 0 – идеальное сопряжение.\nНаружная теплоотдача 0 – теплоизоляция.", 28, 560, 320, 46);				// Поясняет нулевые значения коэффициентов
    control(window, L"STATIC", L"", 0, 28, 635, 326, 72, statusId);																							// Создаёт общую строку состояния
    control(window, TRACKBAR_CLASSW, L"", TBS_NOTICKS | WS_TABSTOP, 0, 0, 100, 25, sliderId);																// Создаёт общую круглую временную ручку
    SendDlgItemMessageW(window, sliderId, TBM_SETRANGE, TRUE, MAKELPARAM(0, timelineEnd));																	// Задаёт разрешение временной шкалы
    control(window, L"STATIC", L"00:00:00", 0, 0, 0, 85, 22, clockId);																						// Создаёт подпись времени слева от ползунка
    const HWND picker = control(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, 0, 190, 160, chartPicker);							// Создаёт выбор показываемых графиков
    for (const auto* name : {L"Прибыль", L"Верхняя шейка", L"Бочка", L"Нижняя шейка", L"Все"}) { SendMessageW(picker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name)); }	// Заполняет список в требуемом порядке
    SendMessageW(picker, CB_SETCURSEL, 4, 0);																												// Начинает с четырёх графиков
    const HWND tips = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);	// Создаёт подсказки кнопок экспорта
    for (int section = 0; section < 4; ++section) {																											// Создаёт четыре кнопки иконки
        const HWND button = control(window, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 0, 0, 22, 22, firstExport + section);								// Создаёт иконку выгрузки графика
        TOOLINFOW tip{}; tip.cbSize = sizeof(tip); tip.uFlags = TTF_IDISHWND | TTF_SUBCLASS; tip.hwnd = window;												// Настраивает всплывающую подсказку
        tip.uId = reinterpret_cast<UINT_PTR>(button); tip.lpszText = const_cast<wchar_t*>(L"Выгрузить график в PNG");										// Задаёт доступное объяснение иконки
        SendMessageW(tips, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tip));																				// Связывает подсказку с кнопкой
    }																																						// Завершает кнопки выгрузки
    showTab(state, 0);																																		// Открывает геометрию при запуске
    writeProject(window, state, state.inputs);																												// Заполняет исходный демонстрационный проект
}																																							// Завершает создание интерфейса
std::size_t frameIndex(const AppState& state) {																												// Сопоставляет ползунок кадру охлаждения
    if (!state.result || state.position <= fillingEnd) { return 0; }																						// Сохраняет начальную температуру во время заливки
    const auto count = state.result->cooling.section(0).samples.size();																						// Читает количество согласованных кадров
    return static_cast<std::size_t>((state.position - fillingEnd) * static_cast<double>(count - 1) / (timelineEnd - fillingEnd));							// Выбирает ближайший предшествующий кадр
}																																							// Завершает выбор кадра
double fillProgress(const AppState& state) {																												// Определяет степень заполнения формы
    return std::min(1.0, state.position / static_cast<double>(fillingEnd));																					// Отделяет заливку от охлаждения
}																																							// Завершает определение стадии
void arrange(HWND window, AppState& state) {																												// Размещает элементы под схемой и графиками
    const auto areas = layout(window);																														// Получает текущую компоновку
    MoveWindow(GetDlgItem(window, sliderId), areas.mold.left + 88, areas.client.bottom - 78, areas.mold.right - areas.mold.left - 92, 26, TRUE);			// Размещает шкалу вне белой области
    MoveWindow(GetDlgItem(window, clockId), areas.mold.left + 4, areas.client.bottom - 75, 85, 22, TRUE);													// Ставит время слева от ползунка
    MoveWindow(GetDlgItem(window, chartPicker), areas.results.left + 12, areas.results.top + 185, 190, 180, TRUE);											// Размещает список под сводной таблицей
    const auto cards = chartAreas(areas.results, state.selection);																							// Получает положения видимых графиков
    for (int i = 0; i < 4; ++i) {																															// Обновляет четыре иконки экспорта
        const HWND button = GetDlgItem(window, firstExport + i);																							// Получает кнопку отделения
        MoveWindow(button, cards[i].right - 27, cards[i].top + 4, 22, 22, TRUE);																			// Закрепляет иконку в правом верхнем углу
        ShowWindow(button, state.result && !state.running && !IsRectEmpty(&cards[i]) ? SW_SHOW : SW_HIDE);													// Скрывает кнопки отсутствующих графиков
        EnableWindow(button, !state.edited);																												// Требует актуальные входные данные
    }																																						// Завершает размещение иконок
    SetDlgItemTextW(window, 5, state.baseline || state.baselineInputs ? L"Убрать базу" : L"Закрепить базу");												// Показывает текущее действие сравнения
}																																							// Завершает размещение элементов
void updateTime(HWND window, AppState& state) {																												// Обновляет выбранный момент без пересчёта
    double seconds = fillProgress(state) * state.inputs.fillingSeconds;																						// Отсчитывает время от начала заливки
    if (state.result && state.position > fillingEnd && !state.running) { seconds += state.result->cooling.section(0).samples[frameIndex(state)].seconds; }	// Добавляет время после заполнения
    const auto whole = static_cast<unsigned long long>(std::llround(seconds));																				// Округляет время до целых секунд
    wchar_t caption[40]{};																																	// Резервирует строку часов
    std::swprintf(caption, std::size(caption), L"%02llu:%02llu:%02llu", whole / 3600, whole / 60 % 60, whole % 60);											// Форматирует часы минуты секунды
    SetDlgItemTextW(window, clockId, caption);																												// Показывает время слева от шкалы
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
    status(window, L"Выполняется расчёт. Параметры можно редактировать");																					// Сообщает о фоновой работе
    arrange(window, state); updateTime(window, state);																										// Обновляет доступность и начальный кадр
    SetTimer(window, 1, 50, nullptr);																														// Запускает плавное воспроизведение заливки
    state.worker = std::jthread([window, project, restore, generation](std::stop_token stop) {																// Передаёт потоку независимую копию входов
        auto completed = std::make_unique<Completed>();																										// Создаёт сообщение для оконного потока
        completed->generation = generation;																													// Отмечает принадлежность результата заданию
        try {																																				// Защищает оконный поток от расчётных исключений
            completed->result = std::make_shared<Run>();																									// Выделяет независимый текущий результат
            completed->result->project = project;																											// Сохраняет входы вместе с историей
            completed->result->cooling.calculate(project, stop);																							// Вычисляет четыре отделения вне интерфейса
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
        status(window, L"Сначала нажмите «Рассчитать» и дождитесь завершения");																				// Объясняет необходимость актуального расчёта
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
    drawSingleChart(image.dc, area, *state.result, state.baseline.get(), section, frameIndex(state), cache, fillProgress(state));							// Рисует полный график с подписями
    const CLSID png{0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};															// Выбирает встроенный кодировщик PNG
    Gdiplus::Bitmap bitmap(image.bitmap, nullptr);																											// Оборачивает готовый растр для кодировщика
    if (bitmap.Save(path.c_str(), &png, nullptr) != Gdiplus::Ok) { throw std::runtime_error("Не удалось сохранить PNG"); }									// Проверяет успешность записи изображения
    status(window, L"График выгружен в PNG");																												// Подтверждает результат выгрузки
}																																							// Завершает экспорт отдельного графика
void action(HWND window, AppState& state, int id) {																											// Выполняет действия верхней панели
    if (id == 1) { calculate(window, state, readProject(window, state)); }																					// Пересчитывает введённые пользователем параметры
    if (id == 2 && discard(window, state)) {																												// Открывает другой проект после проверки изменений
        const auto path = chooseFile(window, state, L"Проект RollCast\0*.roll\0\0", L"roll", false);														// Предлагает выбрать сохранённый проект
        if (!path.empty()) {																																// Продолжает только после выбора файла
            const auto project = loadProject(path);																											// Проверяет файл до замены полей
            wide(project.name); wide(project.source);																										// Проверяет кодировку пользовательского текста
            writeProject(window, state, project);																											// Загружает параметры всех вкладок
            state.filename = path; state.unsaved = false;																									// Запоминает происхождение открытого проекта
            calculate(window, state, project);																												// Рассчитывает историю открытого проекта
        }																																					// Завершает обработку выбранного файла
    }																																						// Завершает открытие проекта
    if (id == 3) {																																			// Сохраняет параметры и доступную итоговую сводку
        const auto project = readProject(window, state);																									// Проверяет сохранение всех вкладок
        const auto path = chooseFile(window, state, L"Проект RollCast\0*.roll\0\0", L"roll", true);															// Предлагает путь нового проекта
        if (!path.empty()) {																																// Учитывает отмену выбора
            saveProject(path, project, !state.running && state.result && state.result->project == project ? state.result.get() : nullptr);					// Записывает только соответствующую входам сводку
            state.filename = path; state.unsaved = false;																									// Отмечает успешное сохранение проекта
            status(window, L"Проект сохранён");																												// Подтверждает запись файла
        }																																					// Завершает сохранение выбранного проекта
    }																																						// Завершает действие сохранения
    if (id == 4 && current(window, state)) {																												// Выгружает полную рассчитанную историю
        const auto path = chooseFile(window, state, L"Таблица CSV\0*.csv\0\0", L"csv", true);																// Предлагает путь таблицы
        if (!path.empty()) { exportHistory(path, *state.result); status(window, L"История выгружена в CSV"); }												// Записывает все четыре отделения
    }																																						// Завершает выгрузку полной истории
    if (id == 5) {																																			// Закрепляет или снимает базовое сравнение
        const auto path = state.directory / L".rollcast-base.roll";																							// Хранит базу между запусками программы
        if (state.running) { status(window, L"Дождитесь завершения расчёта"); return; }																		// Защищает восстановление базы от гонки
        if (state.baseline || state.baselineInputs) {																										// Снимает существующее закрепление
            std::filesystem::remove(path);																													// Удаляет только собственный файл базы
            state.baseline.reset(); state.baselineInputs.reset();																							// Освобождает историю закреплённого проекта
            status(window, L"База сравнения убрана");																										// Подтверждает снятие закрепления
        } else if (current(window, state)) {																												// Закрепляет актуальный рассчитанный проект
            saveProject(path, state.result->project, state.result.get());																					// Сохраняет воспроизводимые входы базы
            state.baseline = state.result; state.baselineInputs = state.result->project;																	// Удерживает независимую историю для сравнения
            status(window, L"База закреплена и сохранена между запусками");																					// Подтверждает постоянное хранение сравнения
        }																																					// Завершает изменение состояния базы
        state.curves = {}; arrange(window, state);																											// Сбрасывает масштаб и название действия
        const auto area = layout(window).results; InvalidateRect(window, &area, FALSE);																		// Обновляет базовые кривые и разницу времени
    }																																						// Завершает действие сравнения
    if (id == 6 && discard(window, state)) {																												// Возвращает исходный демонстрационный проект
        const Project project; writeProject(window, state, project);																						// Восстанавливает все значения трёх вкладок
        state.filename.clear(); state.unsaved = false;																										// Отмечает новый исходный проект
        calculate(window, state, project);																													// Пересчитывает восстановленный пример
    }																																						// Завершает восстановление примера
    if (id >= firstExport && id < firstExport + 4) { exportChart(window, state, id - firstExport); }														// Выгружает выбранное отделение
}																																							// Завершает обработку действий
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
            for (std::size_t i = 0; i < 4; ++i) { frame.samples[i] = &state.result->cooling.section(i).samples[frameIndex(state)]; }						// Передаёт профили выбранного кадра
        }																																					// Завершает подготовку температуры схемы
        drawMold(dc, areas.mold, state.inputs.dimensions, state.hits, state.hovered, state.cursor, frame, state.rebuildMold);								// Рисует форму без пересоздания постоянных областей
        state.rebuildMold = false;																															// Сохраняет геометрические области для следующего кадра
        drawMoldLabel(dc, areas.mold, state.hovered, state.cursor);																							// Показывает название выбранной детали
    }																																						// Завершает отдельное обновление схемы
    if (IntersectRect(&intersection, &painting.rcPaint, &areas.results)) {																					// Пропускает графики при обычном наведении
        if (state.result && !state.running) { drawMoldDashboard(dc, areas.results, *state.result, state.baseline.get(), frameIndex(state), fillProgress(state), state.selection, state.curves); }	// Использует заранее подготовленные компактные кривые
        else { const wchar_t* message = state.running ? L"Выполняется расчёт..." : L"Нажмите «Рассчитать»"; TextOutW(dc, areas.results.left + 15, areas.results.top + 15, message, static_cast<int>(std::wcslen(message))); }	// Поясняет отсутствие готовой истории
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
        Ellipse(draw->hdc, x - 7, y - 7, x + 8, y + 8);																										// Рисует круглую ручку шкалы
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
            calculate(window, *state, state->inputs); return 0;																								// Запускает пример без блокирования окна
        case WM_GETMINMAXINFO:																																// Сохраняет видимость всех трёх панелей
            reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {1240, 800}; return 0;																	// Ограничивает минимальный размер рабочего окна
        case WM_SIZE:																																		// Перестраивает расположение после изменения размеров
            state->rebuildMold = true; state->curves = {};																									// Сбрасывает зависящие от размеров данные
            arrange(window, *state); InvalidateRect(window, nullptr, FALSE); return 0;																		// Обновляет расположение и полный кадр
        case WM_ERASEBKGND: return 1;																														// Подавляет мерцание при двойной буферизации
        case WM_PAINT: paint(window, *state); return 0;																										// Обновляет только повреждённые области окна
        case WM_COMMAND: {																																	// Обрабатывает редактор список и панель действий
            const int id = LOWORD(wParam); const int code = HIWORD(wParam);																					// Разбирает источник пользовательского действия
            if (!state->changing && ((code == EN_CHANGE && ((id >= firstNumber && id < firstNumber + numberCount) || id == projectName || id == sourceName)) || (id == motionId && code == BN_CLICKED))) {	// Отмечает изменение исходных данных
                state->unsaved = true; state->edited = true;																								// Отделяет редактор от рассчитанного результата
                status(window, L"Параметры изменены. Нажмите «Рассчитать»"); arrange(window, *state);														// Требует явного подтверждения нового расчёта
            } else if (id == chartPicker && code == CBN_SELCHANGE) {																						// Меняет набор показываемых графиков
                state->selection = static_cast<int>(SendDlgItemMessageW(window, chartPicker, CB_GETCURSEL, 0, 0));											// Запоминает выбранное отделение
                arrange(window, *state); const auto area = layout(window).results; InvalidateRect(window, &area, FALSE);									// Перерисовывает только панель результатов
            } else if (code == BN_CLICKED && ((id >= 1 && id <= 6) || (id >= firstExport && id < firstExport + 4))) { action(window, *state, id); }			// Выполняет действие выбранной кнопки
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
                status(window, state->edited ? L"Расчёт завершён. Введённые изменения требуют пересчёта" : L"Расчёт завершён. Выберите момент на шкале");	// Показывает соответствие истории текущим полям
            } else { state->result.reset(); status(window, completed->error.c_str()); }																		// Показывает причину неудачного расчёта
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
        case WM_DRAWITEM: {																																	// Рисует иконку выгрузки без текстовой кнопки
            const auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);																					// Получает контекст кнопки
            FillRect(item->hDC, &item->rcItem, GetSysColorBrush(COLOR_BTNFACE));																			// Очищает фон иконки
            const HPEN pen = CreatePen(PS_SOLID, 2, GetSysColor(item->itemState & ODS_DISABLED ? COLOR_GRAYTEXT : COLOR_HIGHLIGHT));						// Выбирает цвет доступного действия
            const auto previous = SelectObject(item->hDC, pen);																								// Сохраняет прежнее перо
            const int x = (item->rcItem.left + item->rcItem.right) / 2; const int y = item->rcItem.top + 3;													// Находит центр значка
            MoveToEx(item->hDC, x, y, nullptr); LineTo(item->hDC, x, y + 10);																				// Рисует вертикальную стрелку вниз
            MoveToEx(item->hDC, x - 4, y + 6, nullptr); LineTo(item->hDC, x, y + 10); LineTo(item->hDC, x + 4, y + 6);										// Рисует наконечник стрелки
            MoveToEx(item->hDC, x - 6, y + 12, nullptr); LineTo(item->hDC, x - 6, y + 15); LineTo(item->hDC, x + 6, y + 15); LineTo(item->hDC, x + 6, y + 12);	// Рисует приёмный лоток
            SelectObject(item->hDC, previous); DeleteObject(pen);																							// Освобождает временное перо
            if (item->itemState & ODS_FOCUS) { DrawFocusRect(item->hDC, &item->rcItem); }																	// Показывает клавиатурный фокус кнопки
            return TRUE;																																	// Завершает рисование иконки
        }																																					// Завершает пользовательское оформление кнопок
        case WM_DESTROY: {																																	// Завершает расчёт перед закрытием приложения
            state->worker.request_stop(); if (state->worker.joinable()) { state->worker.join(); }															// Дожидается остановки фонового потока
            MSG pending{}; while (PeekMessageW(&pending, window, calculationDone, calculationDone, PM_REMOVE)) { delete reinterpret_cast<Completed*>(pending.lParam); }	// Освобождает недоставленные результаты
            KillTimer(window, 1); PostQuitMessage(0); return 0;																								// Завершает таймер и оконный цикл
        }																																					// Завершает закрытие приложения
        default: break;																																		// Передаёт прочие сообщения стандартной процедуре
        }																																					// Завершает выбор оконного сообщения
    } catch (const std::exception& error) { status(window, wide(error.what()).c_str()); }																	// Показывает ошибку без аварийного завершения
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
        const auto base = state.directory / L".rollcast-base.roll";																							// Определяет путь закреплённого проекта
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
