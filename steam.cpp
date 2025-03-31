#include "framework.h"
#include "steam/steam_api.h"
#include "SteamCBHandler.h"
#include "bdAuthXB1toSteam.h"
#include "steamugc.h"
#include "security.h"
#include <unordered_map>
#include <intrin.h>
#include <direct.h>

enum SteamAuthStatus
{
	SteamAuthStatus_Unauthorized = 0,
	SteamAuthStatus_ReceivedPending = 1,
	SteamAuthStatus_OK = 2
};

struct SteamAuthInfo
{
	SteamAuthStatus status;
	uint64_t timeout;
	uint64_t retry;
	uint32_t retry_count;
};

void check_steamauth();

// the steam information fetched upon init
uint64_t gSteamID = 0;
const char* gSteamPlayername = NULL;
bool gSteamInit = false;
uint64_t gLobbyID = 0;

// are our stats dirty
bool gSteamStatsDirty = false;

// the original XUID of the user if GDK stuff is in use
uint64_t gOriginalXUID = 0;

// global handle for XUser stuff
void** gGlobalXUserHandle = (void**)REBASE(0x148f76c0);

// replace the XUID with the SteamID64
MDT_Define_FASTCALL(REBASE(0x29222c8), XalUserGetId_hook, int, (void* xuser, uint64_t* output))
{
	// this is only ever used to reference the current user and is used before gGlobalXUserHandle is populated
	// NOTE(Emma): if Xbox stuff is still enabled we want to make sure any Xal functions use the original XUID
#ifdef USES_GDK
	// if we don't have our original xuid already, make it so
	if (gOriginalXUID == 0) {
		MDT_ORIGINAL(XalUserGetId_hook, (xuser, &gOriginalXUID));
		ALOG("Original XUID: %llu\n", gOriginalXUID);
	}
	// only return the SteamID64 for 
	// TODO(Emma): figure out if this is good enough or if this still breaks dw
	if (*gGlobalXUserHandle == NULL || xuser == *gGlobalXUserHandle)
		*output = gSteamID;
	else {
		ALOG("Forwarding XUID to original instead of SteamID64");
		return MDT_ORIGINAL(XalUserGetId_hook, (xuser, output));
	}
#else
	// xbl is disabled, return steamid
	*output = gSteamID;
#endif
	return 0;
}

// replace the username with the Steam persona name
MDT_Define_FASTCALL(REBASE(0x2922220), XalUserGetGamertag_hook, int, (void *xuser, int component, size_t gt_size, char *output, size_t *output_size))
{
	// since this function is used a lot and i can't be bothered checking if everywhere is referencing the current user
	// if it isn't referencing our global user then call into the original function
	if (xuser != *gGlobalXUserHandle)
		return MDT_ORIGINAL(XalUserGetGamertag_hook, (xuser, component, gt_size, output, output_size));

	if (*security::conf.playername)
	{
		strncpy(output, security::conf.playername, gt_size);
		*output_size = min(strlen(security::conf.playername), gt_size);
	}
	else
	{
		strncpy(output, gSteamPlayername, gt_size);
		*output_size = min(strlen(gSteamPlayername), gt_size);
	}
	return 0;
}

// the packet "format" for a dwInstantMessage sent via Steam P2P
typedef struct _dwInstantMsgSteamPacket {
	int id;
	uint8_t instantMsg[0x400];
	int instantMsgLength;
} dwInstantMsgSteamPacket;

// since we can't dispatch the message in our own thread,
// keep track of the most recent instant message to dispatch on the game thread
bool instantPacketReady = false;
CSteamID instantPacketFrom;
dwInstantMsgSteamPacket instantPacket = { 0 };

#define dwInstantDispatchMessage(fromId, controllerIndex, data, size) ((void(__fastcall*)(uint64_t, int, uint8_t *, int))REBASE(0x14f6710))(fromId, controllerIndex, data, size)

