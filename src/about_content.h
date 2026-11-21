#pragma once	// Защищает содержание справки от повторного подключения
#include <string>	// Хранит русский текст и математические обозначения
#include <vector>	// Сохраняет последовательность разделов теории
enum class HelpKind { heading, paragraph, formula, code };	// Различает типографику элементов справки
struct HelpBlock { HelpKind kind; std::wstring text; };	// Связывает содержание с его оформлением
std::vector<HelpBlock> aboutContent(bool calculations);	// Возвращает общие сведения либо теорию
