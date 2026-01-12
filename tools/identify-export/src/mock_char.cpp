#include "mock_char.h"
#include "engine/entities/char_data.h"
#include "gameplay/skills/skills.h"
#include <memory>

// Create a minimal CharData for identify simulation
CharData* CreateMockChar(int identify_skill_level)
{
	CharData* ch = new CharData();

	// Set identify skill level
	ch->set_skill(ESkill::kIdentify, identify_skill_level);

	// Set minimal required fields
	// No descriptor needed - MessageCapture will intercept output

	return ch;
}

void DestroyMockChar(CharData* ch)
{
	if (ch)
	{
		delete ch;
	}
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
