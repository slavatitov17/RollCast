#pragma once																			// Защищает заголовок от повторного подключения
#ifndef NOMINMAX																		// Проверяет настройку имен стандартных функций
#define NOMINMAX																		// Отключает макросы Windows для максимума и минимума
#endif																					// Завершает выбор настройки заголовка Windows
#include <windows.h>																	// Объявляет контекст рисования и прямоугольник Windows
#include <vector>																		// Хранит области подсказок в порядке рисования
#include <array>																		// Предоставляет массив отделений фиксированного размера
struct SectionDimensions {																// Хранит размеры одного цилиндрического отделения
    int diameter;																		// Задаёт диаметр в миллиметрах
    int height;																			// Задаёт высоту в миллиметрах
};																						// Завершает описание размеров отделения
using MoldDimensions = std::array<SectionDimensions, 4>;								// Упорядочивает отделения формы снизу вверх
inline constexpr MoldDimensions demoDimensions{{										// Задаёт условные исходные размеры
    {520, 820},																			// Нижняя шейка
    {920, 1310},																		// Бочка валка
    {560, 765},																			// Верхняя шейка
    {460, 704},																			// Прибыль
}};																						// Завершает набор демонстрационных размеров
enum class MoldPart {																	// Перечисляет зоны разреза с подсказками
    none,																				// Обозначает свободное пространство вокруг формы
    riser, upperModel, funnel, pipe, upperFrame, rod,									// Перечисляет верхние элементы формы и сифона
    plug, chill, barrel, lowerModelMiddle, middleFrame,									// Перечисляет элементы вокруг бочки
    lowerModelBottom, flask, plate, gate, elbow											// Перечисляет нижние элементы литниковой системы
};																						// Завершает перечень элементов формы
class MoldHitMap {																		// Хранит точные экранные области нарисованных элементов
public:																					// Открывает операции рисования и выбора части формы
    MoldHitMap() = default;																// Создаёт пустую карту областей
    MoldHitMap(const MoldHitMap&) = delete;												// Запрещает копирование дескрипторов областей
    MoldHitMap& operator=(const MoldHitMap&) = delete;									// Запрещает совместное владение областями
    ~MoldHitMap();																		// Освобождает области при закрытии окна
    void clear();																		// Удаляет области предыдущего кадра
    void add(HRGN region, MoldPart part);												// Принимает область нарисованного элемента
    MoldPart partAt(POINT cursor) const;												// Возвращает верхний элемент под указателем
private:																				// Скрывает детали хранения областей Windows
    struct Area {																		// Связывает экранную область с частью формы
        HRGN region;																	// Хранит дескриптор области Windows
        MoldPart part;																	// Хранит имя выбранной зоны
    };																					// Завершает описание экранной области
    std::vector<Area> areas;															// Хранит области в порядке рисования
};																						// Завершает описание карты подсказок
void drawMold(HDC device, const RECT& area, const MoldDimensions& dimensions,			// Получает область построения формы
    MoldHitMap& hits, MoldPart hovered, POINT cursor);									// Получает параметры подсветки и подсказки
