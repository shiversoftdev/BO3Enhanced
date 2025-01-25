#pragma once
#include <stdint.h>

extern uint64_t gSteamID;
extern const char* gSteamPlayername;
extern bool gSteamInit;
extern uint64_t gLobbyID;

void init_steamapi();