// we run this every frame in the game thread
void steam_dispatch_every_frame() 
{
	if (instantPacketReady) 
	{
		dwInstantDispatchMessage(instantPacketFrom.ConvertToUint64(), 0, instantPacket.instantMsg, instantPacket.instantMsgLength);
		instantPacketReady = false;
	}

	if (SteamCBHandler()->hasPendingInvite)
	{
		SteamCBHandler()->hasPendingInvite = false;

		char xuidStr[64]{ 0 }; // <3
		sprintf(xuidStr, "join %llu\n", SteamCBHandler()->pendingInvite.ConvertToUint64());
		Cbuf_AddText(xuidStr);
	}

	check_steamauth();

	if (g_needsUpdatePrompt)
	{
		g_needsUpdatePrompt = false;
		Com_Error(ERROR_UI, "A new update has been installed for the Windows Store Fix. Please restart the game to finish installing changes.");
	}
}

void parse_steam_p2p_msg(CSteamID from, uint8_t* buf, uint32_t size) {
	// TODO(Emma): implement, might be needed for lobbies with Steam hosts
	if (buf[0] == 0x16) // auth ticket request
	{
		ALOG("unimplemented p2p steam auth request 0x16");
	}
	else if (buf[0] == 0x15) // recv auth ticket
	{
		ALOG("unimplemented p2p steam auth request 0x15");
	}
	// handle instant messages
	else if (buf[0] == 0x28 && size == sizeof(dwInstantMsgSteamPacket)) {
		while (instantPacketReady) {
			Sleep(1); // wait for the ready state to be set to false
			// NOTE(Emma): might be a bad idea since we'd be blocking our steam callbacks thread...
		}
		memcpy(&instantPacket, buf, sizeof(instantPacket));
		instantPacketFrom = from;
		instantPacketReady = true;
	}
	else
	{
		ALOG("unimplemented invalid packet");
	}
}

void get_script_checksum(int scriptInst, uint32_t buf[3])
{
	auto off = 120llu * scriptInst;
	buf[0] = *(uint32_t*)(REBASE(0x3EE6810) + off + 72);
	buf[1] = *(uint32_t*)(REBASE(0x3EE6810) + off + 76);
	buf[2] = *(uint32_t*)(REBASE(0x3EE6810) + off + 80);
}

bool hasCachedTicket = false;
HAuthTicket cachedTicket = 0;
uint32_t cachedTicketLength = 0;
char cachedTicketBuf[0x1001]{ 0 };
void SteamAuth_HandleRequest(uint32_t controller, XUID senderXUID, uint8_t forceNew)
{
	uint32_t script_checksums[3];
	char msgBuf[2048];
	msg_t reply;

	get_script_checksum(1, script_checksums);
	MSG_Init(&reply, msgBuf, sizeof(msgBuf));
	MSG_WriteString(&reply, "steamAuth");
	MSG_WriteInt64(&reply, GetXUID(0));
	MSG_WriteInt64(&reply, script_checksums[0]);

	if (!controller)
	{
		if (forceNew && hasCachedTicket)
		{
			hasCachedTicket = false;
			SteamUser()->CancelAuthTicket(cachedTicket);
			cachedTicketLength = cachedTicket = 0;
			memset(cachedTicketBuf, 0, sizeof(cachedTicketBuf));
		}

		cachedTicket = SteamUser()->GetAuthSessionTicket(cachedTicketBuf, sizeof(cachedTicketBuf) - 1, &cachedTicketLength, NULL);
		MSG_WriteUInt16(&reply, (uint16_t)cachedTicketLength);
		MSG_WriteData(&reply, cachedTicketBuf, (uint16_t)cachedTicketLength);
	}
	else
	{
		MSG_WriteUInt16(&reply, 0);
	}
	
	netadr_t host = *(netadr_t*)((*(uint64_t*)(REBASE(0x40A9288)) + 0x25780llu * controller) + 16);
	NET_OutOfBandData(NS_CLIENT1, host, reply.data, reply.cursize);
}

