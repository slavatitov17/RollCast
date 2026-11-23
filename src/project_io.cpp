#define NOMINMAX	// Отключает конфликтующие макросы Windows
#include <windows.h>	// Предоставляет защищенную замену файла
#include "project_io.h"	// Подключает проект и рассчитанную историю
#include <fstream>	// Читает и записывает текстовые файлы
#include <iomanip>	// Сохраняет точность числовых параметров
#include <locale>	// Фиксирует десятичную точку файлового формата
#include <stdexcept>	// Передает ошибки открытия и сохранения
namespace {	// Скрывает внутренний механизм безопасной записи
template<class Writer>	// Принимает функцию заполнения нового документа
void atomicWrite(const std::filesystem::path& path, Writer writer) {	// Сохраняет документ без повреждения прежнего файла
    auto temporary = path;	// Создает временный файл рядом с целевым
    temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64()) + L".tmp";	// Различает временные файлы разных сохранений
    try {	// Обеспечивает уборку незавершенного документа
        std::ofstream stream(temporary, std::ios::binary);	// Открывает собственный временный файл
        stream.exceptions(std::ios::badbit | std::ios::failbit);	// Превращает ошибки записи в исключения
        stream.imbue(std::locale::classic());	// Использует переносимую запись чисел
        stream << std::setprecision(17);	// Сохраняет исходные числа без потери точности
        writer(stream); stream.close();	// Записывает и закрывает полный документ
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {	// Заменяет цель только готовым документом
            throw std::runtime_error("Не удалось сохранить файл");	// Сообщает об отказе окончательной записи
        }	// Завершает текущий блок обработки
    } catch (...) { DeleteFileW(temporary.c_str()); throw; }	// Удаляет только собственный незавершенный файл
}	// Завершает текущий блок обработки
}	// Завершает текущий блок обработки
Project loadProject(const std::filesystem::path& path) {	// Открывает проект пяти отделений монолитного валка
    std::ifstream stream(path, std::ios::binary);	// Поддерживает пути с русскими именами
    stream.imbue(std::locale::classic());	// Читает числа независимо от языка Windows
    std::string magic; int version = 0; stream >> magic >> version;	// Определяет принадлежность и версию документа
    if (magic != "RollCast" || (version != 2 && version != 3)) {	// Описывает параметры текущего действия
        throw std::runtime_error("Нужен файл .roll версии 2 или 3 для пяти отделений");	// Описывает параметры текущего действия
    }	// Завершает текущий блок обработки
    Project p;	// Подготавливает независимый кандидат проекта
    for (auto& d : p.dimensions) { stream >> d.diameter >> d.height; }	// Читает пары D1 L1 до D5 L5
    stream >> p.chill >> p.coating >> p.barrelFlask >> p.sand >> p.flask >> p.pipeDiameter;	// Восстанавливает размеры стенок и литниковой трубы
    for (auto& m : p.materials) { stream >> m.density >> m.heatCapacity >> m.conductivity; }	// Читает свойства четырех материалов
    stream >> p.phase.solidus >> p.phase.liquidus >> p.phase.latentHeat;	// Восстанавливает закон кристаллизации металла
    stream >> p.metalTemperature >> p.formTemperature >> p.airTemperature >> p.externalTransfer;	// Читает температуры и наружную теплоотдачу
    stream >> p.hours >> p.step >> p.diameterStep;	// Восстанавливает горизонт и численную сетку
    if (version == 3) { stream >> p.autoTime >> p.paintTemperature >> p.ironTemperature >> p.sandTemperature >> p.ironLiquidus >> p.ironSolidus; }	// Читает автоматическое время и раздельные температуры
    else { p.autoTime = false; p.paintTemperature = p.ironTemperature = p.sandTemperature = p.formTemperature; }	// Сохраняет смысл прежних файлов версии два
    if (!stream) { throw std::runtime_error("Проект содержит неполные или поврежденные данные"); }	// Отклоняет обрезанные документы
    validateProject(p); return p;	// Возвращает только физически согласованные входы
}	// Завершает текущий блок обработки
void saveProject(const std::filesystem::path& path, const Project& p, const Run* run) {	// Сохраняет проект и доступную итоговую сводку
    validateProject(p);	// Проверяет данные до создания документа
    atomicWrite(path, [&](std::ostream& stream) {	// Защищает существующий проект от ошибок записи
        stream << "RollCast 3\n";	// Описывает параметры текущего действия
        for (const auto& d : p.dimensions) { stream << d.diameter << ' ' << d.height << '\n'; }	// Сохраняет номера в порядке сверху вниз
        stream << p.chill << ' ' << p.coating << ' ' << p.barrelFlask << ' ' << p.sand << ' ' << p.flask << ' ' << p.pipeDiameter << '\n';	// Записывает толщины и диаметр трубы
        for (const auto& m : p.materials) { stream << m.density << ' ' << m.heatCapacity << ' ' << m.conductivity << '\n'; }	// Записывает плотность теплоемкость и теплопроводность
        stream << p.phase.solidus << ' ' << p.phase.liquidus << ' ' << p.phase.latentHeat << '\n';	// Сохраняет свойства фазового перехода
        stream << p.metalTemperature << ' ' << p.formTemperature << ' ' << p.airTemperature << ' ' << p.externalTransfer << '\n';	// Сохраняет тепловые начальные и граничные условия
        stream << p.hours << ' ' << p.step << ' ' << p.diameterStep << '\n';	// Сохраняет настройки численной схемы
        stream << p.autoTime << ' ' << p.paintTemperature << ' ' << p.ironTemperature << ' ' << p.sandTemperature << ' ' << p.ironLiquidus << ' ' << p.ironSolidus << '\n';	// Сохраняет настройки автоматического режима и оснастки
        if (run && run->project == p) {	// Добавляет сводку только соответствующего результата
            stream << "SUMMARY\n";	// Отмечает необязательную итоговую часть документа
            for (std::size_t i = 0; i < sectionCount; ++i) {	// Обходит все пять рассчитанных отделений
                const auto& h = run->cooling.section(i);	// Получает историю выбранного отделения
                const auto v = phaseVolumes(p.dimensions[i], h.samples.back());	// Вычисляет итоговые объемы обеих фаз
                stream << i + 1 << ' ' << v.solid << ' ' << v.liquid << ' ' << h.completedAt.value_or(-1) << '\n';	// Сохраняет объемы и достигнутое время затвердевания
            }	// Завершает текущий блок обработки
        }	// Завершает текущий блок обработки
    });	// Завершает защищенную запись проекта
}	// Завершает текущий блок обработки
