#ifndef IDENTIFY_EXPORT_MOCK_CHAR_H
#define IDENTIFY_EXPORT_MOCK_CHAR_H

// Forward declaration
class CharData;

// Create a minimal CharData for identify simulation
// identify_skill_level: The skill level for identify (0-200, or 400 for immortal)
CharData* CreateMockChar(int identify_skill_level);

// Cleanup mock character
void DestroyMockChar(CharData* ch);

#endif // IDENTIFY_EXPORT_MOCK_CHAR_H

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