std::unordered_map<XUID, SteamAuthInfo> auth_cache;
void SteamAuth_HandleResponse(msg_t* msg, XUID senderXUID, uint16_t length, uint64_t scriptChecksum)
{
	uint32_t script_checksums[3];
	char ticketBuf[0x1001]{ 0 };

	if (length >= sizeof(ticketBuf))
	{
		ALOG("REJECTED AUTH RESPONSE: %llu sent a buffer too big to be processed", senderXUID);
		return;
	}

	// known clients will be placed in this cache when they join the match
	if (auth_cache.find(senderXUID) == auth_cache.end())
	{
		return;
	}

	// check that we need their auth in the first place
	if (auth_cache[senderXUID].status == SteamAuthStatus_OK)
	{
		ALOG("IGNORED AUTH RESPONSE: %llu doesnt need an auth response", senderXUID);
		return;
	}
	
	// check that script checksum is valid
	get_script_checksum(1, script_checksums);

	if (script_checksums[0] != (uint32_t)scriptChecksum)
	{
		ALOG("REJECTED AUTH RESPONSE: %llu sent an invalid checksum", senderXUID);
		return;
	}

	MSG_ReadData(msg, ticketBuf, length);
	auto res = SteamUser()->BeginAuthSession(ticketBuf, length, senderXUID);

	if (res != k_EBeginAuthSessionResultOK)
	{
		ALOG("REJECTED AUTH RESPONSE: %llu sent an invalid ticket", senderXUID);
		return;
	}

	if (auth_cache[senderXUID].status < SteamAuthStatus_ReceivedPending)
	{
		ALOG("AUTH RESPONSE: %llu sent a valid ticket, waiting on identity verification", senderXUID);
		auth_cache[senderXUID].status = SteamAuthStatus_ReceivedPending;
	}
}

void OnGotAuthTicketResponse(ValidateAuthTicketResponse_t* pCallback)
{
	auto xuid = (XUID)pCallback->m_SteamID.ConvertToUint64();
	if (pCallback->m_eAuthSessionResponse != k_EAuthSessionResponseOK)
	{
		ALOG("AUTH TICKET INVALID FOR %llu, reason %u", xuid, pCallback->m_eAuthSessionResponse);
		
		if (auth_cache[xuid].status != SteamAuthStatus_OK)
		{
			auth_cache[xuid].status = SteamAuthStatus_Unauthorized;
		}
		return;
	}

	ALOG("STEAM AUTHORIZATION COMPLETED FOR %llu", xuid);
	auth_cache[xuid].status = SteamAuthStatus_OK;
}

