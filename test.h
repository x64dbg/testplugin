#ifndef _TEST_H
#define _TEST_H

#include "pluginmain.h"

//menu identifiers
#define MENU_DUMP 0
#define MENU_TEST 1
#define MENU_SELECTION 2

//functions
void testInit(PLUG_INITSTRUCT* initStruct);
void testStop();
void testSetup();

#endif // _TEST_H
