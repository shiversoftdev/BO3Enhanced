#include "framework.h"
#include "steam/steam_api.h"
#include "steam.h"
#include "SteamCBHandler.h"

// the title id and version to report to demonware
// 5446 = BO3 Steam, v31 = v98 macOS
#define BD_TITLEID_LARP 5446
#define BD_VERSION_LARP 31

//defined in dllmain.cpp
extern void nlog(const char* str, ...);

#define bdBase64_encode(src, srclen, dst, dstlen) ((void(__fastcall*)(uint8_t *, size_t, char *, size_t))REBASE(0x297ad40))(src, srclen, dst, dstlen)
#define bdBase64_decode(src, srclen, dst, dstlen) ((void(__fastcall*)(char *, size_t, uint8_t *, size_t))REBASE(0x297ab60))(src, srclen, dst, dstlen)

#define bdCryptoUtils_calculateInitialVector(iv_seed, output_iv) ((void(__fastcall*)(uint32_t, uint8_t *))REBASE(0x2d0c030))(iv_seed, output_iv)

#define bdCypher3DES_constructor(bdCypher3DES) ((void(__fastcall*)(void *))REBASE(0x2d0f130))(bdCypher3DES)
#define bdCypher3DES_init(bdCypher3DES, key, keysize) ((void(__fastcall*)(void *, uint8_t *, size_t))REBASE(0x2d0f4b0))(bdCypher3DES, key, keysize)
#define bdCypher3DES_encrypt(bdCypher3DES, iv, in, out, size) ((bool(__fastcall*)(void *, uint8_t *, uint8_t *, uint8_t *, size_t))REBASE(0x2d0f400))(bdCypher3DES, iv, in, out, size)
#define bdCypher3DES_decrypt(bdCypher3DES, iv, in, out, size) ((bool(__fastcall*)(void *, uint8_t *, uint8_t *, uint8_t *, size_t))REBASE(0x2d0f350))(bdCypher3DES, iv, in, out, size)
#define bdCypher3DES_destructor(bdCypher3DES) ((void(__fastcall*)(void *))REBASE(0x2d0f210))(bdCypher3DES)

#define bdJSONSerializer_writeString(bdJSONSerializer, key, value) ((void(__fastcall*)(void *, const char *, const char *))REBASE(0x29f1c30))(bdJSONSerializer, key, value)
#define bdJSONSerializer_writeUInt64(bdJSONSerializer, key, value) ((void(__fastcall*)(void *, const char *, uint64_t))REBASE(0x29f1e30))(bdJSONSerializer, key, value)
#define bdJSONSerializer_writeBoolean(bdJSONSerializer, key, value) ((void(__fastcall*)(void *, const char *, bool))REBASE(0x29f1a10))(bdJSONSerializer, key, value)

#define bdAuth_handleAuthReply(bdAuth, id, idk) ((int(__fastcall*)(void *, int, int))REBASE(0x299fb60))(bdAuth, id, idk)

static uint8_t steamCookie[0x18];
static uint32_t ivSeed = 0xDEADF00D;

void bdCryptoUtils_encrypt(uint8_t* key, uint8_t* iv, uint8_t* in, uint8_t* out, size_t len)
{
	uint8_t ourCypher3Des[0x30];
	bdCypher3DES_constructor(ourCypher3Des);
	bdCypher3DES_init(ourCypher3Des, key, 0x18);
	bdCypher3DES_encrypt(ourCypher3Des, iv, in, out, len);
	bdCypher3DES_destructor(ourCypher3Des);
}

void bdCryptoUtils_decrypt(uint8_t* key, uint8_t* iv, uint8_t* in, uint8_t* out, size_t len)
{
	uint8_t ourCypher3Des[0x30];
	bdCypher3DES_constructor(ourCypher3Des);
	bdCypher3DES_init(ourCypher3Des, key, 0x18);
	bdCypher3DES_decrypt(ourCypher3Des, iv, in, out, len);
	bdCypher3DES_destructor(ourCypher3Des);
}

void bdAuthXB1toSteam_createSteamRequestData(uint8_t *requestData)
{
	memset(requestData, 0, 0x58);
	// demonware uses a secure RNG lmao what dorks
	for (int i = 0; i < 0x18; i++)
		requestData[i] = rand() & 0xFF;
	memcpy(steamCookie, requestData, sizeof(steamCookie));
	strncpy((char *)requestData + 0x18, gSteamPlayername, 0x40);
}

int bdAuthXB1toSteam_handleReply(void* param_1)
{
	return bdAuth_handleAuthReply(param_1, 0x1D, 0x80);
}

MDT_Define_FASTCALL(REBASE(0x29ef8a0), bdJSONDeserializer_getString_hook, bool, (void* bdJSONDeserializer, char* key, char* out, size_t len))
{
	// device id
	if (strcmp(key, "dpi") == 0)
	{
		strncpy(out, "00112233445566778899AABBCCDDEEFFA1A2A3A4", len);
		return true;
	}
	// sandbox id
	else if (strcmp(key, "sbx") == 0)
	{
		strncpy(out, "RETAIL", len);
		return true;
	}
	else return MDT_ORIGINAL(bdJSONDeserializer_getString_hook, (bdJSONDeserializer, key, out, len));
}