uint64_t next_steamauth_check = 0;
void check_steamauth()
{
	// dont check steamauth in UI level
	if (s_runningUILevel)
	{
		return;
	}

	// only tick if we are the host
	if (!HasHost())
	{
		return;
	}

	// only tick at an interval
	if (GetTickCount64() < next_steamauth_check)
	{
		return;
	}

	next_steamauth_check = GetTickCount64() + STEAMAUTH_TICK_DELAY;
	char msgBuf[2048];

	LobbyType sessionType = LOBBY_TYPE_PRIVATE;
	if (LobbySession_GetControllingLobbySession(LOBBY_MODULE_CLIENT))
	{
		sessionType = LOBBY_TYPE_GAME;
	}

	auto session = LobbySession_GetSession(sessionType);
	for (int i = 0; i < 18; i++)
	{
		// get client info for this index
		auto client = LobbySession_GetClientByClientNum(session, i);

		// if not active client, skip
		if (!client->activeClient)
		{
			continue;
		}

		// if the client is the host or the host's splitscreen player, skip
		auto xuid = client->activeClient->fixedClientInfo.xuid;
		if (xuid == GetXUID(0) || xuid == GetXUID(1))
		{
			continue;
		}

		// cant steamauth anyone that isnt a real steam account
		if (CSteamID(xuid).GetEAccountType() != k_EAccountTypeIndividual)
		{
			continue;
		}

		// if the client has no authinfo, we need to send them an auth request
		if (auth_cache.find(xuid) == auth_cache.end())
		{
			auth_cache[xuid] = SteamAuthInfo();
			auth_cache[xuid].status = SteamAuthStatus_Unauthorized;
			auth_cache[xuid].timeout = GetTickCount64() + STEAMAUTH_FAIL_TIMEOUT;
			auth_cache[xuid].retry = 0;
			auth_cache[xuid].retry_count = 0;
			
			goto send_auth;
		}

		// client already authed, skip
		if (auth_cache[xuid].status == SteamAuthStatus_OK)
		{
			continue;
		}

		// if client took too long, kick them
		if (auth_cache[xuid].timeout <= GetTickCount64())
		{
			auth_cache.erase(xuid);

			ALOG("STEAMAUTH TIMEOUT FOR %llu, KICKING", xuid);

			memset(msgBuf, 0, sizeof(msgBuf));
			sprintf_s(msgBuf, "xuidkick_for_reason %llu PLATFORM_STEAM_CONNECT_FAIL\n", xuid);
			Cbuf_AddText(msgBuf);

			continue;
		}

		// not ready to retry yet
		if (auth_cache[xuid].retry > GetTickCount64())
		{
			continue;
		}

		send_auth:
		{
			ALOG("STEAMAUTH: sending %llu a steamauth request, attempt %u", xuid, auth_cache[xuid].retry_count + 1);

			msg_t msg;
			memset(msgBuf, 0, sizeof(msgBuf));
			MSG_Init(&msg, msgBuf, sizeof(msgBuf));

			MSG_WriteString(&msg, "steamAuthReq");
			MSG_WriteInt64(&msg, GetXUID(0));

			// if we arent on a multiple of retry delays that coincides with steamauth delay time, allow an old ticket to be sent again
			if ((auth_cache[xuid].retry_count * STEAMAUTH_CLIENT_RETRY_DELAY) % STEAMAUTH_DELAY_NEW_TIME)
			{
				MSG_WriteBit0(&msg);
			}
			else
			{
				MSG_WriteBit1(&msg);
			}

			auto adr = LobbySession_GetClientNetAdrByIndex(sessionType, i);
			NET_OutOfBandData(NS_SERVER, adr, msg.data, msg.cursize);
			auth_cache[xuid].retry = GetTickCount64() + STEAMAUTH_CLIENT_RETRY_DELAY;
			++auth_cache[xuid].retry_count;

			// force the next steamauth check to be 1 second from now and break the loop such that we dont send multiple clients auth requests within the same second
			next_steamauth_check = GetTickCount64() + 1000;
			break;
		}
	}
}

MDT_Define_FASTCALL(REBASE(0x16579F0), Unk_G_InitGame_Sub16579F0_hook, uint64_t, ())
{
	auth_cache.clear();
	return MDT_ORIGINAL(Unk_G_InitGame_Sub16579F0_hook, ());
}

MDT_Define_FASTCALL(REBASE(0x13F7750), CL_DispatchConnectionlessPacket_hook, uint32_t, (uint32_t controller, netadr_t from, msg_t* msg))
{
	int* v7 = *(int**)(*(uint64_t*)(*(uint64_t*)__readgsqword(0x58u) + 744llu) + 24llu);
	auto firstArg = "";
	if (v7[*v7 + 25] > 0)
	{
		firstArg = **(const CHAR***)&v7[2 * *v7 + 34];
	}

	if (!istrcmp_("steamAuthReq", firstArg))
	{
		XUID serverID = MSG_ReadInt64(msg);
		uint8_t forceNew = MSG_ReadBit(msg);
		ALOG("Handling steamAuthReq: %llu %u", serverID, forceNew);
		SteamAuth_HandleRequest(controller, serverID, forceNew);
		return 1;
	}

	return MDT_ORIGINAL(CL_DispatchConnectionlessPacket_hook, (controller, from, msg));
}

