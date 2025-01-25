#include "steamugc.h"
#include "framework.h"
#include "steam/steam_api.h"
#include <intrin.h>
#include <fstream>
#include <unordered_map>
#include "PicoSHA2.h"
#include <filesystem>

struct workshop_json
{
	enum workshop_json_parserstate
	{
		WJSPS_OPEN_BRACE, // open brace
		WJSPS_READY, // close brace or quote
		WJSPS_TOKEN, // close quote
		WJSPS_DONE
	};
	
private:
	char* __internal_data;
	std::unordered_map<uint32_t, const char*> props;
	size_t data_len;
	
	void clear()
	{
		if (__internal_data)
		{
			delete[] __internal_data;
			__internal_data = NULL;
		}
		props.clear();
	}

	char* make_buf(size_t len)
	{
		clear();
		__internal_data = new char[len];
		memset(__internal_data, 0, len);
		data_len = len;
		return __internal_data;
	}

public:
	bool parse(std::ifstream& inFile)
	{
		inFile.seekg(0, std::ios::end);
		size_t fileSize = inFile.tellg();
		inFile.seekg(0, std::ios::beg);
		inFile.read(make_buf(fileSize + 1), fileSize);

		char* current_index = __internal_data;
		char* current_key = NULL;
		char* current_value = NULL;

		workshop_json_parserstate state = WJSPS_OPEN_BRACE;
		bool isKey = true;
		bool escaped = false;

		while (*current_index && (current_index < (__internal_data + data_len)) && (state != WJSPS_DONE))
		{
			switch (state)
			{
				case WJSPS_OPEN_BRACE:
				{
					if (*current_index++ != '{')
					{
						break;
					}
					state = WJSPS_READY;
				}
				break;
				case WJSPS_READY:
				{
					auto ch = *current_index++;
					if (ch == '"')
					{
						state = WJSPS_TOKEN;

						if (isKey)
						{
							current_key = current_index;
						}
						else
						{
							current_value = current_index;
						}
					}
					else if (ch == '}')
					{
						state = WJSPS_DONE;
					}
				}
				break;
				case WJSPS_TOKEN:
				{
					auto ch = *current_index;

					if (ch == '"')
					{
						if (!escaped)
						{
							*current_index = 0;
							if (!isKey)
							{
								props[fnv1a(current_key)] = current_value;
								current_key = current_value = NULL;
							}

							isKey = !isKey;
							escaped = false;
							state = WJSPS_READY;
						}
					}

					escaped = !escaped && (ch == '\\');
					current_index++;
				}
				break;
			}
		}

		return state == WJSPS_DONE;
	}

	const char* find(const char* key)
	{
		auto it = props.find(fnv1a(key));
		return (it == props.end()) ? NULL : it->second;
	}

	bool is_mod()
	{
		auto val = find("Type");
		return !val || strcmp(val, "map");
	}

	const char* title()
	{
		auto val = find("Title");
		return val ? val : "<Error: Unknown Mod>";
	}

	const char* folder_name()
	{
		auto val = find("FolderName");
		return val ? val : "";
	}

	const char* description()
	{
		auto val = find("Description");
		return val ? val : "";
	}

	workshop_json()
	{
		__internal_data = NULL;
		data_len = 0;
	}

	~workshop_json()
	{
		clear();
	}
};

bool hasCheckedForUpdates = false;
void check_for_updates(const char* rootPath)
{
	if (hasCheckedForUpdates)
	{
		return;
	}

	char wsUpdaterPathOnDisk[MAX_PATH]{ 0 };
	strncat_s(wsUpdaterPathOnDisk, rootPath, strlen(rootPath));
	strncat_s(wsUpdaterPathOnDisk, "\\" WSTORE_UPDATER_FILENAME, strlen("\\" WSTORE_UPDATER_FILENAME));

	std::ifstream ifUpdater;
	ifUpdater.open(wsUpdaterPathOnDisk, std::ios::binary);

	if (!ifUpdater.is_open())
	{
		ALOG("FAILED to open '%s', cannot check for updates.", wsUpdaterPathOnDisk);
		return; // failed to check
	}

	std::vector<unsigned char> hashUpdater(picosha2::k_digest_size);
	picosha2::hash256(ifUpdater, hashUpdater.begin(), hashUpdater.end());
	ifUpdater.close();

	std::ifstream ifT7InternalWS;
	ifT7InternalWS.open(WSINTERNAL_MOD_NAME, std::ios::binary);

	if (!ifT7InternalWS.is_open())
	{
		ALOG("FAILED to open '%s', cannot check for updates.", WSINTERNAL_MOD_NAME);
		return; // failed to check
	}

	std::vector<unsigned char> hashCurrent(picosha2::k_digest_size);
	picosha2::hash256(ifT7InternalWS, hashCurrent.begin(), hashCurrent.end());
	ifT7InternalWS.close();

	bool needsUpdate = false;
	for (int i = 0; i < picosha2::k_digest_size; i++)
	{
		if (hashCurrent.at(i) != hashUpdater.at(i))
		{
			needsUpdate = true;
			break;
		}
	}

#if DEV_TEST_UPDATES
	goto update;
#endif

	if (!needsUpdate)
	{
		nlog("Binary is up to date. Skipping update!");
		goto end;
	}

#if IS_DEVELOPMENT && DEV_SKIP_UPDATES
	nlog("DEV_SKIP_UPDATES. Skipping update...");
	goto end;
#endif

	update:
	nlog("Binary is outdated. Installing update...");
	g_needsUpdatePrompt = std::filesystem::copy_file(wsUpdaterPathOnDisk, WSTORE_UPDATER_DEST_FILENAME, std::filesystem::copy_options::overwrite_existing);

	end:
	hasCheckedForUpdates = true;
}