bool bdAuthXB1toSteam_createPlatformDataJSON(void* bdAuthXboxOne, void* json)
{
	// generate then encrypt the expected "version" parameter with the previously decided iv_seed
	uint8_t versionData[8] = {0xde, 0xad, 0xbd, 0xef, BD_VERSION_LARP, 0, 0, 0 };
	uint8_t versionEncrypted[8] = { 0 };
	char versionEncoded[13];
	uint8_t ourIv[8];
	bdCryptoUtils_calculateInitialVector(ivSeed, ourIv);
	bdCryptoUtils_encrypt(steamCookie, ourIv, versionData, versionEncrypted, 8);
	bdBase64_encode(versionEncrypted, 8, versionEncoded, 13);

	// base64 encode the token we got from steam
	char tokenEncoded[0x800] = { 0 };
	bdBase64_encode(SteamCBHandler()->encryptedAppTicket, SteamCBHandler()->encryptedAppTicketSize, tokenEncoded, sizeof(tokenEncoded));

	// add our token, the encrypted version, and a request for extended data
	bdJSONSerializer_writeString(json, "token", tokenEncoded);
	bdJSONSerializer_writeString(json, "version", versionEncoded);
	bdJSONSerializer_writeBoolean(json, "extended_data", true);

	return true;
}

bool bdAuthXB1toSteam_processPlatformClientTicket(void* bdAuthXboxOne, uint32_t iv_seed, uint8_t* ticket, uint32_t length)
{
	uint8_t ourIv[8];
	bdCryptoUtils_calculateInitialVector(iv_seed, ourIv);
	bdCryptoUtils_decrypt(steamCookie, ourIv, ticket, ticket, length);
	return true;
}

MDT_Define_FASTCALL(REBASE(0x2d0c1a0), GenerateIVSeed, uint32_t, ())
{
	return ivSeed;
}

MDT_Define_FASTCALL(REBASE(0x29a81e0), bdAntiCheat_reportExtendedAuthInfo_Hook, void*, (void* a, void* b, int gameMode, int gameVersion, uint64_t checksum, uint8_t* macaddr, uint64_t internaladdr, uint64_t externaladdr, void* authinfo))
{
	return MDT_ORIGINAL(bdAntiCheat_reportExtendedAuthInfo_Hook, (a, b, gameMode, BD_VERSION_LARP, checksum, macaddr, internaladdr, externaladdr, authinfo));
}

MDT_Define_FASTCALL(REBASE(0x29a7db0), bdAntiCheat_reportConsoleDetails_Hook, void*, (void* a, void* b, int gameMode, int gameVersion, uint64_t checksum, uint8_t* macaddr, uint64_t internaladdr, uint64_t externaladdr, void* consoleid))
{
	return MDT_ORIGINAL(bdAntiCheat_reportConsoleDetails_Hook, (a, b, gameMode, BD_VERSION_LARP, checksum, macaddr, internaladdr, externaladdr, consoleid));
}

void apply_bdAuthXB1toSteam_patches()
{
	uint8_t fiveNops[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
	uint8_t movAl1Ret[3] = { 0xb0, 0x01, 0xC3 };
	char newUnoName[] = "treyarch-cod-ops3-steam";
	char newUnoPlatform[] = "steam";
	char newLSGServer[] = "ops3-pc-lobby.prod.demonware.net";
	char newMarketplaceContext[] = "ops3_pc";

	// replace the dw title id
	chgmem<uint32_t>(REBASE(0x30fad94), BD_TITLEID_LARP); // global
	chgmem<uint32_t>(REBASE(0x1eebafa), BD_TITLEID_LARP); // sets the global
	// modify bdAuth::makeAuthForXBoxOne
	chgmem<uint8_t>(REBASE(0x29a05f8), 28); // auth_task = 28 (Steam)
	chgmem(REBASE(0x29a065b), 5, fiveNops); // remove "identity" value from request
	chgmem(REBASE(0x29a096f), 5, fiveNops); // remove "Authorization" header from request
	// hook a function call in bdAuth::makeAuthForXBoxOne to specify IV seed
	MDT_Activate(GenerateIVSeed);
	// replace members of the bdAuthXboxOne vtable
	chgmem<void*>(REBASE(0x2dd88e8), bdAuthXB1toSteam_handleReply);
	chgmem<void*>(REBASE(0x2dd88f8), bdAuthXB1toSteam_processPlatformClientTicket);
	chgmem<void*>(REBASE(0x2dd8908), bdAuthXB1toSteam_createPlatformDataJSON);
	// patch a bdAuthXboxOne vtable function to return true always
	chgmem(REBASE(0x29a3d10), 3, movAl1Ret);
	// hook bdJSONDeserializer::getString for bdAuthXboxOne::processPlatformData's dpi/sbx requirement
	MDT_Activate(bdJSONDeserializer_getString_hook);
	// replace the client name used for umbrella/uno and other dw stuff
	chgmem(REBASE(0x2f0c558), sizeof(newUnoName), newUnoName);
	// replace the domain name. this tramples the dev url in memory
	chgmem(REBASE(0x2f0c580), sizeof(newLSGServer), newLSGServer);
	// fix the version number in bdAntiCheat
	MDT_Activate(bdAntiCheat_reportConsoleDetails_Hook);
	MDT_Activate(bdAntiCheat_reportExtendedAuthInfo_Hook);
	// replace the uno platform from "xbl" to "steam"
	chgmem(REBASE(0x2f3b8f0), sizeof(newUnoPlatform), newUnoPlatform);
	// replace the marketplace context from ops3_wingdk to ops3_pc
	chgmem(REBASE(0x2F0C5F8), sizeof(newMarketplaceContext), newMarketplaceContext);
}