MDT_Define_FASTCALL(REBASE(0x23249E0), SV_ConnectionlessPacket_hook, void, (netadr_t from, msg_t* msg))
{
	char strBuf[1024]{ 0 };
	msg_t cpy = *msg;

	MSG_ReadInt32(&cpy);
	MSG_ReadStringLine(&cpy, strBuf, sizeof(strBuf));

	if (!istrcmp_("steamauth", strBuf))
	{
		XUID sender  = MSG_ReadInt64(&cpy);
		uint64_t scriptChecksum = MSG_ReadInt64(&cpy);
		uint16_t length = MSG_ReadInt16(&cpy);
		ALOG("Handling steamauth response: %llu %u len(%u)", sender, scriptChecksum, (uint32_t)length);

		if (length)
		{
			
			SteamAuth_HandleResponse(&cpy, sender, length, scriptChecksum);
		}
		else
		{
			ALOG("REJECTED AUTH RESPONSE: %llu sent an empty buffer", sender);
		}

		return;
	}

	MDT_ORIGINAL(SV_ConnectionlessPacket_hook, (from, msg));
}

DWORD steam_callbacks_thread(void *arg)
{
	while (true)
	{
		SteamAPI_RunCallbacks();
		Sleep(33);
		// check for steam networking incoming packets
		{
			uint32_t incomingPacketSize = 0;
			uint8_t incomingPacketBuf[0x800] = { 0 };
			CSteamID incomingPacketFrom;
			if (SteamNetworking()->IsP2PPacketAvailable(&incomingPacketSize))
			{
				// if the message is too big then discard it
				if (incomingPacketSize > sizeof(incomingPacketBuf)) {
					SteamNetworking()->ReadP2PPacket(incomingPacketBuf, sizeof(incomingPacketBuf), &incomingPacketSize, &incomingPacketFrom);
					ALOG("discarding oversized message from %llu", incomingPacketFrom.ConvertToUint64());
				}
				else {
					SteamNetworking()->ReadP2PPacket(incomingPacketBuf, sizeof(incomingPacketBuf), &incomingPacketSize, &incomingPacketFrom);
					ALOG("steam user %llu sent us 0x%x bytes", incomingPacketFrom.ConvertToUint64(), incomingPacketSize);
					parse_steam_p2p_msg(incomingPacketFrom, incomingPacketBuf, incomingPacketSize);
				}
			}
		}
		// store our stats if they're dirty
		if (gSteamStatsDirty) {
			ALOG("storing stats");
			SteamUserStats()->StoreStats();
			gSteamStatsDirty = false;
		}
	}
	return 0;
}

// achievement unlocking hook
MDT_Define_FASTCALL(REBASE(0x1f04fd0), Live_AwardAchievement_Hook, void, (int controllerIndex, char *achievementName))
{
	ALOG("unlocking %s on Steam", achievementName);
	SteamUserStats()->SetAchievement(achievementName);
	gSteamStatsDirty = true;
#ifdef USES_GDK
	MDT_ORIGINAL(Live_AwardAchievement_Hook, (controllerIndex, achievementName));
#endif
}

MDT_Define_FASTCALL(REBASE(0x2cb6430), XblAchievementsUpdateAchievementAsync_Hook, int, (void* xblContext, uint64_t xuid, char* name, int progress, void* async))
{
#ifdef USES_GDK
	ALOG("unlocking %s on Xbox Live for %llu", name, gOriginalXUID);
	return MDT_ORIGINAL(XblAchievementsUpdateAchievementAsync_Hook, (xblContext, gOriginalXUID, name, progress, async));
#else
	return 0;
#endif
}

