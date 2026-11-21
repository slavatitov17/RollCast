#include "about_view.h"	// Подключает встроенный разбор теории и кода
#include "cooling_view.h"	// Подключает быстрые графики и структуру результата
#include <commctrl.h>	// Предоставляет вкладки список и ползунок
#include <commdlg.h>	// Предоставляет диалоги открытия и сохранения
#include <gdiplus.h>	// Сохраняет изображения графиков в PNG
#include <windowsx.h>	// Извлекает координаты мыши из сообщений
#include <algorithm>	// Ограничивает размеры панелей и временные индексы
#include <cmath>	// Проверяет конечность введенных чисел
#include <charconv>	// Сохраняет точность отображаемых исходных чисел
#include <cwchar>	// Преобразует числовые поля интерфейса
#include <memory>	// Управляет временем жизни рассчитанных результатов
#include <thread>	// Выполняет расчет вне оконного потока
#include <stdexcept>	// Передает ошибки проверки пользователю
namespace {	// Ограничивает видимость оконной реализации
constexpr UINT calculationDone = WM_APP + 1;	// Задает сообщение завершения фоновой работы
constexpr int numberCount = 43;	// Описывает параметры текущего действия
constexpr int firstNumber = 100;	// Задает первый идентификатор числового поля
constexpr int tabsId = 202;	// Задает идентификатор вкладок параметров
constexpr int sliderId = 205;	// Задает идентификатор временного ползунка
constexpr int clockId = 206;	// Задает идентификатор временной подписи
constexpr int chartPicker = 207;	// Задает идентификатор выбора графиков
constexpr int firstExport = 300;	// Задает первый идентификатор экспорта графика
constexpr int fillingEnd = 2000;	// Отводит начало шкалы анимации заполнения
constexpr int timelineEnd = 10000;	// Задает разрешение общей временной шкалы
struct Layout { RECT client; RECT parameters; RECT mold; RECT results; };	// Хранит области единого рабочего экрана
Layout layout(HWND window, bool timeline = true) {	// Описывает параметры текущего действия
    RECT client{};	// Принимает доступную область окна
    GetClientRect(window, &client);	// Читает размеры без системной рамки
    const int width = std::clamp((client.right - 340) / 3, 300L, 440L);	// Описывает параметры текущего действия
    return {client, {16, 80, 310, client.bottom - 16}, {322, 80, 322 + width, client.bottom - (timeline ? 95 : 16)}, {334 + width, 80, client.right - 16, client.bottom - 16}};	// Описывает параметры текущего действия
}	// Завершает текущий блок обработки
struct Surface {	// Хранит повторно используемый буфер рисования
    HDC dc = nullptr;	// Хранит контекст буфера
    HBITMAP bitmap = nullptr;	// Хранит растровую поверхность
    HGDIOBJ previous = nullptr;	// Сохраняет исходный объект контекста
    int width = 0;	// Запоминает ширину буфера
    int height = 0;	// Запоминает высоту буфера
    ~Surface() { clear(); }	// Освобождает графические ресурсы
    void clear() {	// Уничтожает поверхность перед изменением размера
        if (dc && previous) { SelectObject(dc, previous); }	// Возвращает исходный объект контекста
        if (bitmap) { DeleteObject(bitmap); }	// Освобождает растровый буфер
        if (dc) { DeleteDC(dc); }	// Освобождает графический контекст
        dc = nullptr; bitmap = nullptr; previous = nullptr;	// Обнуляет освобожденные дескрипторы
    }	// Завершает текущий блок обработки
    void ensure(HDC target, int w, int h) {	// Меняет буфер только после изменения размеров
        if (dc && width == w && height == h) { return; }	// Переиспользует подходящую поверхность
        clear();	// Освобождает прежнюю поверхность
        dc = CreateCompatibleDC(target);	// Создает контекст рисования в памяти
        bitmap = CreateCompatibleBitmap(target, std::max(1, w), std::max(1, h));	// Создает полноразмерный растровый буфер
        if (!dc || !bitmap) { clear(); throw std::runtime_error("Недостаточно графической памяти"); }	// Отклоняет неудачное создание буфера
        previous = SelectObject(dc, bitmap);	// Выбирает новую поверхность
        width = w; height = h;	// Запоминает действительный размер
    }	// Завершает текущий блок обработки
};	// Завершает объявление структуры или набора данных
struct Completed {	// Передает результат из фонового потока
    unsigned long long generation;	// Различает актуальные и отмененные задания
    std::shared_ptr<Run> result;	// Хранит опубликованный неизменяемый результат
    std::wstring error;	// Передает диагностическое сообщение
};	// Завершает объявление структуры или набора данных
struct AppState {	// Хранит состояние оконного приложения
    Project inputs;	// Хранит последний принятый набор исходных данных
    std::shared_ptr<Run> result;	// Хранит опубликованный неизменяемый результат
    std::jthread worker;	// Управляет отменяемым фоновым расчетом
    unsigned long long generation = 0;	// Нумерует расчетные задания
    bool running = false;	// Отмечает выполнение фоновой работы
    bool changing = false;	// Подавляет уведомления при заполнении полей
    bool unsaved = false;	// Отмечает несохраненные пользовательские изменения
    bool edited = false;	// Требует пересчета после изменения входов
    int position = 0;	// Хранит положение общей временной шкалы
    int selection = 0;	// Начинает с отображения всех графиков
    int tab = 0;	// Начинает с геометрических параметров
    bool automatic = false;	// Управляет автоматической анимацией заполнения
    bool started = false;	// Отличает пустой запуск от воспроизведения
    int zoom = 100;	// Хранит масштаб объема без смещения графика
    LayerMetric metric = LayerMetric::volume;	// Выбирает отображаемый показатель без изменения истории
    unsigned layers = 7;	// Хранит независимый выбор трех слоев
    Project preview;	// Хранит проверенную геометрию предварительного просмотра
    std::array<std::vector<HWND>, 3> groups;	// Описывает параметры текущего действия
    ChartCache curves;	// Кэширует компактные температурные линии
    MoldHitMap hits;	// Хранит области наведения и полости металла
    RECT moldGeometry{};	// Запоминает область построенной геометрии
    bool rebuildMold = true;	// Отмечает необходимость пересоздать области формы
    MoldPart hovered = MoldPart::none;	// Хранит выделенную деталь формы
    POINT cursor{};	// Хранит положение подписи
    bool tracking = false;	// Отслеживает уход мыши со схемы
    Surface buffer;	// Переиспользует графический буфер окна
    std::filesystem::path directory;	// Хранит исходный каталог файловых диалогов
    std::filesystem::path filename;	// Хранит путь последнего файла проекта
};	// Завершает объявление структуры или набора данных
std::wstring wide(const std::string& text) {	// Преобразует UTF-8 в строки Windows
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);	// Определяет необходимый размер строки
    if (size == 0 && !text.empty()) { throw std::runtime_error("Некорректная кодировка текста"); }	// Проверяет допустимость UTF-8
    std::wstring result(size, 0);	// Выделяет память строки Unicode
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);	// Преобразует исходный текст
    return result;	// Возвращает строку для Windows
}	// Завершает текущий блок обработки
std::wstring text(HWND field) {	// Читает полное содержимое поля
    const int size = GetWindowTextLengthW(field);	// Узнает длину текста
    std::wstring value(size + 1, 0);	// Выделяет место вместе с завершающим нулем
    value.resize(GetWindowTextW(field, value.data(), size + 1));	// Получает фактическое содержимое поля
    return value;	// Возвращает введенную строку
}	// Завершает текущий блок обработки
HWND control(HWND window, const wchar_t* type, const wchar_t* caption, DWORD style, int x, int y, int w, int h, int id = 0) {	// Создает стандартный элемент интерфейса
    const HWND child = CreateWindowExW(type[0] == L'E' ? WS_EX_CLIENTEDGE : 0, type, caption, WS_CHILD | WS_VISIBLE | style, x, y, w, h, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);	// Передает Windows параметры элемента
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);	// Использует системный шрифт
    return child;	// Возвращает дескриптор созданного элемента
}	// Завершает текущий блок обработки
void groupControl(HWND window, AppState& state, int tab, const wchar_t* type, const wchar_t* caption, int x, int y, int w, int h, int id = 0) {	// Добавляет элемент в группу вкладки
    const DWORD style = type[0] == L'E' ? ES_AUTOHSCROLL | WS_TABSTOP : 0;	// Настраивает текстовый ввод
    state.groups[tab].push_back(control(window, type, caption, style, x, y, w, h, id));	// Запоминает элемент выбранной страницы
}	// Завершает текущий блок обработки
std::array<double, numberCount> numbers(const Project& p) {	// Сопоставляет входы видимым числовым полям
    std::array<double, numberCount> values{};	// Подготавливает единый порядок редактируемых параметров
    for (std::size_t i = 0; i < sectionCount; ++i) { values[2 * i] = p.dimensions[i].diameter; values[2 * i + 1] = p.dimensions[i].height; }	// Связывает пары D и L с номерами
    const double geometry[]{p.chill, p.coating, p.barrelFlask, p.sand, p.flask, p.pipeDiameter, p.hours * 60, p.step, p.diameterStep};	// Перечисляет стенки и численные параметры
    std::copy(std::begin(geometry), std::end(geometry), values.begin() + 10);	// Переносит геометрию и настройки сетки
    for (std::size_t i = 0; i < 4; ++i) { values[19 + 3 * i] = p.materials[i].density; values[20 + 3 * i] = p.materials[i].heatCapacity; values[21 + 3 * i] = p.materials[i].conductivity; }	// Сохраняет три свойства каждого материала
    const double thermal[]{p.phase.latentHeat, p.phase.liquidus, p.phase.solidus, p.metalTemperature, p.formTemperature, p.airTemperature, p.externalTransfer};	// Перечисляет фазовые и температурные параметры
    std::copy(std::begin(thermal), std::end(thermal), values.begin() + 31);	// Заполняет оставшиеся поля материалов
    values[38] = p.paintTemperature; values[39] = p.ironTemperature; values[40] = p.sandTemperature;	// Заполняет температуры трех слоев оснастки
    values[41] = p.ironLiquidus; values[42] = p.ironSolidus;	// Заполняет контрольные пороги металлической оснастки
    return values;	// Возвращает согласованный набор полей
}	// Завершает текущий блок обработки
Project readProject(HWND window, const AppState& state) {	// Читает обе вкладки перед явным расчетом
    Project p = state.inputs; std::array<double, numberCount> values = numbers(p);	// Описывает параметры текущего действия
    for (int i = 0; i < numberCount; ++i) {	// Заполняет каждое поле без потери точности
        if (!GetDlgItem(window, firstNumber + i)) { continue; }	// Сохраняет скрытые теплофизические постоянные без полей
        auto value = text(GetDlgItem(window, firstNumber + i));	// Получает полное содержимое числового поля
        if (i == 16 && value == L"Авто") { continue; }	// Разрешает специальное значение автоматического горизонта
        std::replace(value.begin(), value.end(), L',', L'.');	// Разрешает десятичную запятую русского ввода
        wchar_t* end = nullptr; values[i] = std::wcstod(value.c_str(), &end);	// Разбирает десятичное числовое значение
        if (value.empty() || end != value.c_str() + value.size() || !std::isfinite(values[i])) { throw std::runtime_error("Заполните все числовые поля корректными значениями"); }	// Отклоняет неполные и нечисловые строки
    }	// Завершает текущий блок обработки
    for (std::size_t i = 0; i < sectionCount; ++i) {	// Восстанавливает пять пар геометрических размеров
        const double d = values[2 * i], length = values[2 * i + 1];	// Выбирает соответствующие D и L
        if (d != std::floor(d) || length != std::floor(length) || d < 1 || d > 9999 || length < 1 || length > 9999) { throw std::runtime_error("D1–D5 и L1–L5 задаются целыми миллиметрами 1–9999"); }	// Проверяет целочисленные размеры перед преобразованием
        p.dimensions[i] = {static_cast<int>(d), static_cast<int>(length)};	// Применяет размеры к своему отделению
    }	// Завершает текущий блок обработки
    p.chill = values[10]; p.coating = values[11]; p.barrelFlask = values[12];	// Применяет стенки кокиля краски и опоки
    p.sand = values[13]; p.flask = values[14]; p.pipeDiameter = values[15];	// Применяет песчаную форму и литниковую трубу
    p.autoTime = text(GetDlgItem(window, firstNumber + 16)) == L"Авто";	// Описывает параметры текущего действия
    p.hours = values[16] / 60; p.step = values[17]; p.diameterStep = values[18];	// Переводит минуты и применяет численную сетку
    for (std::size_t i = 0; i < 4; ++i) { p.materials[i] = {values[19 + 3 * i], values[20 + 3 * i], values[21 + 3 * i]}; }	// Восстанавливает свойства четырех материалов
    p.phase = {values[33], values[32], values[31]};	// Сохраняет порядок солидус ликвидус скрытая теплота
    p.metalTemperature = values[34]; p.formTemperature = values[35];	// Применяет начальные температуры металла и формы
    p.airTemperature = values[36]; p.externalTransfer = values[37];	// Применяет наружное конвективное граничное условие
    p.paintTemperature = values[38]; p.ironTemperature = values[39]; p.sandTemperature = values[40];	// Применяет независимые начальные температуры оснастки
    p.ironLiquidus = values[41]; p.ironSolidus = values[42];	// Применяет контрольные пороги кокиля и опоки
    validateProject(p); return p;	// Возвращает только согласованный набор входов
}	// Завершает текущий блок обработки
void updateDerived(HWND window, AppState& state) {	// Обновляет только геометрические производные поля
    const bool changing = state.changing; state.changing = true;	// Исключает повторные события служебных полей
    try {	// Обрабатывает операцию с контролем возможных ошибок
        const auto p = readProject(window, state); state.preview = p; state.rebuildMold = true; wchar_t value[80]{};	// Описывает параметры текущего действия
        std::swprintf(value, std::size(value), L"%.6g", outerDiameter(p)); SetDlgItemTextW(window, 210, value);	// Показывает вычисленный наружный диаметр D0
        std::swprintf(value, std::size(value), L"%.6g", totalLength(p)); SetDlgItemTextW(window, 211, value);	// Показывает сумму осевых размеров L0
    } catch (...) { SetDlgItemTextW(window, 210, L""); SetDlgItemTextW(window, 211, L""); }	// Очищает габариты при незавершенном вводе
    state.changing = changing;	// Восстанавливает обработку пользовательских изменений
}	// Завершает текущий блок обработки
void writeProject(HWND window, AppState& state, const Project& p) {	// Заполняет обе вкладки одним согласованным проектом
    state.changing = true; state.inputs = p;	// Подавляет ложное редактирование при загрузке
    const auto values = numbers(p); char value[80]{};	// Подготавливает числовые значения всех полей
    for (int i = 0; i < numberCount; ++i) {	// Заполняет каждое поле без потери точности
        const auto formatted = std::to_chars(value, value + std::size(value), values[i]);	// Выбирает краткую точную запись числа
        SetDlgItemTextW(window, firstNumber + i, wide(std::string(value, formatted.ptr)).c_str());	// Показывает значение соответствующего параметра
    }	// Завершает текущий блок обработки
    SetDlgItemTextW(window, firstNumber + 16, p.autoTime ? L"Авто" : text(GetDlgItem(window, firstNumber + 16)).c_str());	// Описывает параметры текущего действия
    state.changing = false; updateDerived(window, state);	// Обновляет габариты без теплового расчета
}	// Завершает текущий блок обработки
void showTab(AppState& state, int selected) {	// Переключает группы параметров
    state.tab = selected;	// Запоминает выбранную вкладку
    for (int tab = 0; tab < 3; ++tab) {	// Описывает параметры текущего действия
        for (const HWND item : state.groups[tab]) { ShowWindow(item, tab == selected ? SW_SHOW : SW_HIDE); }	// Показывает только элементы текущей страницы
    }	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
void createControls(HWND window, AppState& state) {	// Создает компактный редактор и панель действий
    control(window, L"BUTTON", L"Сохранить в .roll", BS_PUSHBUTTON | WS_TABSTOP, 16, 10, 155, 31, 3);	// Ставит сохранение проекта первым действием
    control(window, L"BUTTON", L"Сохранить в .csv", BS_PUSHBUTTON | WS_TABSTOP, 181, 10, 155, 31, 4);	// Ставит выгрузку истории вторым действием
    control(window, L"BUTTON", L"Открыть из файла", BS_PUSHBUTTON | WS_TABSTOP, 346, 10, 155, 31, 2);	// Добавляет загрузку входов без автоматического расчета
    control(window, L"BUTTON", L"О программе", BS_PUSHBUTTON | WS_TABSTOP, 0, 10, 130, 31, 6);	// Создает правую кнопку встроенной справки
    control(window, L"BUTTON", L"Рассчитать", BS_PUSHBUTTON | WS_TABSTOP, 28, 0, 336, 34, 1);	// Закрепляет расчет внизу панели параметров
    control(window, L"BUTTON", L"Остановить", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 260, 30, 7);	// Создает доступную во время воспроизведения остановку
    const HWND tabs = control(window, WC_TABCONTROLW, L"", WS_TABSTOP, 24, 92, 278, 24, tabsId);	// Настраивает отображение текущего элемента
    const wchar_t* titles[]{L"Геометрия", L"Материалы", L"Условия"};	// Описывает параметры текущего действия
    for (int i = 0; i < 3; ++i) { TCITEMW item{}; item.mask = TCIF_TEXT; item.pszText = const_cast<wchar_t*>(titles[i]); TabCtrl_InsertItem(tabs, i, &item); }	// Описывает параметры текущего действия
    groupControl(window, state, 0, L"STATIC", L"Диаметры, мм", 28, 135, 125, 20);	// Описывает параметры текущего действия
    groupControl(window, state, 0, L"STATIC", L"Длины, мм", 168, 135, 125, 20);	// Описывает параметры текущего действия
    for (int i = 0; i < 5; ++i) {	// Нумерует пары полей согласно схеме
        const int y = 161 + i * 30; wchar_t name[12]{};	// Определяет строку соответствующего отделения
        std::swprintf(name, std::size(name), L"D%d", i + 1); groupControl(window, state, 0, L"STATIC", name, 28, y + 4, 25, 20);	// Описывает параметры текущего действия
        groupControl(window, state, 0, L"EDIT", L"", 58, y, 78, 24, firstNumber + i * 2);	// Описывает параметры текущего действия
        std::swprintf(name, std::size(name), L"L%d", i + 1); groupControl(window, state, 0, L"STATIC", name, 168, y + 4, 25, 20);	// Описывает параметры текущего действия
        groupControl(window, state, 0, L"EDIT", L"", 202, y, 90, 24, firstNumber + i * 2 + 1);	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    const wchar_t* symbols[]{L"SK", L"SC", L"SOP", L"SFS", L"SOS", L"DS", L"D0", L"L0"};	// Определяет обозначения размеров в заданном порядке
    const wchar_t* walls[]{L"Кокиль, мм", L"Слой краски, мм", L"Опока, мм", L"Стенка формы, мм", L"Стенка опоки, мм", L"Диаметр стояка, мм", L"Общий диаметр, мм", L"Общая длина, мм"};	// Описывает параметры текущего действия
    for (int i = 0; i < 8; ++i) {	// Описывает параметры текущего действия
        const int y = 330 + i * 29;	// Описывает параметры текущего действия
        groupControl(window, state, 0, L"STATIC", symbols[i], 28, y + 4, 32, 20);	// Описывает параметры текущего действия
        const HWND field = control(window, L"EDIT", L"", i < 6 ? ES_AUTOHSCROLL | WS_TABSTOP : ES_READONLY, 62, y, 65, 24, i < 6 ? firstNumber + 10 + i : 210 + i - 6);	// Описывает параметры текущего действия
        state.groups[0].push_back(field);	// Связывает поле с вкладкой геометрии
        groupControl(window, state, 0, L"STATIC", walls[i], 134, y + 4, 166, 22);	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    const wchar_t* settings[]{L"Общее время, мин", L"Шаг по времени, с", L"Шаг по диаметру, мм"};	// Оставляет действующие настройки численной модели
    for (int i = 0; i < 3; ++i) {	// Рисует три обозначения вне белой области
        groupControl(window, state, 2, L"STATIC", settings[i], 28, (i == 0 ? 143 : 249 + (i - 1) * 34), 155, 24);	// Настраивает отображение текущего элемента
        groupControl(window, state, 2, L"EDIT", L"", 182, (i == 0 ? 139 : 245 + (i - 1) * 34), i == 0 ? 65 : 105, 24, firstNumber + 16 + i);	// Настраивает отображение текущего элемента
    }	// Завершает текущий блок обработки
    state.groups[2].push_back(control(window, L"BUTTON", L"Авто", BS_PUSHBUTTON | WS_TABSTOP, 251, 139, 46, 24, 8));	// Возвращает автоматический поиск одним нажатием
    groupControl(window, state, 2, L"STATIC", L"При выборе «Авто» расчет\nостановится в момент полного\nзатвердевания всех участков", 28, 172, 268, 63);	// Настраивает отображение текущего элемента
    int row = 134;	// Размещает компактные группы свойств вертикально
    const auto heading = [&](const wchar_t* name) { groupControl(window, state, 1, L"STATIC", name, 28, row, 260, 20); row += 23; };	// Создает название очередного материала
    const auto field = [&](const wchar_t* name, int number) { groupControl(window, state, 1, L"STATIC", name, 28, row + 3, 185, 20); groupControl(window, state, 1, L"EDIT", L"", 215, row, 78, 22, firstNumber + number); row += 23; };	// Связывает подпись с соответствующим параметром
    heading(L"Металл");	// Описывает параметры текущего действия
    field(L"Теплота кр., Дж/кг", 31); field(L"Т ликвидус, град C", 32); field(L"Т солидус, град C", 33); field(L"Т заливки, град C", 34);	// Описывает параметры текущего действия
    row += 10; heading(L"Краска");	// Описывает параметры текущего действия
    field(L"Т-провод., Вт/(м·К)", 24); field(L"Т-емкость, Дж/(кг·К)", 23); field(L"Т начальная, град C", 38); field(L"Плотность, кг/м3", 22);	// Описывает параметры текущего действия
    row += 10; heading(L"Кокиль и опока");	// Описывает параметры текущего действия
    field(L"Т ликвидус, град C", 41); field(L"Т солидус, град C", 42); field(L"Т начальная, град C", 39); field(L"Т окр. ср., град C", 36); field(L"Теплоотд., Вт/(м2·К)", 37);	// Описывает параметры текущего действия
    row += 10; heading(L"Форма");	// Описывает параметры текущего действия
    field(L"Т-провод., Вт/(м·К)", 30); field(L"Т-емкость, Дж/(кг·К)", 29); field(L"Т начальная, град C", 40); field(L"Плотность, кг/м3", 28);	// Описывает параметры текущего действия
    control(window, TRACKBAR_CLASSW, L"", TBS_NOTICKS | WS_TABSTOP, 0, 0, 100, 25, sliderId);	// Создает общую круглую временную ручку
    SendDlgItemMessageW(window, sliderId, TBM_SETRANGE, TRUE, MAKELPARAM(0, timelineEnd));	// Сохраняет разрешение общей шкалы
    control(window, L"STATIC", L"00:00:00", 0, 0, 0, 85, 22, clockId);	// Оставляет нулевое время до расчета
    control(window, L"STATIC", L"Участок", 0, 0, 0, 160, 22, 208);	// Настраивает отображение текущего элемента
    const HWND picker = control(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, 0, 250, 180, chartPicker);	// Предлагает только одно отделение за раз
    for (const auto* name : areaNumbers) { SendMessageW(picker, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name)); }	// Настраивает отображение текущего элемента
    SendMessageW(picker, CB_SETCURSEL, 0, 0);	// Выбирает прибыль по умолчанию
    control(window, L"BUTTON", L"Выгрузить", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 105, 28, firstExport);	// Возвращает стандартную текстовую кнопку выгрузки
    control(window, L"STATIC", L"", 0, 0, 0, 80, 22, 209);	// Показывает правую границу только числового горизонта
    control(window, L"BUTTON", L"-", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 25, 25, 301);	// Уменьшает масштаб вертикальной оси
    control(window, L"EDIT", L"100%", ES_CENTER | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 55, 25, 302);	// Позволяет ввести точный масштаб объема
    control(window, L"BUTTON", L"+", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 25, 25, 303);	// Увеличивает масштаб вертикальной оси
    control(window, L"STATIC", L"Показатель", 0, 0, 0, 80, 22, 306);	// Подписывает общий выбор объема или толщины
    const HWND metric = control(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 130, 100, 304);	// Создает переключатель показателя готовых данных
    for (const auto* name : {L"Объем, м3", L"Толщина, мм"}) { SendMessageW(metric, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name)); }	// Предлагает две согласованные системы единиц
    SendMessageW(metric, CB_SETCURSEL, 0, 0);	// Начинает с объемов слоев
    control(window, L"STATIC", L"Слой", 0, 0, 0, 40, 22, 307);	// Подписывает независимый выбор кривых
    control(window, L"BUTTON", L"Все ▾", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 100, 24, 305);	// Открывает список с несколькими отмеченными слоями
    showTab(state, 0); writeProject(window, state, state.inputs);	// Заполняет исходные поля без запуска вычислений
}	// Завершает текущий блок обработки
std::size_t frameIndex(const AppState& state) {	// Сопоставляет ползунок кадру охлаждения
    if (!state.result || state.position <= fillingEnd) { return 0; }	// Сохраняет начальную температуру во время заливки
    const auto count = state.result->cooling.section(0).samples.size();	// Читает количество согласованных кадров
    return static_cast<std::size_t>((state.position - fillingEnd) * static_cast<double>(count - 1) / (timelineEnd - fillingEnd));	// Выбирает ближайший предшествующий кадр
}	// Завершает текущий блок обработки
double fillProgress(const AppState& state) {	// Определяет степень заполнения формы
    return std::min(1.0, state.position / static_cast<double>(fillingEnd));	// Отделяет заливку от охлаждения
}	// Завершает текущий блок обработки
void arrange(HWND window, AppState& state) {	// Размещает постоянно видимые элементы управления
    const auto areas = layout(window, state.started);	// Описывает параметры текущего действия
    MoveWindow(GetDlgItem(window, 1), 28, areas.client.bottom - 92, 270, 30, TRUE);	// Описывает параметры текущего действия
    MoveWindow(GetDlgItem(window, 7), 28, areas.client.bottom - 56, 270, 30, TRUE);	// Ставит остановку под запуском
    EnableWindow(GetDlgItem(window, 7), state.started && state.position < timelineEnd);	// Разрешает остановку и продолжение до конца истории
    SetDlgItemTextW(window, 7, state.started && !state.automatic && state.position < timelineEnd ? L"Продолжить расчет" : L"Остановить");	// Меняет действие после приостановки
    for (int id : {sliderId, clockId, 209}) { ShowWindow(GetDlgItem(window, id), state.started ? SW_SHOW : SW_HIDE); }	// Скрывает шкалу до первого расчета
    MoveWindow(GetDlgItem(window, 6), areas.client.right - 146, 10, 130, 31, TRUE);	// Выравнивает справку по правому краю окна
    MoveWindow(GetDlgItem(window, sliderId), areas.mold.left + 88, areas.client.bottom - 78, areas.mold.right - areas.mold.left - (state.inputs.autoTime ? 92 : 165), 26, TRUE);	// Описывает параметры текущего действия
    MoveWindow(GetDlgItem(window, clockId), areas.mold.left + 4, areas.client.bottom - 75, 85, 22, TRUE);	// Ставит время слева от круглой ручки
    MoveWindow(GetDlgItem(window, 209), areas.mold.right - 76, areas.client.bottom - 75, 76, 22, TRUE);	// Ставит конечное время справа от шкалы
    ShowWindow(GetDlgItem(window, 209), state.started && !state.inputs.autoTime ? SW_SHOW : SW_HIDE);	// Убирает конечную метку автоматического режима
    wchar_t endTime[32]{}; const auto endSeconds = static_cast<long long>(state.inputs.hours * 3600);	// Переводит заданный горизонт в секунды
    std::swprintf(endTime, std::size(endTime), L"%02lld:%02lld:%02lld", endSeconds / 3600, endSeconds / 60 % 60, endSeconds % 60);	// Форматирует правую метку времени
    SetDlgItemTextW(window, 209, endTime);	// Показывает горизонт без условного времени заливки
    const auto card = chartAreas(areas.results, state.selection)[state.selection];	// Находит карточку единственного выбранного графика
    MoveWindow(GetDlgItem(window, 306), areas.results.left + 12, areas.results.top + 13, 76, 22, TRUE);	// Ставит показатель над таблицей слева
    MoveWindow(GetDlgItem(window, 304), areas.results.left + 88, areas.results.top + 8, 125, 100, TRUE);	// Сохраняет место для правой метки времени
    MoveWindow(GetDlgItem(window, 208), card.left + 12, card.top + 15, 52, 22, TRUE);	// Подписывает участок в общей строке управления
    MoveWindow(GetDlgItem(window, chartPicker), card.left + 65, card.top + 10, 55, 180, TRUE);	// Показывает только номер участка
    MoveWindow(GetDlgItem(window, 307), card.right - 399, card.top + 15, 35, 22, TRUE);	// Подписывает раскрывающийся выбор слоев
    MoveWindow(GetDlgItem(window, 305), card.right - 358, card.top + 10, 104, 26, TRUE);	// Приближает выбор слоев к масштабу
    MoveWindow(GetDlgItem(window, firstExport), card.right - 116, card.top + 10, 105, 26, TRUE);	// Выравнивает выгрузку с остальными элементами
    MoveWindow(GetDlgItem(window, 301), card.right - 247, card.top + 10, 25, 26, TRUE);	// Приближает масштаб к кнопке выгрузки
    MoveWindow(GetDlgItem(window, 302), card.right - 218, card.top + 10, 55, 26, TRUE);	// Сохраняет ввод точного процента масштаба
    MoveWindow(GetDlgItem(window, 303), card.right - 159, card.top + 10, 25, 26, TRUE);	// Ставит увеличение справа от поля
    EnableWindow(GetDlgItem(window, firstExport), state.result && !state.running && !state.edited);	// Запрещает выгрузку отсутствующего или устаревшего результата
    EnableWindow(GetDlgItem(window, sliderId), state.result || state.running);	// Оставляет шкалу неподвижной до команды расчета
    EnableWindow(GetDlgItem(window, 4), state.result && !state.running && !state.edited);	// Разрешает CSV только для актуальной истории
    EnableWindow(GetDlgItem(window, 1), TRUE);	// Позволяет начать новое задание после отмены прежнего
    SetDlgItemTextW(window, 1, state.started ? L"Рассчитать заново" : L"Рассчитать");	// Различает первый запуск и повторное вычисление
}	// Завершает текущий блок обработки
void updateTime(HWND window, AppState& state) {	// Обновляет выбранный момент без пересчета
    double seconds = 0;	// Не включает иллюстративную заливку в расчетное время
    if (state.result && state.position > fillingEnd && !state.running) { seconds = state.result->cooling.section(0).samples[frameIndex(state)].seconds; }	// Описывает параметры текущего действия
    const auto whole = static_cast<unsigned long long>(std::llround(seconds));	// Округляет время до целых секунд
    wchar_t caption[40]{};	// Резервирует строку часов
    std::swprintf(caption, std::size(caption), L"%02llu:%02llu:%02llu", whole / 3600, whole / 60 % 60, whole % 60);	// Форматирует часы минуты секунды
    SetDlgItemTextW(window, clockId, caption);	// Показывает время слева от шкалы
    InvalidateRect(GetDlgItem(window, sliderId), nullptr, TRUE);	// Очищает прежнее положение круглого маркера
    SendDlgItemMessageW(window, sliderId, TBM_SETPOS, TRUE, state.position);	// Синхронизирует ручку шкалы
    const auto areas = layout(window, state.started);	// Описывает параметры текущего действия
    InvalidateRect(window, &areas.mold, FALSE);	// Перерисовывает состояние металла
    InvalidateRect(window, &areas.results, FALSE);	// Обновляет таблицу и указатели времени
}	// Завершает текущий блок обработки
void calculate(HWND window, AppState& state, const Project& project) {	// Запускает проверенный проект в фоновом потоке
    validateProject(project);	// Проверяет входы до отмены предыдущей работы
    state.worker.request_stop();	// Просит прежнее задание остановиться
    if (state.worker.joinable()) { state.worker.join(); }	// Дожидается безопасного завершения прежнего задания
    const auto generation = ++state.generation;	// Назначает номер нового задания
    state.inputs = project; state.running = true; state.edited = false;	// Фиксирует исходные данные текущего задания
    state.position = 0; state.automatic = true; state.started = true; state.preview = project; state.rebuildMold = true;	// Описывает параметры текущего действия
    state.curves = {};	// Сбрасывает линии прежнего результата
    arrange(window, state); InvalidateRect(window, nullptr, FALSE); updateTime(window, state);	// Описывает параметры текущего действия
    SetTimer(window, 1, 50, nullptr);	// Запускает плавное воспроизведение заливки
    state.worker = std::jthread([window, project, generation](std::stop_token stop) {	// Настраивает отображение текущего элемента
        auto completed = std::make_unique<Completed>();	// Создает сообщение для оконного потока
        completed->generation = generation;	// Отмечает принадлежность результата заданию
        try {	// Защищает оконный поток от расчетных исключений
            completed->result = std::make_shared<Run>();	// Выделяет независимый текущий результат
            completed->result->project = project;	// Сохраняет входы вместе с историей
            completed->result->cooling.calculate(project, stop);	// Вычисляет пять отделений вне интерфейса
        } catch (const std::exception& error) { completed->error = wide(error.what()); }	// Передает причину неудачного расчета
        if (!stop.stop_requested() && PostMessageW(window, calculationDone, 0, reinterpret_cast<LPARAM>(completed.get()))) { completed.release(); }	// Передает владение только доставляемому сообщению
    });	// Завершает создание фонового задания
}	// Завершает текущий блок обработки
std::filesystem::path chooseFile(HWND window, const AppState& state, const wchar_t* filter, const wchar_t* extension, bool save) {	// Показывает системный файловый диалог
    wchar_t filename[32768]{};	// Поддерживает длинные пути и русские имена
    OPENFILENAMEW dialog{};	// Обнуляет системную структуру диалога
    dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = window;	// Привязывает диалог к главному окну
    dialog.lpstrFilter = filter; dialog.lpstrDefExt = extension;	// Задает тип сохраняемого документа
    dialog.lpstrFile = filename; dialog.nMaxFile = std::size(filename);	// Предоставляет буфер полного пути
    dialog.lpstrInitialDir = state.directory.c_str();	// Начинает просмотр с папки программы
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);	// Проверяет путь и подтверждает перезапись
    const bool accepted = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);	// Выполняет требуемый файловый диалог
    if (!accepted && CommDlgExtendedError()) { throw std::runtime_error("Не удалось открыть файловый диалог"); }	// Отличает ошибку от отмены
    return accepted ? std::filesystem::path(filename) : std::filesystem::path{};	// Возвращает выбранный путь или отмену
}	// Завершает текущий блок обработки
bool current(HWND window, AppState& state) {	// Запрещает экспорт устаревшего расчета
    if (state.running || !state.result || state.edited || !(readProject(window, state) == state.result->project)) {	// Проверяет соответствие истории редактору
        throw std::runtime_error("Сначала нажмите «Рассчитать» и дождитесь завершения");	// Объясняет необходимость актуального расчета
        return false;	// Отклоняет действие над устаревшими данными
    }	// Завершает текущий блок обработки
    return true;	// Разрешает действие над текущим результатом
}	// Завершает текущий блок обработки
bool discard(HWND window, const AppState& state) {	// Защищает несохраненные изменения проекта
    return !state.unsaved || MessageBoxW(window, L"Есть несохраненные изменения. Продолжить без их сохранения?", L"RollCast", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;	// Запрашивает отказ от введенных изменений
}	// Завершает текущий блок обработки
void exportChart(HWND window, AppState& state, int section) {	// Сохраняет выбранный график отдельным изображением
    if (!current(window, state)) { return; }	// Требует завершенный актуальный результат
    const auto path = chooseFile(window, state, L"Изображение PNG\0*.png\0\0", L"png", true);	// Предлагает путь изображения
    if (path.empty()) { return; }	// Учитывает отмену диалога
    const HDC target = GetDC(window);	// Получает образец экранного контекста
    Surface image;	// Управляет буфером экспортируемого изображения
    try { image.ensure(target, 1100, 760); } catch (...) { ReleaseDC(window, target); throw; }	// Освобождает контекст при ошибке выделения
    ReleaseDC(window, target);	// Возвращает экранный контекст системе
    const RECT area{0, 0, 1100, 760};	// Задает независимое разрешение изображения
    FillRect(image.dc, &area, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));	// Подготавливает белый фон
    SelectObject(image.dc, GetStockObject(DEFAULT_GUI_FONT));	// Сохраняет читаемый системный шрифт
    CurveCache cache;	// Создает отдельный масштаб экспортируемых линий
    drawSingleChart(image.dc, area, state.result->project, state.result.get(), section, frameIndex(state), cache, state.zoom, state.metric, state.layers);	// Настраивает отображение текущего элемента
    const CLSID png{0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};	// Выбирает встроенный кодировщик PNG
    Gdiplus::Bitmap bitmap(image.bitmap, nullptr);	// Оборачивает готовый растр для кодировщика
    if (bitmap.Save(path.c_str(), &png, nullptr) != Gdiplus::Ok) { throw std::runtime_error("Не удалось сохранить PNG"); }	// Проверяет успешность записи изображения
}	// Завершает текущий блок обработки
void action(HWND window, AppState& state, int id) {	// Выполняет команды верхней и левой панелей
    if (id == 1) { calculate(window, state, readProject(window, state)); }	// Запускает расчет только по явной команде
    if (id == 2 && discard(window, state)) {	// Загружает другой проект с защитой изменений
        const auto path = chooseFile(window, state, L"Проект RollCast\0*.roll\0\0", L"roll", false);	// Предлагает системный диалог открытия
        if (!path.empty()) {	// Учитывает отмену системного диалога
            const auto p = loadProject(path);	// Проверяет файл до изменения редактора
            state.worker.request_stop(); if (state.worker.joinable()) { state.worker.join(); }	// Отменяет прежнее вычисление перед заменой входов
            ++state.generation; state.running = false; state.automatic = false; KillTimer(window, 1);	// Исключает публикацию отмененного задания
            writeProject(window, state, p); state.result.reset(); state.curves = {};	// Загружает поля и очищает прежние результаты
            state.filename = path; state.unsaved = false; state.edited = false;	// Запоминает выбранный документ
            state.position = 0; state.started = false; state.rebuildMold = true;	// Описывает параметры текущего действия
            arrange(window, state); InvalidateRect(window, nullptr, FALSE); updateTime(window, state);	// Описывает параметры текущего действия
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    if (id == 3) {	// Сохраняет текущие входы без автоматического расчета
        const auto p = readProject(window, state);	// Проверяет обе вкладки редактора
        const auto path = chooseFile(window, state, L"Проект RollCast\0*.roll\0\0", L"roll", true);	// Получает название от пользователя при сохранении
        if (!path.empty()) {	// Учитывает отмену системного диалога
            saveProject(path, p, !state.running && state.result && state.result->project == p ? state.result.get() : nullptr);	// Добавляет сводку только соответствующего результата
            state.filename = path; state.unsaved = false;	// Отмечает успешно сохраненные входные данные
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    if (id == 4 && current(window, state)) {	// Сохраняет полную историю пяти отделений
        const auto path = chooseFile(window, state, L"Таблица CSV\0*.csv\0\0", L"csv", true);	// Предлагает пользовательское название таблицы
        if (!path.empty()) { exportHistory(path, *state.result); }	// Записывает все рассчитанные кадры
    }	// Завершает текущий блок обработки
    if (id == 6) { showAbout(window); }	// Открывает теорию и концепцию поверх рабочего окна
    if (id == 7 && state.started && state.position < timelineEnd) {	// Возобновляет тот же расчет с сохраненного момента
        state.automatic = !state.automatic;	// Переключает остановку и продолжение без сброса данных
        if (state.automatic) { SetTimer(window, 1, 50, nullptr); } else { KillTimer(window, 1); }	// Управляет лишь воспроизведением готовой истории
        arrange(window, state);	// Согласует подписи доступных кнопок
    }	// Завершает текущий блок обработки
    if (id == 305) {	// Открывает отмечаемый список слоев графика
        const HMENU menu = CreatePopupMenu();	// Создает штатное раскрывающееся меню
        AppendMenuW(menu, MF_STRING | (state.layers == 7 ? MF_CHECKED : 0), 400, L"Все");	// Отмечает одновременный выбор трех слоев
        for (unsigned z = 0; z < 3; ++z) { AppendMenuW(menu, MF_STRING | ((state.layers & (1u << z)) ? MF_CHECKED : 0), 401 + z, layerNames[z]); }	// Сохраняет независимые отметки каждого слоя
        RECT anchor{}; GetWindowRect(GetDlgItem(window, 305), &anchor);	// Привязывает список к раскрывающей кнопке
        const int selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, anchor.left, anchor.bottom, 0, window, nullptr);	// Получает действие без изменения расчетного состояния
        DestroyMenu(menu);	// Освобождает временный список
        if (selected == 400) { state.layers = 7; }	// Выбирает сразу все кривые
        else if (selected >= 401 && selected <= 403) { state.layers ^= 1u << (selected - 401); }	// Переключает одну отметку сохраняя остальные
        std::wstring caption = state.layers == 7 ? L"Все" : state.layers == 0 ? L"Не выбрано" : state.layers == 1 ? L"Твердый" : state.layers == 2 ? L"Двухфазный" : state.layers == 4 ? L"Жидкий" : L"Два слоя";	// Отражает текущий набор в закрытом списке
        SetDlgItemTextW(window, 305, (caption + L" ▾").c_str());	// Показывает выбранный набор без сброса момента
        const auto area = layout(window).results; InvalidateRect(window, &area, FALSE);	// Перерисовывает только представление результата
    }	// Завершает текущий блок обработки
    if (id == 8) { SetDlgItemTextW(window, firstNumber + 16, L"Авто"); }	// Возвращает автоматический горизонт
    if (id == 301 || id == 303) {	// Меняет масштаб графика по кнопкам
        state.zoom = std::clamp(state.zoom + (id == 303 ? 10 : -10), 10, 1000);	// Ограничивает выбранное увеличение
        const auto value = std::to_wstring(state.zoom) + L"%"; SetDlgItemTextW(window, 302, value.c_str());	// Обновляет поле масштаба
    }	// Завершает текущий блок обработки
    if (id == firstExport) { exportChart(window, state, state.selection); }	// Выгружает единственное выбранное отделение
}	// Завершает текущий блок обработки
void paint(HWND window, AppState& state) {	// Рисует только запрошенную часть рабочего экрана
    PAINTSTRUCT painting{}; const HDC target = BeginPaint(window, &painting);	// Получает область требуемого обновления
    const auto areas = layout(window, state.started);	// Описывает параметры текущего действия
    state.buffer.ensure(target, areas.client.right, areas.client.bottom);	// Переиспользует растровый буфер до изменения окна
    const HDC dc = state.buffer.dc; const int saved = SaveDC(dc);	// Сохраняет настройки повторного контекста
    SelectClipRgn(dc, nullptr);	// Восстанавливает полную область внутреннего буфера
    FillRect(dc, &painting.rcPaint, GetSysColorBrush(COLOR_BTNFACE));	// Очищает только изменившуюся часть
    SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));	// Выбирает системный шрифт интерфейса
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));	// Настраивает читаемые подписи
    const wchar_t* headings[]{L"Параметры", L"Форма для заливки валка", L"Кристаллизация металла"};	// Называет три постоянно видимые панели
    const RECT panels[]{areas.parameters, areas.mold, areas.results};	// Сопоставляет заголовки панелям
    RECT intersection{};	// Принимает пересечение с обновляемой областью
    for (int i = 0; i < 3; ++i) {	// Рисует три обозначения вне белой области
        TextOutW(dc, panels[i].left, 56, headings[i], static_cast<int>(std::wcslen(headings[i])));	// Рисует заголовок над линией рамки
        if (IntersectRect(&intersection, &painting.rcPaint, &panels[i])) {	// Проверяет необходимость обновления панели
            FillRect(dc, &panels[i], GetSysColorBrush(i == 1 ? COLOR_WINDOW : COLOR_BTNFACE));	// Сохраняет белый фон только под схемой
            FrameRect(dc, &panels[i], GetSysColorBrush(COLOR_3DSHADOW));	// Отделяет панель тонкой рамкой
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    if (IntersectRect(&intersection, &painting.rcPaint, &areas.mold)) {	// Обновляет форму отдельно от графиков
        MoldFrame frame; frame.filling = fillProgress(state);	// Задает текущую стадию заполнения
        frame.filled = filledSections(state.preview, frame.filling); frame.phase = state.preview.phase;	// Описывает параметры текущего действия
        if (state.result && !state.running) {	// Подключает готовые температуры после расчета
            for (std::size_t i = 0; i < sectionCount; ++i) { frame.samples[i] = &state.result->cooling.section(i).samples[frameIndex(state)]; }	// Передает профили выбранного кадра
        }	// Завершает текущий блок обработки
        drawMold(dc, areas.mold, state.preview, state.hits, state.hovered, state.cursor, frame, state.rebuildMold);	// Описывает параметры текущего действия
        state.rebuildMold = false;	// Сохраняет геометрические области для следующего кадра
        drawMoldLabel(dc, areas.mold, state.hovered, state.cursor);	// Показывает название выбранной детали
    }	// Завершает текущий блок обработки
    if (IntersectRect(&intersection, &painting.rcPaint, &areas.results)) {	// Пропускает графики при обычном наведении
        drawMoldDashboard(dc, areas.results, state.preview, !state.running ? state.result.get() : nullptr, frameIndex(state), fillProgress(state), state.selection, state.curves, state.zoom, state.metric, state.layers);	// Настраивает отображение текущего элемента
    }	// Завершает текущий блок обработки
    const wchar_t* legend[]{L"Жидкий", L"Двухфазный", L"Твердый"};	// Настраивает отображение текущего элемента
    const COLORREF colors[]{RGB(228, 76, 57), RGB(242, 177, 46), RGB(45, 112, 181)};	// Настраивает отображение текущего элемента
    for (int i = 0; state.started && i < 3; ++i) {	// Описывает параметры текущего действия
        const int x = areas.mold.left + 3 + i * (areas.mold.right - areas.mold.left) / 3;	// Распределяет обозначения под формой
        const int y = areas.client.bottom - 33;	// Ставит легенду ниже временной шкалы
        const RECT square{x, y + 3, x + 11, y + 14};	// Задает цветной образец
        const HBRUSH brush = CreateSolidBrush(colors[i]); FillRect(dc, &square, brush); DeleteObject(brush);	// Рисует и освобождает образец цвета
        TextOutW(dc, x + 15, y, legend[i], static_cast<int>(std::wcslen(legend[i])));	// Подписывает состояние металла
    }	// Завершает текущий блок обработки
    const HPEN separator = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));	// Создает тонкие разделители панелей
    const auto oldSeparator = SelectObject(dc, separator);	// Сохраняет исходное перо
    MoveToEx(dc, 16, 48, nullptr); LineTo(dc, areas.client.right - 16, 48);	// Отделяет основную панель действий
    if (state.tab == 1) { for (int y : {251, 376, 524}) { MoveToEx(dc, 28, y, nullptr); LineTo(dc, 296, y); } }	// Разделяет группы свойств материалов
    SelectObject(dc, oldSeparator); DeleteObject(separator);	// Освобождает перо разделителей
    BitBlt(target, painting.rcPaint.left, painting.rcPaint.top, painting.rcPaint.right - painting.rcPaint.left, painting.rcPaint.bottom - painting.rcPaint.top, dc, painting.rcPaint.left, painting.rcPaint.top, SRCCOPY);	// Переносит только обновленную область
    RestoreDC(dc, saved); EndPaint(window, &painting);	// Завершает кадр и восстанавливает контекст
}	// Завершает текущий блок обработки
LRESULT sliderDraw(NMCUSTOMDRAW* draw) {	// Рисует круглую ручку на серой линии
    if (draw->dwDrawStage == CDDS_PREPAINT) { return CDRF_NOTIFYITEMDRAW; }	// Запрашивает рисование частей ползунка
    if (draw->dwDrawStage != CDDS_ITEMPREPAINT) { return CDRF_DODEFAULT; }	// Сохраняет стандартную обработку прочих стадий
    RECT area = draw->rc;	// Получает область отдельной части
    if (draw->dwItemSpec == TBCD_CHANNEL) {	// Заменяет канал простой серой линией
        area.top = (area.top + area.bottom) / 2 - 2; area.bottom = area.top + 4;	// Задает толщину линии шкалы
        FillRect(draw->hdc, &area, GetSysColorBrush(COLOR_3DSHADOW));	// Рисует нейтральную серую линию
        return CDRF_SKIPDEFAULT;	// Подавляет стандартную прямоугольную ручку
    }	// Завершает текущий блок обработки
    if (draw->dwItemSpec == TBCD_THUMB) {	// Заменяет ручку круглым маркером
        const int x = (area.left + area.right) / 2; const int y = (area.top + area.bottom) / 2;	// Находит центр ручки
        const HBRUSH brush = CreateSolidBrush(RGB(45, 112, 181));	// Выбирает цвет активного маркера
        const auto oldBrush = SelectObject(draw->hdc, brush); const auto oldPen = SelectObject(draw->hdc, GetStockObject(NULL_PEN));	// Сохраняет исходные объекты
        const int radius = std::max<int>(2, std::min(area.right - area.left, area.bottom - area.top) / 2 - 1);	// Ограничивает маркер областью системной ручки
        Ellipse(draw->hdc, x - radius, y - radius, x + radius + 1, y + radius + 1);	// Рисует круглую ручку шкалы
        SelectObject(draw->hdc, oldBrush); SelectObject(draw->hdc, oldPen); DeleteObject(brush);	// Восстанавливает объекты и освобождает кисть
        return CDRF_SKIPDEFAULT;	// Подавляет стандартную прямоугольную ручку
    }	// Завершает текущий блок обработки
    return CDRF_DODEFAULT;	// Оставляет остальную обработку системе
}	// Завершает текущий блок обработки
LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {	// Обрабатывает действия пользователя главного окна
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));	// Получает принадлежащее окну состояние
    if (message == WM_NCCREATE) {	// Связывает состояние до создания дочерних элементов
        state = static_cast<AppState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);	// Извлекает переданный экземпляр приложения
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));	// Запоминает состояние на время жизни окна
    }	// Завершает текущий блок обработки
    if (!state) { return DefWindowProcW(window, message, wParam, lParam); }	// Передает ранние сообщения стандартному обработчику
    try {	// Обрабатывает операцию с контролем возможных ошибок
        switch (message) {	// Выбирает обработчик оконного сообщения
        case WM_CREATE:	// Создает рабочий экран и запускает первый расчет
            createControls(window, *state); arrange(window, *state);	// Создает и размещает элементы управления
            return 0;	// Возвращает нулевое значение
        case WM_GETMINMAXINFO:	// Сохраняет видимость всех трех панелей
            reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {1240, 800}; return 0;	// Ограничивает минимальный размер рабочего окна
        case WM_SIZE:	// Перестраивает расположение после изменения размеров
            state->rebuildMold = true; state->curves = {};	// Сбрасывает зависящие от размеров данные
            arrange(window, *state); InvalidateRect(window, nullptr, FALSE); return 0;	// Обновляет расположение и полный кадр
        case WM_ERASEBKGND: return 1;	// Подавляет мерцание при двойной буферизации
        case WM_PAINT: paint(window, *state); return 0;	// Обновляет только поврежденные области окна
        case WM_COMMAND: {	// Обрабатывает редактор список и панель действий
            const int id = LOWORD(wParam); const int code = HIWORD(wParam);	// Разбирает источник пользовательского действия
            if (!state->changing && code == EN_CHANGE && id >= firstNumber && id < firstNumber + numberCount) {	// Отмечает только пользовательское редактирование входов
                state->unsaved = true; state->edited = true;	// Различает редактор и рассчитанную историю
                state->worker.request_stop(); ++state->generation; state->running = false;	// Отклоняет незавершенный результат прежних входных данных
                updateDerived(window, *state); arrange(window, *state);	// Обновляет габариты без тепловых вычислений
                state->automatic = false; KillTimer(window, 1); state->result.reset(); state->started = false; state->position = 0;	// Снимает результат при изменении входных параметров
                arrange(window, *state); InvalidateRect(window, nullptr, FALSE);	// Немедленно обновляет геометрию предварительного просмотра
            } else if (id == 302 && code == EN_CHANGE && !state->changing) {	// Применяет ручной масштаб без изменения координат карточки
                const int value = _wtoi(text(GetDlgItem(window, 302)).c_str());	// Разрешает завершающий знак процента
                if (value >= 10 && value <= 1000) { state->zoom = value; state->curves = {}; const auto area = layout(window).results; InvalidateRect(window, &area, FALSE); }	// Ограничивает масштаб разумным диапазоном
            } else if (id == 304 && code == CBN_SELCHANGE) {	// Меняет показатель уже рассчитанной истории
                state->metric = SendDlgItemMessageW(window, 304, CB_GETCURSEL, 0, 0) == 1 ? LayerMetric::thickness : LayerMetric::volume;	// Выбирает толщину либо объем
                state->curves = {}; const auto area = layout(window).results; InvalidateRect(window, &area, FALSE);	// Не меняет время выбранный участок и состояние паузы
            } else if (id == chartPicker && code == CBN_SELCHANGE) {	// Меняет набор показываемых графиков
                state->selection = static_cast<int>(SendDlgItemMessageW(window, chartPicker, CB_GETCURSEL, 0, 0));	// Запоминает выбранное отделение
                arrange(window, *state); const auto area = layout(window).results; InvalidateRect(window, &area, FALSE);	// Перерисовывает только панель результатов
            } else if (code == BN_CLICKED && ((id >= 1 && id <= 8) || (id >= 300 && id <= 305))) { action(window, *state, id); }	// Настраивает отображение текущего элемента
            return 0;	// Возвращает нулевое значение
        }	// Завершает текущий блок обработки
        case WM_NOTIFY: {	// Обрабатывает вкладки и оформление шкалы
            const auto* notice = reinterpret_cast<NMHDR*>(lParam);	// Определяет источник уведомления
            if (notice->idFrom == tabsId && notice->code == TCN_SELCHANGE) { showTab(*state, TabCtrl_GetCurSel(notice->hwndFrom)); const auto area = layout(window).parameters; InvalidateRect(window, &area, FALSE); }	// Перерисовывает разделители новой вкладки
            if (notice->idFrom == sliderId && notice->code == NM_CUSTOMDRAW) { return sliderDraw(reinterpret_cast<NMCUSTOMDRAW*>(lParam)); }	// Рисует круглый маркер времени
            return 0;	// Возвращает нулевое значение
        }	// Завершает текущий блок обработки
        case WM_HSCROLL:	// Показывает ранее рассчитанные кадры без вычислений
            if (reinterpret_cast<HWND>(lParam) == GetDlgItem(window, sliderId)) {	// Проверяет источник смены положения
                state->automatic = false; KillTimer(window, 1); arrange(window, *state);	// Описывает параметры текущего действия
                state->position = static_cast<int>(SendDlgItemMessageW(window, sliderId, TBM_GETPOS, 0, 0));	// Получает положение круглой ручки
                if (state->running) { state->position = std::min(fillingEnd, state->position); }	// Ограничивает кадры до завершения расчета
                updateTime(window, *state);	// Показывает следующий кадр заливки
            }	// Завершает текущий блок обработки
            return 0;	// Возвращает нулевое значение
        case WM_TIMER:	// Воспроизводит заполнение формы после запуска расчета
            if (wParam == 1 && state->automatic) {	// Обрабатывает только таймер анимации
                state->position = std::min(state->result && !state->running ? timelineEnd : fillingEnd, state->position + 20);	// Описывает параметры текущего действия
                updateTime(window, *state);	// Показывает следующий кадр заливки
                if (state->position == timelineEnd) { state->automatic = false; KillTimer(window, 1); arrange(window, *state); }	// Описывает параметры текущего действия
            }	// Завершает текущий блок обработки
            return 0;	// Возвращает нулевое значение
        case calculationDone: {	// Принимает полностью готовую историю
            std::unique_ptr<Completed> completed(reinterpret_cast<Completed*>(lParam));	// Принимает владение сообщением потока
            if (completed->generation != state->generation) { return 0; }	// Отбрасывает отмененный результат
            state->running = false;	// Возвращает доступность экспортных действий
            if (completed->error.empty()) {	// Публикует только согласованный полный результат
                state->result = std::move(completed->result);	// Заменяет текущую историю одним действием
                double hottestIron = -273;	// Собирает максимальную температуру оснастки за расчет
                for (std::size_t i = 0; i < sectionCount; ++i) { hottestIron = std::max(hottestIron, state->result->cooling.section(i).maxIronTemperature); }	// Проверяет все кокили и опоки
                if (hottestIron >= state->inputs.ironSolidus) {	// Предупреждает о выходе за допущение твердой оснастки
                    wchar_t warning[320]{}; std::swprintf(warning, std::size(warning), L"За весь расчет максимум температуры оснастки: %.1f град C. Достигнут %ls. Плавление оснастки не рассчитывается; результат требует проверки.", hottestIron, hottestIron >= state->inputs.ironLiquidus ? L"ликвидус" : L"солидус");	// Различает два контрольных порога
                    state->automatic = false; KillTimer(window, 1); MessageBoxW(window, warning, L"Контроль нагрева оснастки", MB_OK | MB_ICONWARNING);	// Сохраняет результат и приостанавливает воспроизведение для проверки
                }	// Завершает текущий блок обработки
            } else { state->result.reset(); MessageBoxW(window, completed->error.c_str(), L"Ошибка расчета", MB_OK | MB_ICONERROR); }	// Объясняет неудачу без скрытых результатов
            state->curves = {};	// Описывает параметры текущего действия
            if (!completed->error.empty()) { state->automatic = false; state->started = false; KillTimer(window, 1); }	// Настраивает отображение текущего элемента
            arrange(window, *state); updateTime(window, *state);	// Описывает параметры текущего действия
            return 0;	// Возвращает нулевое значение
        }	// Завершает текущий блок обработки
        case WM_MOUSEMOVE: {	// Обновляет подсказку без перерисовки графиков
            const POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};	// Получает положение указателя
            const auto areas = layout(window, state->started);	// Описывает параметры текущего действия
            const MoldPart part = PtInRect(&areas.mold, cursor) ? state->hits.partAt(cursor) : MoldPart::none;	// Находит деталь под курсором
            if (part != state->hovered || (part != MoldPart::none && (std::abs(cursor.x - state->cursor.x) > 3 || std::abs(cursor.y - state->cursor.y) > 3))) {	// Ограничивает лишние обновления подписи
                state->hovered = part; state->cursor = cursor;	// Сохраняет новое выделение
                InvalidateRect(window, &areas.mold, FALSE);	// Обновляет исключительно схему формы
            }	// Завершает текущий блок обработки
            if (!state->tracking) { TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, window, 0}; TrackMouseEvent(&tracking); state->tracking = true; }	// Запрашивает уведомление об уходе мыши
            return 0;	// Возвращает нулевое значение
        }	// Завершает текущий блок обработки
        case WM_MOUSELEAVE: {	// Снимает выделение после ухода указателя
            state->hovered = MoldPart::none; state->tracking = false;	// Очищает состояние наведения
            const auto area = layout(window).mold; InvalidateRect(window, &area, FALSE); return 0;	// Обновляет только форму
        }	// Завершает текущий блок обработки
        case WM_DESTROY: {	// Завершает расчет перед закрытием приложения
            state->worker.request_stop(); if (state->worker.joinable()) { state->worker.join(); }	// Дожидается остановки фонового потока
            MSG pending{}; while (PeekMessageW(&pending, window, calculationDone, calculationDone, PM_REMOVE)) { delete reinterpret_cast<Completed*>(pending.lParam); }	// Освобождает недоставленные результаты
            KillTimer(window, 1); PostQuitMessage(0); return 0;	// Завершает таймер и оконный цикл
        }	// Завершает текущий блок обработки
        default: break;	// Передает прочие сообщения стандартной процедуре
        }	// Завершает текущий блок обработки
    } catch (const std::exception& error) { MessageBoxW(window, wide(error.what()).c_str(), L"RollCast", MB_OK | MB_ICONWARNING); }	// Показывает ошибку без аварийного завершения
    return DefWindowProcW(window, message, wParam, lParam);	// Выполняет стандартную обработку Windows
}	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {	// Запускает оконное приложение Windows
    Gdiplus::GdiplusStartupInput graphics; ULONG_PTR token = 0;	// Подготавливает системный кодировщик изображений
    if (Gdiplus::GdiplusStartup(&token, &graphics, nullptr) != Gdiplus::Ok) { return 1; }	// Проверяет доступность GDI+
    INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_WIN95_CLASSES};	// Выбирает системные классы элементов управления
    InitCommonControlsEx(&controls);	// Регистрирует вкладки список шкалу и подсказки
    WNDCLASSW windowClass{}; windowClass.lpfnWndProc = windowProcedure;	// Назначает процедуру главного окна
    windowClass.hInstance = instance; windowClass.lpszClassName = L"RollCastMainWindow";	// Задает класс окна приложения
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);	// Использует стандартный указатель
    windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);	// Задает серый фон рабочего экрана
    if (!RegisterClassW(&windowClass)) { Gdiplus::GdiplusShutdown(token); return 1; }	// Проверяет регистрацию главного окна
    AppState state; wchar_t executable[32768]{};	// Создает независимое состояние текущего запуска
    GetModuleFileNameW(nullptr, executable, std::size(executable));	// Получает путь исполняемого файла
    state.directory = std::filesystem::path(executable).parent_path();	// Выбирает исходную папку сохранения пользовательских файлов
    const HWND window = CreateWindowExW(0, windowClass.lpszClassName, L"RollCast", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900, nullptr, nullptr, instance, &state);	// Создает единый рабочий экран
    if (!window) { Gdiplus::GdiplusShutdown(token); return 1; }	// Проверяет успешность создания окна
    ShowWindow(window, SW_SHOWMAXIMIZED); UpdateWindow(window);	// Открывает окно на весь доступный экран
    MSG message{};	// Принимает сообщения интерфейса
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {	// Обрабатывает действия до закрытия окна
        if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN && (message.hwnd == window || GetClassLongPtrW(message.hwnd, GCW_ATOM) == 0x0081)) {	// Применяет параметры клавишей Enter в редакторе
            if (GetDlgCtrlID(message.hwnd) == 302) {	// Отделяет масштаб графика от запуска расчета
                const int zoom = _wtoi(text(message.hwnd).c_str());	// Читает введенный процент масштаба
                if (zoom >= 10 && zoom <= 1000) { state.zoom = zoom; state.curves = {}; }	// Применяет допустимое значение масштаба
                SetWindowTextW(message.hwnd, (std::to_wstring(state.zoom) + L"%").c_str()); InvalidateRect(window, nullptr, FALSE); continue;	// Подтверждает масштаб без нового расчета
            }	// Завершает текущий блок обработки
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(1, BN_CLICKED), 0); continue;	// Запускает расчет введенных параметров
        }	// Завершает текущий блок обработки
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }	// Сохраняет клавиатурную навигацию по элементам
    }	// Завершает текущий блок обработки
    Gdiplus::GdiplusShutdown(token); return static_cast<int>(message.wParam);	// Освобождает кодировщик после закрытия окна
}	// Завершает текущий блок обработки
