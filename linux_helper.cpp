// TODO(Emma): remove these as WineGDK becomes more mature.
#include "framework.h"

typedef struct _XAsyncBlock {
	void* queue;
	void* context;
	void(*cb_func)(_XAsyncBlock* block);
	void* reserved[4];
} XAsyncBlock;

MDT_Define_FASTCALL(REBASE(0x2920410), XGameSaveFilesGetFolderWithUi_hook, uint64_t, (uint64_t a1, char* scid, XAsyncBlock* async))
{
	if (async->cb_func != NULL)
		async->cb_func(async);
	return 0;
}

MDT_Define_FASTCALL(REBASE(0x2921FA8), XalAddUserWithUiAsync_hook, uint64_t, (uint32_t userId, XAsyncBlock* async))
{
	if (async->cb_func != NULL)
		async->cb_func(async);
	return 0;
}

MDT_Define_FASTCALL(REBASE(0x2922020), XalAddUserWithUiResult_hook, uint64_t, (uint32_t userId, uint64_t* out_handle))
{
	*out_handle = 0x72742069670B00B5;
	return 0;
}

void do_linux_hooks()
{
	MDT_Activate(XGameSaveFilesGetFolderWithUi_hook);
	MDT_Activate(XalAddUserWithUiAsync_hook);
	MDT_Activate(XalAddUserWithUiResult_hook);
}

static const char* (*wine_get_version)(void) = NULL;
static const char* (*wine_get_build_id)(void) = NULL;
static void (*wine_get_host_version)(const char **sysname, const char **release) = NULL;

static bool done_check = false;
bool is_on_linux()
{
	// if we've already done the check before, return the result
	if (done_check)
		return (wine_get_version != NULL);

	// look up the Wine exports from ntdll
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (ntdll == NULL)
		return false;
	done_check = true;
	wine_get_version = (const char *(*)(void))GetProcAddress(ntdll, "wine_get_version");
	if (wine_get_version == NULL)
		return false;
	wine_get_build_id = (const char* (*)(void))GetProcAddress(ntdll, "wine_get_build_id");
	wine_get_host_version = (void (*)(const char**, const char**))GetProcAddress(ntdll, "wine_get_host_version");

	// if we've got this far we're on Wine - print the version out
	const char* version = wine_get_version();
	const char* build_id = "N/A";
	const char* sysname = "N/A";
	const char* release = "N/A";
	if (wine_get_build_id != NULL)
		build_id = wine_get_build_id();
	if (wine_get_host_version != NULL)
		wine_get_host_version(&sysname, &release);
	ALOG("Wine detected - running on Wine %s build %s (on %s %s)", version, build_id, sysname, release);
	return true;
}