MDT_Define_FASTCALL(REBASE(0x1EEDBE0), ShowPlatformProfile_hook, void, (XUID xuid)) // <3
{
	if ((uint64_t)_ReturnAddress() == REBASE(0x201F558))
	{
		SteamFriends()->ActivateGameOverlayToUser("friendadd", CSteamID(xuid));
		return;
	}
	SteamFriends()->ActivateGameOverlayToUser("steamid", CSteamID(xuid));
}

MDT_Define_FASTCALL(REBASE(0x292049C), XGameSaveFilesGetFolderWithUiResult_hook, uint64_t, (uint64_t a1, size_t folderSize, char* result))
{
	if (!_getcwd(result, folderSize))
	{
		return MDT_ORIGINAL(XGameSaveFilesGetFolderWithUiResult_hook, (a1, folderSize, result));
	}
	return 0;
}

typedef struct _PresenceData {
	char unk1[0x8];
	uint64_t XUID;
	char unk2[0x30];
	int state;
	int activity;
	int ctx;
	int joinable;
	int gametypeID;
	int mapID;
	int difficulty;
	int playlist;
	char unk_[0x318];
} PresenceData;

typedef struct _FriendInfo {
	char name[17];
	char pad[0x8];
	PresenceData presence;
	int lastFetchedTime;
	char unk[0x4C];
} FriendInfo;
int sizeofFriendInfo = sizeof(FriendInfo); // 1000

typedef struct _FriendsListTime {
	int all;
	int online;
	int title;
} FriendsListTime;

typedef struct _SortInfo {
	int index;
	FriendInfo* friendInfo;
} SortInfo;

typedef struct _FriendsListSorting {
	SortInfo online[100];
	SortInfo alphabetical[100];
	SortInfo* current;
} FriendsListSorting;

typedef struct _FriendsList {
	FriendInfo* friends;
	int numFriends;
	int maxNumFriends;
	FriendsListTime requestTime;
	FriendsListTime fetchTime;
	FriendsListSorting sorting;
} FriendsList;

static void fill_in_friend(CSteamID steamID, FriendInfo* info)
{
	memset(info, 0, sizeof(info));
	uint64_t uintsteam = steamID.ConvertToUint64();

	// keep the name handy for parsing
	char tempName[0x80];
	strncpy(tempName, SteamFriends()->GetFriendPersonaName(steamID), sizeof(tempName));

	// fill the name and steamid in the friendinfo struct
	strncpy(info->name, tempName, 0x10);
	info->name[0x10] = 0;
	info->presence.XUID = steamID.ConvertToUint64();

	*(int*)info->presence.unk_ = time(NULL);
	info->presence.unk_[4] = 0x12;

	if (SteamFriends()->GetFriendPersonaState(steamID) == k_EPersonaStateOffline)
	{
		info->presence.state = 0x08;
		info->presence.activity = 0x00; // PRESENCE_ACTIVITY_OFFLINE
		info->presence.gametypeID = 0xFFFFFFFF;
		info->presence.mapID = 0xFFFFFFFF;
		return;
	}

	FriendGameInfo_t currentGame;
	// check if user is playing BO3 (GetFriendGamePlayed fails = no game, PS3 mode engaged)
	if (!SteamFriends()->GetFriendGamePlayed(steamID, &currentGame) ||
		currentGame.m_gameID.AppID() != 311210) {
		info->presence.state = 0x04;
		info->presence.activity = 0x01; // PRESENCE_ACTIVITY_ONLINE_NOT_IN_TITLE
		info->presence.gametypeID = 0xFFFFFFFF;
		info->presence.mapID = 0xFFFFFFFF;
		return;
	}

	info->presence.state = 0x04;
	info->presence.activity = 0x07; // PRESENCE_ACTIVITY_IN_TITLE

	// they are playing BO3 so get rich presence data
	const char* state_str = SteamFriends()->GetFriendRichPresence(steamID, "state");
	const char *tActivity_str = SteamFriends()->GetFriendRichPresence(steamID, "tActivity");
	const char* tCtx_str = SteamFriends()->GetFriendRichPresence(steamID, "tCtx");
	const char* tJoinable_str = SteamFriends()->GetFriendRichPresence(steamID, "tJoinable");
	const char* tGametype_str = SteamFriends()->GetFriendRichPresence(steamID, "tGametype");
	const char* tMapId_str = SteamFriends()->GetFriendRichPresence(steamID, "tMapId");
	const char* tDifficulty_str = SteamFriends()->GetFriendRichPresence(steamID, "tDifficulty");
	const char* tPlaylist_str = SteamFriends()->GetFriendRichPresence(steamID, "tPlaylist");

	// blank state string assume presence isn't valid
	if (strlen(state_str) == 0)
		return;

	// otherwise fill in our struct
	//info->presence.state |= atoi(state_str);
	info->presence.activity = atoi(tActivity_str);
	info->presence.ctx = atoi(tCtx_str);
	info->presence.joinable = atoi(tJoinable_str);
	info->presence.gametypeID = atoi(tGametype_str);
	info->presence.mapID = atoi(tMapId_str);
	info->presence.difficulty = atoi(tDifficulty_str);
	info->presence.playlist = atoi(tPlaylist_str);
}