void parse_ugc_entry(bool isMod, PublishedFileId_t fileId)
{
	auto itemState = SteamUGC()->GetItemState(fileId);

	// make sure its actually installed
	if (!(itemState & k_EItemStateInstalled))
	{
		return;
	}

	// check to make sure its not pending modifications
	if (itemState & (k_EItemStateNeedsUpdate | k_EItemStateDownloading | k_EItemStateDownloadPending | k_EItemStateDisabledLocally))
	{
		return;
	}

	uint64_t sizeOnDisk = 0;
	uint32_t timeStamp = 0;
	char pathOnDisk[MAX_PATH]{ 0 };
	char wsJsonPathOnDisk[MAX_PATH]{ 0 };

	// retrieve workshop info for the item
	if (!SteamUGC()->GetItemInstallInfo(fileId, &sizeOnDisk, pathOnDisk, MAX_PATH, &timeStamp))
	{
		return;
	}

	strncat_s(wsJsonPathOnDisk, pathOnDisk, strlen(pathOnDisk));
	strncat_s(wsJsonPathOnDisk, "\\workshop.json", strlen("\\workshop.json"));

	std::ifstream inFile;
	inFile.open(wsJsonPathOnDisk, std::ifstream::in | std::ios::binary);

	// make sure we found the workshop.json
	if (!inFile.is_open())
	{
		return;
	}

	workshop_json json;
	auto res = json.parse(inFile);
	inFile.close();

	// ensure workshop json was parsed correctly
	if (!res)
	{
		return;
	}
	
	// ensure its matching the query type
	if (json.is_mod() != isMod)
	{
		return;
	}

	if (WSTORE_UPDATER_WSID == fileId)
	{
		check_for_updates(pathOnDisk);
		return;
	}

	auto dest_list = (ugcinfo_wstor*)REBASE(0x18A7ED68); // usermaps
	if (isMod)
	{
		dest_list = (ugcinfo_wstor*)REBASE(0x18A58F60); // mods
	}

	// make sure we have space for a new entry
	if (dest_list->num_entries >= (uint32_t)WORKSHOP_MAX_ENTRIES)
	{
		return;
	}

	ugcinfo_entry_wstor* dest_entry = dest_list->entries + dest_list->num_entries++;
	memset(dest_entry, 0, sizeof(ugcinfo_entry_wstor));

	strncpy_s(dest_entry->name, json.title(), _TRUNCATE);
	strncpy_s(dest_entry->internalName, json.folder_name(), _TRUNCATE);
	snprintf(dest_entry->ugcName, sizeof(dest_entry->ugcName), "%llu", fileId);
	strncpy_s(dest_entry->description, json.description(), _TRUNCATE);
	strncpy_s(dest_entry->ugcPath, pathOnDisk, _TRUNCATE);

	auto sPos = strstr(pathOnDisk, "311210");

	if (sPos)
	{
		strncpy_s(dest_entry->ugcPathBasic, sPos, _TRUNCATE);
		strncpy_s(dest_entry->ugcRoot, pathOnDisk, sPos - pathOnDisk - 1);
	}

	auto fs_game = dest_entry->ugcName;
	auto v39 = *fs_game;
	for (dest_entry->hash_id = 5381; *fs_game; v39 = *fs_game)
	{
		++fs_game;
		dest_entry->hash_id = tolower(v39) + 33 * dest_entry->hash_id;
	}

	dest_entry->unk_4B0 = 1;
	dest_entry->unk_4B8 = isMod ? 1 : 2;
}

void update_steam_ugc(bool isMod)
{
	uint32_t numItems = SteamUGC()->GetNumSubscribedItems();

	auto itemsList = new PublishedFileId_t[numItems];
	SteamUGC()->GetSubscribedItems(itemsList, numItems);

	for (uint32_t i = 0; i < numItems; i++)
	{
		parse_ugc_entry(isMod, itemsList[i]);
	}

	delete[] itemsList;
	itemsList = NULL;
}

MDT_Define_FASTCALL(REBASE(0x21EF320), populate_ugc_list_hook, void, (uint64_t a1, unsigned char a2))
{
	update_steam_ugc((uint64_t)_ReturnAddress() != REBASE(0x21EF73A));
	MDT_ORIGINAL(populate_ugc_list_hook, (a1, a2));
}

void steamugc::setup()
{
	MDT_Activate(populate_ugc_list_hook);
}
