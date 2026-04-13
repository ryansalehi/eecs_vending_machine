#ifndef NAME_ROSTER_H
#define NAME_ROSTER_H

#include "ctype.h"
#include "string.h"
#include <stdint.h>

//pass in the string from the magstripe
//returns 1 if admin
//returns 2 if student
//returns 3 if not authorized
int auth(char * message_from_mag, char * name_for_LCD);

// helper function used by auth() to
// check the authentication level of the user
uint8_t which_user(char * message_for_LCD);

#endif
