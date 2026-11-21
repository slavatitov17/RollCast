#define NOMINMAX	// Настраивает отображение текущего элемента
#include <windows.h>	// Настраивает отображение текущего элемента
#include "formula_view.h"	// Настраивает отображение текущего элемента
#include <algorithm>	// Настраивает отображение текущего элемента
#include <cmath>	// Настраивает отображение текущего элемента
#include <stdexcept>	// Настраивает отображение текущего элемента
#include <vector>	// Настраивает отображение текущего элемента
namespace {	// Настраивает отображение текущего элемента
struct Node {	// Хранит измеренное дерево математического выражения
    std::wstring text;	// Содержит обычные математические символы
    std::vector<Node> children;	// Содержит последовательность либо числитель и знаменатель
    bool fraction = false;	// Различает дробь и горизонтальную последовательность
    int width = 0, height = 0, baseline = 0, font = 24;	// Определяет размеры и базовую линию элемента
};	// Завершает объявление структуры или набора данных
class Font {	// Временно выбирает математический шрифт устройства
    HDC dc; HGDIOBJ previous; HFONT handle;	// Хранит ресурсы и исходное состояние
public:	// Настраивает отображение текущего элемента
    Font(HDC device, int height) : dc(device) {	// Создает курсивное математическое начертание
        handle = CreateFontW(-height, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Cambria");	// Задает размер и шрифт математического текста
        if (!handle) { throw std::runtime_error("Не удалось создать шрифт формулы"); }	// Не продолжает рисование без шрифта
        previous = SelectObject(dc, handle);	// Сохраняет предыдущее начертание
    }	// Завершает текущий блок обработки
    ~Font() { SelectObject(dc, previous); DeleteObject(handle); }	// Восстанавливает шрифт и освобождает созданный ресурс
};	// Завершает объявление структуры или набора данных
Node parse(const std::wstring& value, std::size_t& pos, int depth = 0) {	// Разбирает вложенные числители и знаменатели
    if (depth > 32) { throw std::runtime_error("Слишком глубокая формула"); }	// Ограничивает вложенность математической структуры
    Node result;	// Собирает горизонтальную последовательность элементов
    while (pos < value.size() && value[pos] != L'}') {	// Читает до конца текущей группы
        if (value.compare(pos, 5, L"frac{") == 0) {	// Распознает структурную дробь без косой черты
            pos += 5; Node fraction; fraction.fraction = true;	// Начинает отдельный математический элемент
            fraction.children.push_back(parse(value, pos, depth + 1));	// Читает весь числитель включая вложенные дроби
            if (pos >= value.size() || value[pos++] != L'}' || pos >= value.size() || value[pos++] != L'{') { throw std::runtime_error("Поврежден числитель формулы"); }	// Требует корректную границу двух частей
            fraction.children.push_back(parse(value, pos, depth + 1));	// Читает весь знаменатель
            if (pos >= value.size() || value[pos++] != L'}') { throw std::runtime_error("Поврежден знаменатель формулы"); }	// Проверяет завершенность дроби
            result.children.push_back(std::move(fraction));	// Добавляет дробь в последовательность
        } else {	// Обрабатывает обычную часть выражения
            Node text;	// Собирает соседние печатные символы
            while (pos < value.size() && value[pos] != L'}' && value.compare(pos, 5, L"frac{") != 0) { text.text += value[pos++]; }	// Сохраняет индексы производные и математические знаки
            result.children.push_back(std::move(text));	// Добавляет текстовый элемент выражения
        }	// Завершает текущий блок обработки
    }	// Завершает текущий блок обработки
    return result;	// Возвращает разобранную группу
}	// Завершает текущий блок обработки
void measure(HDC dc, Node& node, int font) {	// Измеряет дерево с учетом вертикального выравнивания
    node.font = font;	// Запоминает размер начертания текущего элемента
    if (!node.text.empty()) {	// Измеряет обычные математические символы
        Font selected(dc, font); SIZE size{}; TEXTMETRICW metrics{};	// Выбирает шрифт для точных размеров
        GetTextExtentPoint32W(dc, node.text.c_str(), static_cast<int>(node.text.size()), &size); GetTextMetricsW(dc, &metrics);	// Получает ширину высоту и положение базовой линии
        node.width = size.cx; node.height = size.cy; node.baseline = metrics.tmAscent;	// Сохраняет геометрию текстового фрагмента
    } else if (node.fraction) {	// Размещает числитель над знаменателем
        for (auto& child : node.children) { measure(dc, child, std::max(10, font - 2)); }	// Уменьшает шрифт вложенных частей дроби
        node.width = std::max(node.children[0].width, node.children[1].width) + 10;	// Оставляет боковые поля дробной черты
        node.height = node.children[0].height + node.children[1].height + 12;	// Оставляет вертикальные зазоры между частями
        node.baseline = node.children[0].height + 5 + font / 4;	// Совмещает дробную черту с математической осью
    } else {	// Размещает последовательность по общей базовой линии
        node.width = node.height = node.baseline = 0;	// Исключает размеры предыдущего измерения
        int descent = 0;	// Находит наибольший нижний вынос
        for (auto& child : node.children) {	// Измеряет каждый соседний элемент
            measure(dc, child, font); node.width += child.width;	// Складывает горизонтальные размеры
            node.baseline = std::max(node.baseline, child.baseline); descent = std::max(descent, child.height - child.baseline);	// Учитывает оба вертикальных выноса
        }	// Завершает текущий блок обработки
        node.height = node.baseline + descent;	// Получает полную высоту последовательности
    }	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
void draw(HDC dc, const Node& node, int x, int y) {	// Рисует математическое дерево в векторном контексте
    if (!node.text.empty()) {	// Выводит текстовую часть выражения
        Font selected(dc, node.font); TextOutW(dc, x, y, node.text.c_str(), static_cast<int>(node.text.size()));	// Сохраняет курсив и все математические символы
    } else if (node.fraction) {	// Выводит настоящую вертикальную дробь
        const auto& numerator = node.children[0]; const auto& denominator = node.children[1];	// Выбирает обе части дроби
        draw(dc, numerator, x + (node.width - numerator.width) / 2, y);	// Центрирует числитель над чертой
        const int bar = y + numerator.height + 5; MoveToEx(dc, x + 1, bar, nullptr); LineTo(dc, x + node.width - 1, bar);	// Рисует горизонтальную дробную черту
        draw(dc, denominator, x + (node.width - denominator.width) / 2, bar + 6);	// Центрирует знаменатель под чертой
    } else {	// Выводит горизонтальную последовательность
        for (const auto& child : node.children) { draw(dc, child, x, y + node.baseline - child.baseline); x += child.width; }	// Согласует базовые линии соседних выражений
    }	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
std::string formulaRtf(const std::wstring& expression, int number, int width) {	// Встраивает масштабируемую векторную формулу в RichEdit
    std::size_t pos = 0; const auto value = expression + L"    (" + std::to_wstring(number) + L")";	// Добавляет последовательный номер уравнения
    auto node = parse(value, pos);	// Строит математическую структуру выражения
    if (pos != value.size()) { throw std::runtime_error("Лишняя скобка в формуле"); }	// Отклоняет неполностью разобранное выражение
    const HDC device = GetDC(nullptr);	// Получает измерительный контекст экрана
    if (!device) { throw std::runtime_error("Недоступен контекст формулы"); }	// Проверяет доступность графического устройства
    try {	// Обеспечивает освобождение измерительного контекста при ошибке
        int font = 24; measure(device, node, font);	// Начинает с читаемого размера математического шрифта
        while (node.width > width && font > 20) { measure(device, node, --font); }	// Уменьшает длинное выражение в разумных пределах
        const int w = node.width + 6, h = node.height + 6;	// Оставляет поля вокруг векторного изображения
        const int dpiX = GetDeviceCaps(device, LOGPIXELSX), dpiY = GetDeviceCaps(device, LOGPIXELSY);	// Согласует физические размеры с разрешением устройства
        const RECT bounds{0, 0, MulDiv(w, 2540, dpiX), MulDiv(h, 2540, dpiY)};	// Переводит размеры метафайла в сотые миллиметра
        const HDC meta = CreateEnhMetaFileW(device, nullptr, &bounds, L"RollCast\0Формула\0\0");	// Создает векторное изображение без временных файлов
        if (!meta) { throw std::runtime_error("Не удалось создать формулу"); }	// Не заменяет формулу неполным представлением
        SetBkMode(meta, TRANSPARENT); SetTextColor(meta, RGB(25,42,55));	// Согласует оформление с текстом справки
        draw(meta, node, 3, 3);	// Выводит числители знаменатели и обычный текст
        const HENHMETAFILE file = CloseEnhMetaFile(meta);	// Завершает запись векторного изображения
        const UINT size = GetEnhMetaFileBits(file, 0, nullptr);	// Определяет объем векторных команд
        std::vector<BYTE> bytes(size); GetEnhMetaFileBits(file, size, bytes.data()); DeleteEnhMetaFile(file);	// Копирует команды и освобождает метафайл
        const double scale = std::min(1.0, static_cast<double>(width) / w);	// Вписывает длинную формулу в текущую ширину справки
        std::string rtf = "{\\pict\\emfblip\\picw" + std::to_string(w) + "\\pich" + std::to_string(h) + "\\picwgoal" + std::to_string(static_cast<int>(w * scale * 1440 / dpiX)) + "\\pichgoal" + std::to_string(static_cast<int>(h * scale * 1440 / dpiY)) + " ";	// Задает точный размер встроенной формулы
        constexpr char hex[] = "0123456789abcdef";	// Кодирует метафайл в поддерживаемый RichEdit формат
        for (const BYTE byte : bytes) { rtf += hex[byte >> 4]; rtf += hex[byte & 15]; }	// Передает векторные команды без растрового масштабирования
        ReleaseDC(nullptr, device); return rtf + "}";	// Возвращает готовую встроенную формулу
    } catch (...) { ReleaseDC(nullptr, device); throw; }	// Освобождает измерительный контекст при отказе
}	// Завершает текущий блок обработки