MDT_Define_FASTCALL(REBASE(0x1ee7610), LiveFriends_Update_hook, void, (void))
{
	FriendsList* friendsList = (FriendsList*)REBASE(0x148f1510);
	// clear out some of the friends list struct
	friendsList->numFriends = 0;
	int allowedFriends = SteamFriends()->GetFriendCount(k_EFriendFlagImmediate);
	if (allowedFriends > friendsList->maxNumFriends)
		allowedFriends = friendsList->maxNumFriends;
	// enumerate through all steam friends
	for (int i = 0; i < allowedFriends; i++)
	{
		int idx = friendsList->numFriends;
		CSteamID cppDoesntLetYouSayFriend = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagImmediate);
		fill_in_friend(cppDoesntLetYouSayFriend, &friendsList->friends[idx]);
		friendsList->sorting.alphabetical[idx].index = idx;
		friendsList->sorting.alphabetical[idx].friendInfo = &friendsList->friends[idx];
		friendsList->sorting.online[idx].index = idx;
		friendsList->sorting.online[idx].friendInfo = &friendsList->friends[idx];
		friendsList->numFriends++;
	}
}


MDT_Define_FASTCALL(REBASE(0x1f95bd0), LivePresence_UpdatePlatform_hook, void, (int controller, PresenceData* presence))
{
	// set stuff we can't rip out of the presence blob
	SteamFriends()->SetRichPresence("ref", "PRESENCE_NOTINGAME"); // never properly set
	SteamFriends()->SetRichPresence("params", ""); // unsure, never seen it different
	SteamFriends()->SetRichPresence("status", "Main Menu"); // "Main Menu", "Multiplayer", "Zombies" etc
	SteamFriends()->SetRichPresence("version", "1"); // unsure, always 1

	// and set the stuff we can!
	static char stateBuf[14];
	static char tActivityBuf[14];
	static char tCtxBuf[14];
	static char tJoinableBuf[14];
	static char tGametypeBuf[14];
	static char tMapIdBuf[14];
	static char tDifficultyBuf[14];
	static char tPlaylistBuf[14];
	snprintf(stateBuf, sizeof(stateBuf), "%i", presence->state);
	snprintf(tActivityBuf, sizeof(tActivityBuf), "%i", presence->activity);
	snprintf(tCtxBuf, sizeof(tCtxBuf), "%i", presence->ctx);
	snprintf(tJoinableBuf, sizeof(tJoinableBuf), "%i", presence->joinable);
	snprintf(tGametypeBuf, sizeof(tGametypeBuf), "%i", presence->gametypeID);
	snprintf(tMapIdBuf, sizeof(tMapIdBuf), "%i", presence->mapID);
	snprintf(tDifficultyBuf, sizeof(tDifficultyBuf), "%i", presence->difficulty);
	snprintf(tPlaylistBuf, sizeof(tPlaylistBuf), "%i", presence->playlist);
	SteamFriends()->SetRichPresence("state", stateBuf);
	SteamFriends()->SetRichPresence("tActivity", tActivityBuf);
	SteamFriends()->SetRichPresence("tCtx", tCtxBuf);
	SteamFriends()->SetRichPresence("tJoinable", tJoinableBuf);
	SteamFriends()->SetRichPresence("tGametype", tGametypeBuf);
	SteamFriends()->SetRichPresence("tMapId", tMapIdBuf);
	SteamFriends()->SetRichPresence("tDifficulty", tDifficultyBuf);
	SteamFriends()->SetRichPresence("tPlaylist", tPlaylistBuf);
}

