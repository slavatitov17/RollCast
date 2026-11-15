#include "mold_view.h"																							// Подключает размеры и интерфейс отрисовки
#include <algorithm>																							// Предоставляет поиск крайних размеров и заполнение массива
#include <cmath>																								// Предоставляет округление координат до пикселей
#include <initializer_list>																						// Позволяет задавать контуры списками точек
#include <string_view>																							// Передаёт подписи без копирования строк
#include <vector>																								// Хранит экранные точки многоугольника
namespace {																										// Оставляет вспомогательные средства внутри модуля разреза
struct Location {																								// Хранит координаты точки на чертеже
    double x;																									// Задаёт положение поперёк формы
    double y;																									// Задаёт положение от верхней части формы вниз
};																												// Завершает описание точки
struct Drawing {																								// Преобразует точки чертежа в координаты окна
    HDC device;																									// Принимает контекст отрисовки окна
    double scale;																								// Определяет единый масштаб по обеим осям
    double originX;																								// Задаёт экранную координату оси валка
    double originY;																								// Задаёт экранную координату верха прибыли
    POINT pixel(Location point) const {																			// Переводит одну точку в пиксели
        return {std::lround(originX + point.x * scale),															// Округляет горизонтальную координату
                std::lround(originY + point.y * scale)};														// Округляет вертикальную координату
    }																											// Завершает преобразование точки
    void shape(std::initializer_list<Location> outline, HBRUSH fill) const {									// Рисует замкнутый контур с заливкой
        std::vector<POINT> points;																				// Подготавливает точки для GDI
        points.reserve(outline.size());																			// Резервирует память для всего контура
        for (const Location vertex : outline) {																	// Обходит опорные точки контура
            points.push_back(pixel(vertex));																	// Добавляет экранную точку
        }																										// Завершает преобразование контура
        const HGDIOBJ previous = SelectObject(device, fill);													// Выбирает кисть текущего элемента
        Polygon(device, points.data(), static_cast<int>(points.size()));										// Рисует заполненный контур
        SelectObject(device, previous);																			// Восстанавливает прежнюю кисть
    }																											// Завершает отрисовку многоугольника
    void box(double left, double top, double right, double bottom, HBRUSH fill) const {							// Рисует прямоугольный элемент
        shape({{left, top}, {right, top}, {right, bottom}, {left, bottom}}, fill);								// Передаёт четыре угла элемента
    }																											// Завершает отрисовку прямоугольника
    void moveTo(Location point) const {																			// Устанавливает начальную точку линии
        const POINT screen = pixel(point);																		// Вычисляет экранные координаты начала
        MoveToEx(device, screen.x, screen.y, nullptr);															// Перемещает перо
    }																											// Завершает установку начала линии
    void lineTo(Location point) const {																			// Проводит отрезок до заданной точки
        const POINT screen = pixel(point);																		// Вычисляет экранные координаты конца
        LineTo(device, screen.x, screen.y);																		// Рисует отрезок текущим пером
    }																											// Завершает отрисовку отрезка
    void curveTo(Location first, Location second, Location end) const {											// Проводит кубическую кривую Безье
        const POINT points[]{pixel(first), pixel(second), pixel(end)};											// Преобразует две опорные и конечную точки
        PolyBezierTo(device, points, 3);																		// Рисует кривую от текущего положения пера
    }																											// Завершает отрисовку кривой
};																												// Завершает описание координат чертежа
HBRUSH createStipple(HDC device) {																				// Создаёт мелкий точечный узор подложки
    BITMAPINFO texture{};																						// Подготавливает параметры восьмипиксельного образца
    texture.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);														// Задаёт размер заголовка формата DIB
    texture.bmiHeader.biWidth = 8;																				// Задаёт ширину повторяемого образца
    texture.bmiHeader.biHeight = -8;																			// Задаёт строки образца сверху вниз
    texture.bmiHeader.biPlanes = 1;																				// Указывает одну цветовую плоскость
    texture.bmiHeader.biBitCount = 32;																			// Хранит по четыре байта на пиксель
    void* data = nullptr;																						// Принимает адрес буфера пикселей
    const HBITMAP bitmap = CreateDIBSection(device, &texture, DIB_RGB_COLORS, &data, nullptr, 0);				// Создаёт образец кисти
    if (!bitmap) {																								// Проверяет успешность создания образца
        return CreateSolidBrush(RGB(231, 237, 241));															// Возвращает однотонную подложку при ошибке
    }																											// Завершает проверку буфера
    auto* pixels = static_cast<DWORD*>(data);																	// Получает доступ к каждому пикселю
    std::fill_n(pixels, 64, 0xE7EDF1);																			// Заполняет образец светлым цветом
    pixels[9] = pixels[45] = 0x8597A4;																			// Расставляет две точки по диагонали
    const HBRUSH brush = CreatePatternBrush(bitmap);															// Создаёт кисть с повторяемым узором
    DeleteObject(bitmap);																						// Освобождает исходный образец после создания кисти
    return brush;																								// Передаёт кисть вызывающей функции
}																												// Завершает создание точечной кисти
void drawFasteners(const Drawing& drawing, double radius, double height, HBRUSH brush) {						// Рисует парные скобы опоки
    for (double side : {-1.0, 1.0}) {																			// Повторяет форму скобы с противоположной стороны
        const double inner = side * (radius - 22);																// Находит точку захвата у стенки
        const double outer = side * (radius + 72);																// Задаёт внешний край скобы
        const double lip = side * (radius + 37);																// Задаёт внутренний уступ скобы
        drawing.shape({{inner, height - 80}, {outer, height - 80}, {outer, height + 80},						// Задаёт внешний контур скобы
                       {inner, height + 80}, {inner, height + 39}, {lip, height + 39},							// Формирует нижний захват скобы
                       {lip, height - 39}, {inner, height - 39}}, brush);										// Формирует верхний захват скобы
    }																											// Завершает отрисовку пары
}																												// Завершает отрисовку скоб
void leftLabel(const Drawing& drawing, double radius, double top, double bottom, std::wstring_view name) {		// Подписывает отделение
    if ((bottom - top) * drawing.scale < 23) {																	// Проверяет место для подписи по высоте
        return;																									// Пропускает подпись при слишком малой высоте
    }																											// Завершает проверку места
    const POINT anchor = drawing.pixel({-radius, (top + bottom) / 2});											// Находит середину стенки отделения
    MoveToEx(drawing.device, anchor.x - 11, anchor.y, nullptr);													// Начинает короткую выноску
    LineTo(drawing.device, anchor.x, anchor.y);																	// Доводит выноску до стенки
    SetTextAlign(drawing.device, TA_RIGHT | TA_BASELINE);														// Прижимает подпись к выноске справа
    TextOutW(drawing.device, anchor.x - 15, anchor.y + 4, name.data(), static_cast<int>(name.size()));			// Выводит название отделения
}																												// Завершает подпись отделения
void rightLabel(const Drawing& drawing, Location start, std::wstring_view name) {								// Подписывает элемент сифона
    const POINT anchor = drawing.pixel(start);																	// Определяет экранную точку выноски
    MoveToEx(drawing.device, anchor.x, anchor.y, nullptr);														// Начинает линию у края элемента
    LineTo(drawing.device, anchor.x + 12, anchor.y);															// Выводит линию за край элемента
    SetTextAlign(drawing.device, TA_LEFT | TA_BASELINE);														// Помещает подпись справа от линии
    TextOutW(drawing.device, anchor.x + 16, anchor.y + 4, name.data(), static_cast<int>(name.size()));			// Выводит название элемента
}																												// Завершает подпись сифона
}																												// Завершает область вспомогательных средств
void drawMold(HDC device, const RECT& area, const MoldDimensions& dimensions) {									// Рисует форму во внутренней части панели
    const auto& [lower, barrel, upper, riser] = dimensions;														// Распаковывает отделения снизу вверх
    const double riserRadius = riser.diameter / 2.0;															// Вычисляет наружный радиус полости прибыли
    const double upperRadius = upper.diameter / 2.0;															// Вычисляет радиус верхней шейки
    const double barrelRadius = barrel.diameter / 2.0;															// Вычисляет радиус бочки
    const double lowerRadius = lower.diameter / 2.0;															// Вычисляет радиус нижней шейки
    const double frame = std::max({riserRadius, upperRadius, barrelRadius, lowerRadius}) + 235;					// Оставляет толщину вокруг самой широкой полости
    const double upperTop = riser.height;																		// Находит уровень между прибылью и шейкой
    const double barrelTop = upperTop + upper.height;															// Находит верхний торец бочки
    const double lowerTop = barrelTop + barrel.height;															// Находит нижний торец бочки
    const double floor = lowerTop + lower.height;																// Находит дно полости нижней шейки
    const double baseTop = floor - lower.height * 0.49;															// Находит верхнюю плоскость нижней опоки
    const double pipe = frame + 475;																			// Выносит ось литниковой трубы вправо
    const double collar = pipe + 172;																			// Находит правую стенку литниковой трубы
    const double funnelTop = -465;																				// Поднимает воронку над верхом формы
    const double funnelBottom = -90;																			// Находит место соединения воронки с трубой
    const double sceneWidth = pipe + 365 + frame + 135;															// Учитывает воронку и края подопочной плиты
    const double sceneHeight = floor + 155 - funnelTop;															// Учитывает высоту воронки и подопочной плиты
    const double width = area.right - area.left - 244;															// Резервирует место для подписей с обеих сторон
    const double height = area.bottom - area.top - 36;															// Оставляет поля сверху и снизу
    if (width <= 0 || height <= 0) {																			// Проверяет место для чертежа
        return;																									// Пропускает отрисовку при свёрнутом окне
    }																											// Завершает проверку области
    const double scale = std::min(width / sceneWidth, height / sceneHeight);									// Вписывает весь разрез с сохранением пропорций
    const Drawing drawing{device, scale,																		// Подготавливает единый масштаб разреза
        area.left + 122 + (width - sceneWidth * scale) / 2 + (frame + 135) * scale,								// Центрирует чертёж между боковыми полями
        area.top + 18 + (height - sceneHeight * scale) / 2 - funnelTop * scale};								// Центрирует чертёж по высоте
    const int saved = SaveDC(device);																			// Сохраняет настройки контекста окна
    SetBkColor(device, RGB(218, 227, 233));																		// Задаёт фон диагональной штриховки
    SetBkMode(device, OPAQUE);																					// Закрашивает фон между штрихами
    const HPEN outline = CreatePen(PS_SOLID, 1, RGB(49, 65, 77));												// Создаёт чёткий внешний контур
    const HBRUSH hatch = CreateHatchBrush(HS_BDIAGONAL, RGB(114, 134, 148));									// Создаёт диагональные штрихи оснастки
    const HBRUSH dots = createStipple(device);																	// Создаёт точечную заливку подложки
    const HBRUSH fastener = CreateSolidBrush(RGB(166, 184, 195));												// Выделяет кромки и крепления оснастки
    const auto empty = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));										// Показывает незаполненные полости белыми
    SelectObject(device, outline);																				// Задаёт перо для контуров формы
    drawing.box(-frame, 0, frame, baseTop, hatch);																// Рисует высокую наружную опоку
    drawing.box(-frame + 120, 0, frame - 120, baseTop, dots);													// Рисует внутреннюю подложку вокруг полостей
    drawing.box(-frame - 25, barrelTop - upper.height * 0.23, frame + 25, lowerTop + 26, hatch);				// Выделяет широкий пояс вокруг бочки
    drawing.box(-barrelRadius - 27, barrelTop, barrelRadius + 27, lowerTop, dots);								// Добавляет узкую прослойку вокруг бочки
    drawing.box(-frame - 14, baseTop, collar + 28, floor + 92, hatch);											// Рисует общую нижнюю опоку
    drawing.box(-frame + 118, baseTop, collar - 47, floor + 67, dots);											// Заполняет внутренний объём нижней опоки
    drawing.box(-frame - 135, floor + 92, pipe + 365, floor + 128, fastener);									// Формирует подопочную плиту под всей сборкой
    drawing.box(-frame - 82, floor + 67, collar + 83, floor + 92, hatch);										// Рисует опорный пояс над плитой
    drawing.box(-frame - 24, -25, frame + 34, 19, fastener);													// Обводит верхнюю кромку формы
    drawing.box(-frame - 89, -17, -riserRadius * 0.88, 69, hatch);												// Добавляет выступ верхней рамки
    drawFasteners(drawing, frame, upperTop, fastener);															// Устанавливает парные скобы на стыке прибыли
    drawFasteners(drawing, frame, baseTop, fastener);															// Устанавливает скобы нижней опоки
    drawing.shape({{-riserRadius * 0.88, 0}, {riserRadius * 0.88, 0}, {riserRadius, upperTop},					// Задаёт верх и боковые границы прибыли
                   {-riserRadius, upperTop}}, empty);															// Замыкает контур прибыли у верхней шейки
    drawing.shape({{-upperRadius * 0.82, upperTop}, {upperRadius * 0.82, upperTop},								// Задаёт узкую верхнюю часть шейки
                   {upperRadius * 0.93, barrelTop - upper.height * 0.12}, {upperRadius, barrelTop},				// Формирует правый скос возле бочки
                   {-upperRadius, barrelTop}, {-upperRadius * 0.93, barrelTop - upper.height * 0.12}}, empty);	// Формирует левый скос шейки
    drawing.box(-barrelRadius, barrelTop, barrelRadius, lowerTop, empty);										// Рисует цилиндрическую полость бочки
    drawing.shape({{-lowerRadius, lowerTop}, {lowerRadius, lowerTop}, {lowerRadius * 0.76, floor},				// Формирует правый скос нижней шейки
                   {-lowerRadius * 0.76, floor}}, empty);														// Формирует левый скос нижней шейки
    drawing.moveTo({-frame, upperTop});																			// Начинает линию стыка верхних рамок
    drawing.lineTo({-riserRadius, upperTop});																	// Доводит стык до левой стенки прибыли
    drawing.moveTo({std::max(riserRadius, upperRadius * 0.82), upperTop});										// Продолжает стык за правой полостью
    drawing.lineTo({frame, upperTop});																			// Доводит стык до правой стенки формы
    drawing.box(-10, 0, 10, barrelTop + barrel.height * 0.22, fastener);										// Рисует тонкий центральный стержень на схеме
    drawing.box(pipe - 172, funnelBottom, collar, baseTop, hatch);												// Рисует наружную стенку литниковой трубы
    for (double joint : {funnelBottom, floor * 0.27, floor * 0.56, baseTop}) {									// Расставляет крепления вдоль трубы
        drawing.box(pipe - 248, joint - 26, pipe + 248, joint + 26, hatch);										// Рисует кольцевой выступ в разрезе
    }																											// Завершает отрисовку креплений
    drawing.box(pipe - 125, funnelBottom, pipe + 125, baseTop, dots);											// Рисует промежуточный слой трубы
    drawing.box(pipe - 64, funnelBottom, pipe + 64, baseTop, empty);											// Рисует вертикальный канал сифона
    drawing.shape({{pipe - 365, funnelTop}, {pipe + 365, funnelTop},											// Задаёт наружную верхнюю кромку воронки
                   {pipe + 125, funnelBottom}, {pipe - 125, funnelBottom}}, hatch);								// Рисует скошенные наружные стенки воронки
    drawing.shape({{pipe - 303, funnelTop}, {pipe + 303, funnelTop},											// Задаёт широкое входное отверстие воронки
                   {pipe + 64, funnelBottom}, {pipe - 64, funnelBottom}}, empty);								// Соединяет полость воронки с каналом сифона
    const double entry = lowerRadius * 0.78;																	// Совмещает вход питателя с нижней шейкой
    const double gate = floor - lower.height * 0.12;															// Располагает питатель немного выше дна
    const double turn = baseTop + lower.height * 0.28;															// Находит высоту разворота канала сифона
    const double halfGate = std::min(28.0, lower.height * 0.06);												// Сохраняет толщину канала при малой высоте
    const HGDIOBJ lastBrush = SelectObject(device, empty);														// Выбирает белую заливку канала
    BeginPath(device);																							// Собирает непрерывный контур колена и питателя
    drawing.moveTo({pipe - 64, baseTop - 3});																	// Начинает контур внутри канала трубы
    drawing.curveTo({pipe - 64, turn - 48}, {pipe - 142, turn - 30}, {pipe - 212, turn - 22});					// Прокладывает внутренний скруглённый поворот
    drawing.lineTo({entry - 6, gate - halfGate});																// Подводит верхнюю грань питателя к шейке
    drawing.lineTo({entry - 6, gate + halfGate});																// Задаёт отверстие со стороны шейки
    drawing.lineTo({pipe - 204, turn + 30});																	// Ведёт нижнюю грань к колену сифона
    drawing.curveTo({pipe - 74, turn + 21}, {pipe + 64, turn + 4}, {pipe + 64, baseTop - 3});					// Прокладывает наружный скруглённый поворот
    CloseFigure(device);																						// Замыкает контур у входа из трубы
    EndPath(device);																							// Завершает построение контура
    StrokeAndFillPath(device);																					// Рисует стенки и белую полость питателя
    SelectObject(device, lastBrush);																			// Восстанавливает прежнюю кисть после питателя
    SelectObject(device, GetStockObject(DEFAULT_GUI_FONT));														// Использует системный шрифт для выносок
    SetBkMode(device, TRANSPARENT);																				// Оставляет фон чертежа видимым за подписями
    SetTextColor(device, RGB(39, 53, 65));																		// Задаёт контрастный цвет текста
    leftLabel(drawing, frame, 0, upperTop, L"Прибыль");															// Подписывает верхнее отделение формы
    leftLabel(drawing, frame, upperTop, barrelTop, L"Верхняя шейка");											// Подписывает верхнюю шейку валка
    leftLabel(drawing, frame + 25, barrelTop, lowerTop, L"Бочка");												// Подписывает основное отделение формы
    leftLabel(drawing, frame + 14, lowerTop, floor, L"Нижняя шейка");											// Подписывает нижнюю шейку валка
    rightLabel(drawing, {pipe + 271, funnelTop + 145}, L"Воронка");												// Подписывает вход литниковой системы
    rightLabel(drawing, {collar, floor * 0.38}, L"Сифон");														// Подписывает вертикальный канал
    RestoreDC(device, saved);																					// Возвращает настройки контекста окна
    DeleteObject(fastener);																						// Освобождает кисть креплений
    DeleteObject(dots);																							// Освобождает кисть точечной заливки
    DeleteObject(hatch);																						// Освобождает кисть штриховки
    DeleteObject(outline);																						// Освобождает перо контуров
}																												// Завершает отрисовку разреза
