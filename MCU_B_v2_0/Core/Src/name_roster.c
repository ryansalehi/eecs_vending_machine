#include "name_roster.h"

static const char *student_roster[] = {
    "shah", "guzman", "perkovic", "nelson", "gamilla", "edlund", "layman",
    "raffensberger", "whitten", "park", "senerpida", "cheung", "nguyen",
    "zhang", "hadi", "ying", "qian", "kocheril", "ahn", "sagui", "tkaczyk",
    "james", "cui", "deng", "he", "krishna", "jin", "rong", "yan", "windsor",
    "kushel", "story", "mostafa", "castillo", "gollapalli", "olejnik", "samba",
    "ortega", "qu", "seifferly", "leonard", "khatri", "wozniak", "bichala",
    "lin", "cooley", "maglione", "duncan", "jacob", "springsteen", "jackson",
    "islam", "chen", "khadka", "conforti", "narayanan", "menon", "shao",
    "mittal", "herz", "stifter", "lamay", "davis", "barbat", "pitam", "ke",
    "prabhu", "boone", "mcphee", "arunchunaikani", "capitani", "ondrus", "song",
    "kennedy", "bafna", "comerford", "johnson", "truntaev", "ganesh", "tai",
    "ullman", "karim", "vogel", "yang", "shi", "chopra", "choudhury", "noronha",
    "upadhyay", "unadkat", "bakker", "brown", "murphy", "thornton",
    "schmalenberg", "tilwankar", "nimmagadda", "vilayan", "valecha", "wong",
    "vandevoorde", "carlson", "weingarden", "hairston", "ko", "ranjan", "la",
    "rossi", "zhao", "brehob", "salehi", "chang",
};

static const char *admin_roster[] = {
    "greco", "pickos", "strayhorn", "zhu",
    "carl", "mccloskey", "akhmatdinov", "desai", "koduru", "lee",
};

#define STUDENT_COUNT (sizeof(student_roster) / sizeof(student_roster[0]))
#define ADMIN_COUNT   (sizeof(admin_roster)   / sizeof(admin_roster[0]))

int auth(char * message_from_mag, char * name_for_LCD)
{
	uint8_t cursor;;
    memset(name_for_LCD, 0, 16);
	//fast forward until you hit the '/' character
	//this should mark the end of the user's last name
	uint8_t found = 0;
	for(uint8_t i = 0; i < 255; ++i)
	{
		if(message_from_mag[i] == '/')
		{
			cursor = ++i;
			found = 1;
			break;
		}
	}

	//if we never hit '/'
	if (!found)
		return 3;

	//reset the flag
	found = 0;

	//now rewind until you hit a digit
	uint8_t steps_back = 0;
	for(uint8_t i = 0; i < 255; ++i)
	{
		if(isdigit(message_from_mag[cursor]))
		{
			found = 1;
			break;
		}
		--cursor;
		steps_back++;
	}

	if(!found)
		return 3;

	uint8_t name_length = (steps_back - 2) / 2;
	uint8_t start_of_name = cursor + 1; //

	if (name_length > 15)
		return 3;

	for(uint8_t i = 0; i < name_length ; i++)
	{
		name_for_LCD[i] = message_from_mag[start_of_name + (2*i)];
	}

	name_for_LCD[name_length] = '\0';
	return which_user(name_for_LCD);
}

uint8_t which_user(char * message_for_LCD)
{
	for(uint8_t i = 0; i < STUDENT_COUNT; ++i)
	{
		if (strcmp(student_roster[i], message_for_LCD) == 0)
		{
			// user is a student
			return 1;
		}
	}
	for(uint8_t i = 0; i < ADMIN_COUNT; ++i)
	{
		if(strcmp(admin_roster[i], message_for_LCD) == 0)
		{
			// user is an admin
			return 2;
		}
	}
	// user not authorized
	return 3;
}






