#pragma once
#include "steam/steam_api.h"
#include <functional>

class CSteamCBHandler {
public:
	void RequestEncryptedAppTicket(uint8_t* encryptedData, size_t encryptedDataLen);
	void OnRequestEncryptedAppTicket(EncryptedAppTicketResponse_t* pEncryptedAppTicketResponse, bool bIOFailure);
	CCallResult< CSteamCBHandler, EncryptedAppTicketResponse_t > m_SteamCallResultEncryptedAppTicket;
	std::function<void(ValidateAuthTicketResponse_t*)> OnValidateAuthTicket;

	STEAM_CALLBACK(CSteamCBHandler, OnP2PSessionRequest, P2PSessionRequest_t);
	STEAM_CALLBACK(CSteamCBHandler, OnUserStatsReceived, UserStatsReceived_t);
	STEAM_CALLBACK(CSteamCBHandler, OnGameRichPresenceJoinRequested, GameRichPresenceJoinRequested_t);
	STEAM_CALLBACK(CSteamCBHandler, OnGameLobbyJoinRequested, GameLobbyJoinRequested_t);
	STEAM_CALLBACK(CSteamCBHandler, __OnValidateAuthTicket, ValidateAuthTicketResponse_t);
	STEAM_CALLBACK(CSteamCBHandler, OnLobbyEnter, LobbyEnter_t);

	uint8_t encryptedAppTicket[0x400];
	uint32_t encryptedAppTicketSize;
	uint8_t hasPendingInvite;
	CSteamID pendingInvite;
};

extern CSteamCBHandler* gSteamCBHandler;
CSteamCBHandler* SteamCBHandler();