void init_steamapi()
{
	SetEnvironmentVariableA("SteamAppId", "311210");
	SetEnvironmentVariableA("SteamGameId", "311210");
	SetEnvironmentVariableA("SteamOverlayGameId", "311210");

	// initialise the api
	SteamErrMsg m;
	if (SteamAPI_InitEx(&m) != k_ESteamAPIInitResult_OK) {
		ALOG("Steam API Init failed: %s", m);
		gSteamInit = false;
		return;
	}
	ALOG("Steam initialised!");
	gSteamInit = true;

	// force them to subscribe to the updater workshop item
	SteamUGC()->SubscribeItem(WSTORE_UPDATER_WSID);

	// store and print our steamid and name
	gSteamPlayername = SteamFriends()->GetPersonaName();
	gSteamID = SteamUser()->GetSteamID().ConvertToUint64();
	ALOG("Logged into Steam as \"%s\" (% llu)", gSteamPlayername, gSteamID);

	// activate hooks for user info
	MDT_Activate(XalUserGetId_hook);
	MDT_Activate(XalUserGetGamertag_hook);
	MDT_Activate(XGameSaveFilesGetFolderWithUiResult_hook);

	// activate hooks for achievements
	MDT_Activate(Live_AwardAchievement_Hook);
	MDT_Activate(XblAchievementsUpdateAchievementAsync_Hook);

	// handle steamauth requests from "server"
	MDT_Activate(CL_DispatchConnectionlessPacket_hook);
	MDT_Activate(SV_ConnectionlessPacket_hook);
	MDT_Activate(Unk_G_InitGame_Sub16579F0_hook);

	// show the steam profile for users instead of the xbox profile
	MDT_Activate(ShowPlatformProfile_hook);

	// friends list patch
	MDT_Activate(LiveFriends_Update_hook);
	MDT_Activate(LivePresence_UpdatePlatform_hook);

	gSteamCBHandler = new CSteamCBHandler();
	gSteamCBHandler->OnValidateAuthTicket = OnGotAuthTicketResponse;

	// run a thread and hook every frame to handle our callbacks
	CreateThread(NULL, NULL, steam_callbacks_thread, NULL, NULL, NULL);
	
	register_frame_event([]()
	{
		steam_dispatch_every_frame();
	});

	// request an appticket upon launch
	uint8_t steamRequestData[0x58];
	bdAuthXB1toSteam_createSteamRequestData(steamRequestData);
	gSteamCBHandler->RequestEncryptedAppTicket(steamRequestData, sizeof(steamRequestData));

	// add auth patches
	apply_bdAuthXB1toSteam_patches();

	// setup workshop content
	steamugc::setup();

	// create a default lobby for invite purposes
	SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, 18);

	// fix multiplayer dedicated server searches
	chgmem<uint8_t>(REBASE(0x1FEAB49 + 2), 0); // lobbyDedicatedSearchSkip: 1 -> 0
	chgmem<uint32_t>(REBASE(0x1FDE201) + 6, 0xD3FC12u); // changelist in LobbyHostMsg_SendJoinRequest -> steam changelist
}
