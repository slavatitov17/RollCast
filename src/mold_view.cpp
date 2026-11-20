#include "mold_view.h"																								// Подключает размеры и интерфейс отрисовки
#include <algorithm>																								// Предоставляет поиск крайних размеров и заполнение массива
#include <cmath>																									// Предоставляет округление координат до пикселей
#include <initializer_list>																							// Позволяет задавать контуры списками точек
#include <cwchar>																									// Измеряет длину подписи для Win32
#include <vector>																									// Хранит экранные точки многоугольника
MoldHitMap::~MoldHitMap() {																							// Освобождает карту перед завершением программы
    clear();																										// Удаляет оставшиеся графические области
}																													// Завершает освобождение карты
void MoldHitMap::clear() {																							// Удаляет экранные области старого кадра
    for (const auto& area : areas) {																				// Обходит записанные элементы формы
        DeleteObject(area.region);																					// Освобождает графический дескриптор
    }																												// Завершает обход элементов
    areas.clear();																																			// Удаляет освобождённые элементы карты
    if (casting) { DeleteObject(casting); casting = nullptr; }																								// Освобождает объединённую полость отливки
    if (runners) { DeleteObject(runners); runners = nullptr; }																								// Освобождает объединённые каналы
}																													// Завершает очистку карты
void MoldHitMap::add(HRGN region, MoldPart part) {																	// Сохраняет область вместе с именем элемента
    if (region) {																									// Проверяет успешность создания области
        areas.push_back({region, part});																			// Сохраняет область в порядке отрисовки
    }																												// Завершает проверку области
}																													// Завершает регистрацию области
void MoldHitMap::addFluid(HRGN region, bool metal) {																										// Объединяет белые полости без стенок формы
    HRGN& target = metal ? casting : runners;																												// Выбирает отливку или литниковую систему
    if (!target) { target = CreateRectRgn(0, 0, 0, 0); }																									// Создаёт начальную пустую область
    CombineRgn(target, target, region, RGN_OR);																												// Добавляет очередную полость к маске
}																																							// Завершает объединение полостей
MoldPart MoldHitMap::partAt(POINT cursor) const {																	// Находит видимый элемент под курсором
    for (auto it = areas.rbegin(); it != areas.rend(); ++it) {														// Ищет от верхнего слоя изображения
        if (PtInRegion(it->region, cursor.x, cursor.y)) {															// Проверяет попадание в контур элемента
            return it->part;																						// Возвращает первый видимый элемент
        }																											// Завершает проверку контура
    }																												// Завершает поиск по изображению
    return MoldPart::none;																							// Оставляет форму без подсветки вне контуров
}																													// Завершает поиск элемента
namespace {																											// Оставляет вспомогательные средства внутри модуля разреза
struct Location {																									// Хранит координаты точки на чертеже
    double x;																										// Задаёт положение поперёк формы
    double y;																										// Задаёт положение от верхней части формы вниз
};																													// Завершает описание точки
struct Drawing {																									// Преобразует точки чертежа в координаты окна
    HDC device;																										// Принимает контекст отрисовки окна
    double scale;																									// Определяет единый масштаб по обеим осям
    double originX;																									// Задаёт экранную координату оси валка
    double originY;																									// Задаёт экранную координату верха прибыли
    const RECT& viewport;																							// Ограничивает подпись рамкой чертежа
    MoldHitMap& hits;																								// Принимает карту экранных областей формы
    MoldPart hovered;																								// Хранит текущий элемент под курсором
    HPEN outline;																									// Задаёт обычный контур детали
    HPEN mutedOutline;																								// Задаёт осветлённый контур остальных деталей
    HBRUSH mutedFill;																																		// Хранит кисть приглушённых элементов
    bool rebuild;																																			// Управляет обновлением геометрической карты
    void style(MoldPart part, HBRUSH fill) const {																	// Выбирает яркость текущей части формы
        const bool muted = hovered != MoldPart::none && hovered != part;											// Проверяет необходимость приглушения элемента
        SelectObject(device, muted ? mutedOutline : outline);														// Выбирает цвет контура
        SelectObject(device, muted ? mutedFill : fill);																// Выбирает обычную или осветлённую заливку
    }																												// Завершает подбор цвета элемента
    POINT pixel(Location point) const {																				// Переводит одну точку в пиксели
        return {std::lround(originX + point.x * scale),																// Округляет горизонтальную координату
                std::lround(originY + point.y * scale)};															// Округляет вертикальную координату
    }																												// Завершает преобразование точки
    void shape(std::initializer_list<Location> outlinePoints, HBRUSH fill, MoldPart part) const {					// Рисует и регистрирует замкнутый контур
        std::vector<POINT> points;																					// Подготавливает точки для GDI
        points.reserve(outlinePoints.size());																		// Резервирует память для всего контура
        for (const Location vertex : outlinePoints) {																// Обходит опорные точки контура
            points.push_back(pixel(vertex));																		// Добавляет экранную точку
        }																											// Завершает преобразование контура
        const int count = static_cast<int>(points.size());															// Переводит размер к типу Win32
        style(part, fill);																							// Устанавливает цвет элемента формы
        Polygon(device, points.data(), count);																		// Рисует заполненный контур
        if (rebuild) {																																		// Строит области только после изменения геометрии
            const HRGN region = CreatePolygonRgn(points.data(), count, ALTERNATE);																			// Создаёт область многоугольника
            if (fill == GetStockObject(WHITE_BRUSH)) {																										// Распознаёт внутреннюю полость формы
                const bool metal = part == MoldPart::upperModel || part == MoldPart::plug || part == MoldPart::barrel || part == MoldPart::lowerModelMiddle || part == MoldPart::lowerModelBottom;	// Отделяет отливку от литников
                hits.addFluid(region, metal);																												// Дополняет маску металла
            }																																				// Завершает обработку внутренних полостей
            hits.add(region, part);																															// Передаёт области владение геометрической карте
        }																																					// Завершает обновление областей наведения
    }																												// Завершает отрисовку многоугольника
    void box(double left, double top, double right, double bottom, HBRUSH fill, MoldPart part) const {				// Рисует прямоугольный элемент
        shape({{left, top}, {right, top}, {right, bottom}, {left, bottom}}, fill, part);							// Передаёт четыре угла элемента
    }																												// Завершает отрисовку прямоугольника
    void moveTo(Location point) const {																				// Устанавливает начальную точку линии
        const POINT screen = pixel(point);																			// Вычисляет экранные координаты начала
        MoveToEx(device, screen.x, screen.y, nullptr);																// Перемещает перо
    }																												// Завершает установку начала линии
    void lineTo(Location point) const {																				// Проводит отрезок до заданной точки
        const POINT screen = pixel(point);																			// Вычисляет экранные координаты конца
        LineTo(device, screen.x, screen.y);																			// Рисует отрезок текущим пером
    }																												// Завершает отрисовку отрезка
    void curveTo(Location first, Location second, Location end) const {												// Проводит кубическую кривую Безье
        const POINT points[]{pixel(first), pixel(second), pixel(end)};												// Преобразует две опорные и конечную точки
        PolyBezierTo(device, points, 3);																			// Рисует кривую от текущего положения пера
    }																												// Завершает отрисовку кривой
};																													// Завершает описание координат чертежа
HBRUSH createStipple(HDC device) {																					// Создаёт мелкий точечный узор подложки
    BITMAPINFO texture{};																							// Подготавливает параметры восьмипиксельного образца
    texture.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);															// Задаёт размер заголовка формата DIB
    texture.bmiHeader.biWidth = 8;																					// Задаёт ширину повторяемого образца
    texture.bmiHeader.biHeight = -8;																				// Задаёт строки образца сверху вниз
    texture.bmiHeader.biPlanes = 1;																					// Указывает одну цветовую плоскость
    texture.bmiHeader.biBitCount = 32;																				// Хранит по четыре байта на пиксель
    void* data = nullptr;																							// Принимает адрес буфера пикселей
    const HBITMAP bitmap = CreateDIBSection(device, &texture, DIB_RGB_COLORS, &data, nullptr, 0);					// Создаёт образец кисти
    if (!bitmap) {																									// Проверяет успешность создания образца
        return CreateSolidBrush(RGB(231, 237, 241));																// Возвращает однотонную подложку при ошибке
    }																												// Завершает проверку буфера
    auto* pixels = static_cast<DWORD*>(data);																		// Получает доступ к каждому пикселю
    std::fill_n(pixels, 64, 0xE7EDF1);																				// Заполняет образец светлым цветом
    pixels[9] = pixels[45] = 0x8597A4;																				// Расставляет две точки по диагонали
    const HBRUSH brush = CreatePatternBrush(bitmap);																// Создаёт кисть с повторяемым узором
    DeleteObject(bitmap);																							// Освобождает исходный образец после создания кисти
    return brush;																									// Передаёт кисть вызывающей функции
}																													// Завершает создание точечной кисти
void drawFasteners(const Drawing& drawing, double radius, double height, HBRUSH brush, MoldPart part) {				// Рисует парные скобы опоки
    for (double side : {-1.0, 1.0}) {																				// Повторяет форму скобы с противоположной стороны
        const double inner = side * (radius - 22);																	// Находит точку захвата у стенки
        const double outer = side * (radius + 72);																	// Задаёт внешний край скобы
        const double lip = side * (radius + 37);																	// Задаёт внутренний уступ скобы
        drawing.shape({{inner, height - 80}, {outer, height - 80}, {outer, height + 80},							// Задаёт внешний контур скобы
                       {inner, height + 80}, {inner, height + 39}, {lip, height + 39},								// Формирует нижний захват скобы
                       {lip, height - 39}, {inner, height - 39}}, brush, part);										// Формирует верхний захват скобы
    }																												// Завершает отрисовку пары
}																													// Завершает отрисовку скоб
const wchar_t* partName(MoldPart part) {																			// Сопоставляет элементу подпись исходной схемы
    switch (part) {																									// Выбирает название видимой области
        case MoldPart::riser: return L"Прибыльная надставка";														// Называет верхнюю надставку
        case MoldPart::upperModel: return L"Модель верхней шейки";													// Называет модель верхней шейки
        case MoldPart::funnel: return L"Воронка";																	// Называет вход литниковой системы
        case MoldPart::pipe: return L"Литниковая труба";															// Называет вертикальную литниковую трубу
        case MoldPart::upperFrame: return L"Верхняя рамка";															// Называет рамку над кокилем
        case MoldPart::rod: return L"«Сигналка»";																	// Называет центральный контрольный стержень
        case MoldPart::plug: return L"Модель заделки";																// Называет нижнюю зону модели верхней шейки
        case MoldPart::chill: return L"Кокиль";																		// Называет обе боковые стенки бочки
        case MoldPart::barrel: return L"Бочка";																		// Называет центральную часть валка
        case MoldPart::lowerModelMiddle: return L"Модель нижней шейки, средняя часть";								// Называет верхнюю часть нижней модели
        case MoldPart::middleFrame: return L"Средняя рамка";														// Называет рамку под кокилем
        case MoldPart::lowerModelBottom: return L"Модель нижней шейки, нижняя часть";								// Называет нижнюю часть модели
        case MoldPart::flask: return L"Нижняя опока";																// Называет нижнюю часть литейной оснастки
        case MoldPart::plate: return L"Подопочная плита";															// Называет плиту под сборкой
        case MoldPart::gate: return L"Питатель";																	// Называет подвод к полости валка
        case MoldPart::elbow: return L"Колено питателя";															// Называет поворот литникового канала
        case MoldPart::none: return L"";																			// Оставляет свободную область без подписи
    }																												// Завершает выбор названия
    return L"";																										// Защищает вывод при неизвестном идентификаторе
}																													// Завершает получение названия
}																																							// Завершает область внутренних функций формы
void drawMoldLabel(HDC device, const RECT& area, MoldPart part, POINT cursor) {																				// Показывает название рядом с указателем
    const wchar_t* text = partName(part);																													// Получает название выбранного элемента
    const int length = static_cast<int>(std::wcslen(text));															// Находит длину строки для Win32
    if (length == 0) {																								// Проверяет наличие выбранного элемента
        return;																										// Оставляет рисунок без подписи
    }																												// Завершает проверку выбора
    SelectObject(device, GetStockObject(DEFAULT_GUI_FONT));																									// Выбирает системный шрифт подписи
    SetBkMode(device, TRANSPARENT);																															// Оставляет заливку подсказки под текстом
    SetTextColor(device, RGB(27, 45, 57));																													// Задаёт читаемый тёмный цвет текста
    SIZE size{};																									// Принимает размеры текста в пикселях
    GetTextExtentPoint32W(device, text, length, &size);																										// Измеряет подпись выбранного элемента
    const LONG x = std::clamp(cursor.x + 14, area.left + 3, std::max(area.left + 3, area.right - size.cx - 19));	// Вмещает подпись по горизонтали
    const LONG y = std::clamp(cursor.y + 17, area.top + 3, std::max(area.top + 3, area.bottom - size.cy - 15));		// Вмещает подпись по вертикали
    const RECT label{x, y, x + size.cx + 15, y + size.cy + 10};														// Задаёт поля вокруг подписи
    FillRect(device, &label, GetSysColorBrush(COLOR_INFOBK));																								// Закрашивает фон подписи
    FrameRect(device, &label, GetSysColorBrush(COLOR_3DSHADOW));																							// Обводит подсказку тонкой рамкой
    TextOutW(device, x + 7, y + 5, text, length);																											// Выводит имя возле выбранного элемента
}																													// Завершает отрисовку подписи
void drawMold(HDC device, const RECT& area, const MoldDimensions& dimensions,										// Рисует форму внутри правой панели
    MoldHitMap& hits, MoldPart hovered, POINT cursor, const MoldFrame& frameState, bool rebuild) {															// Подписывает элемент и сохраняет экранные области
    if (rebuild) { hits.clear(); }																															// Обновляет карту только при новой геометрии
    const auto& [lower, barrel, upper, riser] = dimensions;															// Распаковывает отделения снизу вверх
    const double riserRadius = riser.diameter / 2.0;																// Вычисляет наружный радиус полости прибыли
    const double upperRadius = upper.diameter / 2.0;																// Вычисляет радиус верхней шейки
    const double barrelRadius = barrel.diameter / 2.0;																// Вычисляет радиус бочки
    const double lowerRadius = lower.diameter / 2.0;																// Вычисляет радиус нижней шейки
    const double frame = std::max({riserRadius, upperRadius, barrelRadius, lowerRadius}) + 235;						// Оставляет толщину вокруг самой широкой полости
    const double upperTop = riser.height;																			// Находит уровень между прибылью и шейкой
    const double barrelTop = upperTop + upper.height;																// Находит верхний торец бочки
    const double lowerTop = barrelTop + barrel.height;																// Находит нижний торец бочки
    const double floor = lowerTop + lower.height;																	// Находит дно полости нижней шейки
    const double baseTop = floor - lower.height * 0.49;																// Находит верхнюю плоскость нижней опоки
    const double pipe = frame + 475;																				// Выносит ось литниковой трубы вправо
    const double collar = pipe + 172;																				// Находит правую стенку литниковой трубы
    const double funnelTop = -465;																					// Поднимает воронку над верхом формы
    const double funnelBottom = -90;																				// Находит место соединения воронки с трубой
    const double sceneWidth = pipe + 365 + frame + 135;																// Учитывает воронку и края подопочной плиты
    const double sceneHeight = floor + 155 - funnelTop;																// Учитывает высоту воронки и подопочной плиты
    const double width = area.right - area.left - 52;																// Оставляет поля по обеим сторонам формы
    const double height = area.bottom - area.top - 36;																// Оставляет поля сверху и снизу
    if (width <= 0 || height <= 0) {																				// Проверяет место для чертежа
        return;																										// Пропускает отрисовку при свёрнутом окне
    }																												// Завершает проверку области
    const double scale = std::min(width / sceneWidth, height / sceneHeight);										// Вписывает весь разрез с сохранением пропорций
    const int saved = SaveDC(device);																				// Сохраняет настройки контекста окна
    SetBkColor(device, RGB(218, 227, 233));																			// Задаёт фон диагональной штриховки
    SetBkMode(device, OPAQUE);																						// Закрашивает фон между штрихами
    const HPEN outline = CreatePen(PS_SOLID, 1, RGB(49, 65, 77));													// Создаёт чёткий внешний контур
    const HBRUSH hatch = CreateHatchBrush(HS_BDIAGONAL, RGB(114, 134, 148));										// Создаёт диагональные штрихи оснастки
    const HBRUSH dots = createStipple(device);																		// Создаёт точечную заливку подложки
    const HBRUSH fastener = CreateSolidBrush(RGB(166, 184, 195));													// Выделяет кромки и крепления оснастки
    const auto empty = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));											// Показывает незаполненные полости белыми
    const HPEN mutedOutline = CreatePen(PS_SOLID, 1, RGB(202, 211, 217));											// Осветляет контур невыбранных элементов
    const HBRUSH mutedFill = CreateSolidBrush(RGB(245, 248, 250));													// Осветляет заливку невыбранных элементов
    const Drawing drawing{device, scale,																			// Подготавливает единый масштаб разреза
        area.left + 26 + (width - sceneWidth * scale) / 2 + (frame + 135) * scale,									// Центрирует чертёж по горизонтали
        area.top + 18 + (height - sceneHeight * scale) / 2 - funnelTop * scale,										// Центрирует чертёж по высоте
        area, hits, hovered, outline, mutedOutline, mutedFill, rebuild};																					// Подключает области наведения и цвета подсветки
    const double plugTop = barrelTop - upper.height * 0.23;															// Отмечает границу модели заделки
    drawing.box(-frame, 0, frame, upperTop, hatch, MoldPart::riser);												// Рисует стенки прибыльной надставки
    drawing.box(-frame + 120, 0, frame - 120, upperTop, dots, MoldPart::riser);										// Рисует внутренние слои надставки
    drawing.box(-frame, upperTop, frame, plugTop, hatch, MoldPart::upperFrame);										// Рисует наружный пояс верхней рамки
    drawing.box(-frame + 120, upperTop, frame - 120, plugTop, dots, MoldPart::upperFrame);							// Рисует внутреннюю часть рамки
    drawing.box(-frame - 25, plugTop, frame + 25, lowerTop, hatch, MoldPart::chill);								// Рисует обе стенки кокиля
    drawing.box(-barrelRadius - 27, barrelTop, barrelRadius + 27, lowerTop, dots, MoldPart::chill);					// Добавляет прослойку у поверхности бочки
    drawing.box(-frame, lowerTop, frame, baseTop, hatch, MoldPart::middleFrame);									// Рисует наружную часть средней рамки
    drawing.box(-frame + 120, lowerTop, frame - 120, baseTop, dots, MoldPart::middleFrame);							// Рисует внутреннюю часть средней рамки
    drawing.box(-frame - 14, baseTop, collar + 28, floor + 92, hatch, MoldPart::flask);								// Рисует общую нижнюю опоку
    drawing.box(-frame + 118, baseTop, collar - 47, floor + 67, dots, MoldPart::flask);								// Заполняет внутренний объём нижней опоки
    drawing.box(-frame - 135, floor + 92, pipe + 365, floor + 128, fastener, MoldPart::plate);						// Рисует подопочную плиту
    drawing.box(-frame - 82, floor + 67, collar + 83, floor + 92, hatch, MoldPart::flask);							// Рисует опорный пояс нижней опоки
    drawing.box(-frame - 24, -25, frame + 34, 19, fastener, MoldPart::riser);										// Обводит верхнюю кромку формы
    drawing.box(-frame - 89, -17, -riserRadius * 0.88, 69, hatch, MoldPart::riser);									// Добавляет выступ верхней надставки
    drawFasteners(drawing, frame, upperTop, fastener, MoldPart::upperFrame);										// Устанавливает парные скобы верхней рамки
    drawFasteners(drawing, frame, baseTop, fastener, MoldPart::flask);												// Устанавливает скобы нижней опоки
    drawing.shape({{-riserRadius * 0.88, 0}, {riserRadius * 0.88, 0}, {riserRadius, upperTop},						// Задаёт контур верхней части модели
                   {-riserRadius, upperTop}}, empty, MoldPart::upperModel);											// Относит белую полость к верхней шейке
    const double plugRadius = upperRadius * 0.91;																	// Находит ширину горловины на стыке модели
    drawing.shape({{-upperRadius * 0.82, upperTop}, {upperRadius * 0.82, upperTop},									// Задаёт узкую верхнюю часть шейки
                   {plugRadius, plugTop}, {-plugRadius, plugTop}}, empty, MoldPart::upperModel);					// Рисует основную часть верхней модели
    drawing.shape({{-plugRadius, plugTop}, {plugRadius, plugTop}, {upperRadius, barrelTop},							// Формирует переход к широкой полости бочки
                   {-upperRadius, barrelTop}}, empty, MoldPart::plug);												// Выделяет модель заделки отдельной зоной
    drawing.box(-barrelRadius, barrelTop, barrelRadius, lowerTop, empty, MoldPart::barrel);							// Рисует только цилиндрическую полость бочки
    const double joinRadius = lowerRadius * (1 - 0.24 * (baseTop - lowerTop) / lower.height);						// Находит ширину нижней шейки на стыке
    drawing.shape({{-lowerRadius, lowerTop}, {lowerRadius, lowerTop}, {joinRadius, baseTop},						// Задаёт среднюю часть нижней шейки
                   {-joinRadius, baseTop}}, empty, MoldPart::lowerModelMiddle);										// Регистрирует среднюю часть нижней модели
    drawing.shape({{-joinRadius, baseTop}, {joinRadius, baseTop}, {lowerRadius * 0.76, floor},						// Задаёт нижнюю часть шейки
                   {-lowerRadius * 0.76, floor}}, empty, MoldPart::lowerModelBottom);								// Регистрирует нижнюю часть модели
																																							// Рисует контрольный стержень
    drawing.box(pipe - 172, funnelBottom, collar, baseTop, hatch, MoldPart::pipe);									// Рисует наружную стенку литниковой трубы
    for (double joint : {funnelBottom, floor * 0.27, floor * 0.56, baseTop}) {										// Расставляет крепления вдоль трубы
        drawing.box(pipe - 248, joint - 26, pipe + 248, joint + 26, hatch, MoldPart::pipe);							// Рисует кольцевой выступ трубы
    }																												// Завершает отрисовку креплений
    drawing.box(pipe - 125, funnelBottom, pipe + 125, baseTop, dots, MoldPart::pipe);								// Рисует прослойку литниковой трубы
    drawing.box(pipe - 64, funnelBottom, pipe + 64, baseTop, empty, MoldPart::pipe);								// Рисует вертикальный канал трубы
    drawing.shape({{pipe - 365, funnelTop}, {pipe + 365, funnelTop},												// Задаёт наружную верхнюю кромку воронки
                   {pipe + 125, funnelBottom}, {pipe - 125, funnelBottom}}, hatch, MoldPart::funnel);				// Рисует скошенные стенки воронки
    drawing.shape({{pipe - 303, funnelTop}, {pipe + 303, funnelTop},												// Задаёт широкое входное отверстие воронки
                   {pipe + 64, funnelBottom}, {pipe - 64, funnelBottom}}, empty, MoldPart::funnel);					// Соединяет воронку с литниковой трубой
    const double gate = floor - lower.height * 0.12;																// Располагает питатель немного выше дна
    const double turn = baseTop + lower.height * 0.28;																// Находит высоту разворота канала сифона
    const double halfGate = std::min(28.0, lower.height * 0.06);													// Сохраняет толщину канала при малой высоте
    const double lead = std::min(52.0, lower.height * 0.12);														// Ограничивает толщину колена при малой высоте
    const auto entry = [&](double y) {																				// Находит точку прохода в нижнюю шейку
        return lowerRadius * (1 - 0.24 * (y - lowerTop) / lower.height);											// Вычисляет радиус полости на указанной высоте
    };																												// Завершает расчёт положения входа
    const Location jointTop{pipe - 212, turn - lead / 2};															// Задаёт верх стыка колена с питателем
    const Location jointBottom{pipe - 204, turn + lead / 2};														// Задаёт низ стыка колена с питателем
    drawing.shape({{entry(gate - halfGate), gate - halfGate}, jointTop, jointBottom,								// Строит верх и правую грань питателя
                   {entry(gate + halfGate), gate + halfGate}}, empty, MoldPart::gate);								// Доводит питатель до нижней шейки
    const auto elbowPath = [&]() {																					// Строит один контур для изображения и указателя
        BeginPath(device);																							// Открывает криволинейный контур
        drawing.moveTo({pipe - 64, baseTop});																		// Начинает контур внутри трубы
        drawing.curveTo({pipe - 64, turn - lead}, {pipe - 142, turn - lead / 2}, jointTop);							// Рисует внутреннюю стенку колена
        drawing.lineTo(jointBottom);																				// Переходит к наружной стенке колена
        drawing.curveTo({pipe - 74, turn + lead / 2}, {pipe + 64, turn}, {pipe + 64, baseTop});						// Рисует наружную стенку колена
        CloseFigure(device);																						// Замыкает контур на конце трубы
        EndPath(device);																							// Завершает построение контура
    };																												// Завершает описание криволинейного элемента
    drawing.style(MoldPart::elbow, empty);																			// Подбирает подсветку колена
    elbowPath();																									// Готовит видимый контур колена
    StrokeAndFillPath(device);																						// Рисует стенки и внутренний канал
    elbowPath();																									// Повторяет контур для карты наведения
    if (rebuild) {																																			// Обновляет маску изогнутого литникового канала
        const HRGN region = PathToRegion(device);																											// Получает внутреннюю область колена
        hits.addFluid(region, false);																														// Добавляет колено к маске литников
        hits.add(region, MoldPart::elbow);																													// Регистрирует область для наведения
    }																																						// Завершает обновление колена
    const HBRUSH phaseBrushes[]{CreateSolidBrush(RGB(45, 112, 181)), CreateSolidBrush(RGB(242, 177, 46)), CreateSolidBrush(RGB(228, 76, 57)), CreateSolidBrush(RGB(236, 240, 243))};	// Задаёт цвета твёрдого двухфазного и жидкого металла
    const auto fillBox = [&](double left, double top, double right, double bottom, HBRUSH brush) {															// Закрашивает прямоугольник в координатах модели
        const POINT first = drawing.pixel({left, top});																										// Находит верхний левый угол
        const POINT last = drawing.pixel({right, bottom});																									// Находит нижний правый угол
        const RECT rectangle{first.x, first.y, last.x, last.y};																								// Собирает экранный прямоугольник
        FillRect(device, &rectangle, brush);																												// Окрашивает только разрешённую часть полости
    };																																						// Завершает вспомогательную заливку прямоугольника
    const bool mutedRunners = hovered != MoldPart::none && hovered != MoldPart::funnel && hovered != MoldPart::pipe && hovered != MoldPart::gate && hovered != MoldPart::elbow;	// Приглушает литники при выборе другой детали
    const HBRUSH runnerBrush = phaseBrushes[mutedRunners ? 3 : 2];																							// Сохраняет контраст выбранной литниковой детали
    if (hits.runners && frameState.filling > 0) {																											// Показывает продвижение металла по литникам
        const int clip = SaveDC(device);																													// Сохраняет область перед окраской каналов
        SelectClipRgn(device, hits.runners);																												// Исключает окраску стенок литниковой системы
        const double front = funnelTop + (turn + lead - funnelTop) * std::min(1.0, frameState.filling / 0.14);												// Продвигает металл вниз по трубе
        fillBox(pipe - 212, funnelTop, pipe + 400, front, runnerBrush);																						// Заполняет воронку трубу и колено
        if (frameState.filling > 0.14) {																													// Продолжает путь по наклонному питателю
            const double start = pipe - 212 - (pipe - 212 - entry(gate + halfGate)) * std::min(1.0, (frameState.filling - 0.14) / 0.06);					// Перемещает фронт от колена к отливке
            fillBox(start, turn - lead, pipe - 204, floor, runnerBrush);																					// Заполняет питатель перед подъёмом в отливке
        }																																					// Завершает движение по питателю
        RestoreDC(device, clip);																															// Восстанавливает ограничения рисования
    }																																						// Завершает окраску литников
    const double tops[]{lowerTop, barrelTop, upperTop, 0};																									// Хранит верхние отметки отделений
    const double bottoms[]{floor, lowerTop, barrelTop, upperTop};																							// Хранит нижние отметки отделений
    if (hits.casting) {																																		// Проверяет наличие геометрии отливки
        for (std::size_t section = 0; section < 4; ++section) {																								// Окрашивает текущее состояние каждого отделения
            const bool active = hovered == MoldPart::none || (section == 0 && (hovered == MoldPart::lowerModelMiddle || hovered == MoldPart::lowerModelBottom)) || (section == 1 && hovered == MoldPart::barrel) || (section >= 2 && (hovered == MoldPart::upperModel || hovered == MoldPart::plug));	// Выделяет металл только выбранного отделения
            const int clip = SaveDC(device);																												// Сохраняет область отделения
            SelectClipRgn(device, hits.casting);																											// Ограничивает окраску внутренней полостью отливки
            const double front = bottoms[section] - (bottoms[section] - tops[section]) * frameState.filled[section];										// Вычисляет уровень по объёму цилиндра
            const POINT first = drawing.pixel({-frame, front});																								// Находит верхнюю границу заполненного металла
            const POINT last = drawing.pixel({frame, bottoms[section]});																					// Находит нижнюю границу отделения
            IntersectClipRect(device, first.x, first.y, last.x, last.y);																					// Исключает ещё пустую часть формы
            fillBox(-frame, front, frame, bottoms[section], phaseBrushes[active ? 2 : 3]);																	// Сначала показывает металл жидким
            const auto* sample = frameState.samples[section];																								// Получает температурный профиль выбранного момента
            if (sample && frameState.filling >= 1) {																										// Включает фазовую окраску после заполнения
                for (std::size_t i = 1; i < sample->profile.size(); ++i) {																					// Обходит кольцевые элементы температурного профиля
                    const auto& inner = sample->profile[i - 1];																								// Получает внутренний узел кольца
                    const auto& outer = sample->profile[i];																									// Получает наружный узел кольца
                    const double temperature = (inner.temperature + outer.temperature) / 2;																	// Оценивает состояние тонкого радиального кольца
                    const int phase = temperature <= frameState.phase.solidus ? 0 : temperature >= frameState.phase.liquidus ? 2 : 1;						// Выбирает цвет геометрической зоны
                    fillBox(inner.radius * 1000, tops[section], outer.radius * 1000, bottoms[section], phaseBrushes[active ? phase : 3]);					// Окрашивает правую половину кольца
                    fillBox(-outer.radius * 1000, tops[section], -inner.radius * 1000, bottoms[section], phaseBrushes[active ? phase : 3]);					// Окрашивает левую половину кольца
                }																																			// Завершает окраску радиального профиля
            }																																				// Завершает проверку стадии охлаждения
            RestoreDC(device, clip);																														// Восстанавливает исходную область рисования
        }																																					// Завершает окраску четырёх отделений
    }																																						// Завершает проверку полости отливки
    for (const HBRUSH brush : phaseBrushes) { DeleteObject(brush); }																						// Освобождает кисти фазовой окраски
    drawing.box(-10, 0, 10, barrelTop + barrel.height * 0.22, fastener, MoldPart::rod);																		// Возвращает видимость сигнального стержня
    static_cast<void>(cursor);																																// Сохраняет совместимость аргумента положения указателя
    RestoreDC(device, saved);																						// Возвращает настройки контекста окна
    DeleteObject(mutedFill);																						// Освобождает осветлённую кисть
    DeleteObject(mutedOutline);																						// Освобождает осветлённое перо
    DeleteObject(fastener);																							// Освобождает кисть креплений
    DeleteObject(dots);																								// Освобождает кисть точечной заливки
    DeleteObject(hatch);																							// Освобождает кисть штриховки
    DeleteObject(outline);																							// Освобождает перо контуров
}																													// Завершает отрисовку разреза
