#include "../new_pins.h"
#include "../new_cfg.h"
#include "../logging/logging.h"
#include "../obk_config.h"
#include <ctype.h>
#include "cmd_local.h"
#include "../driver/drv_ir.h"
#include "../driver/drv_uart.h"
#include "../driver/drv_public.h"
#include "../hal/hal_adc.h"
#include "../hal/hal_flashVars.h"

int g_pingWatchDog_intervalMs = 1000;
int cmd_uartInitIndex = 0;


#ifdef ENABLE_LITTLEFS
#include "../littlefs/our_lfs.h"
#endif
#ifdef PLATFORM_BL602
#include <wifi_mgmr_ext.h>
#endif

#define HASH_SIZE 128

static int generateHashValue(const char* fname) {
	int		i;
	int		hash;
	int		letter;
	unsigned char* f = (unsigned char*)fname;

	hash = 0;
	i = 0;
	while (f[i]) {
		letter = tolower(f[i]);
		hash += (letter) * (i + 119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (HASH_SIZE - 1);
	return hash;
}

command_t* g_commands[HASH_SIZE] = { NULL };
bool g_powersave;

static commandResult_t CMD_PowerSave(const void* context, const char* cmd, const char* args, int cmdFlags) {
	int bOn = 1;
	Tokenizer_TokenizeString(args, 0);

	if (Tokenizer_GetArgsCount() > 0) {
		bOn = Tokenizer_GetArgInteger(0);
	}
	ADDLOG_INFO(LOG_FEATURE_CMD, "CMD_PowerSave: will set to %i", bOn);

#ifdef PLATFORM_BEKEN
	extern int bk_wlan_power_save_set_level(BK_PS_LEVEL level);
	if (bOn) {
		bk_wlan_power_save_set_level(/*PS_DEEP_SLEEP_BIT */  PS_RF_SLEEP_BIT | PS_MCU_SLEEP_BIT);
	}
	else {
		bk_wlan_power_save_set_level(0);
	}
#elif defined(PLATFORM_W600)
	if (bOn) {
		tls_wifi_set_psflag(1, 0);	//Enable powersave but don't save to flash
	}
	else {
		tls_wifi_set_psflag(0, 0);	//Disable powersave but don't save to flash
	}
#elif defined(PLATFORM_BL602)
	if (bOn) {
		wifi_mgmr_sta_powersaving(2);
	}
	else {
		wifi_mgmr_sta_powersaving(0);
	}
#else
	ADDLOG_INFO(LOG_FEATURE_CMD, "PowerSave is not implemented on this platform");
#endif    
	g_powersave = bOn;
	return CMD_RES_OK;
}
static commandResult_t CMD_DeepSleep(const void* context, const char* cmd, const char* args, int cmdFlags) {
	int timeMS;

	Tokenizer_TokenizeString(args, 0);

	// following check must be done after 'Tokenizer_TokenizeString',
	// so we know arguments count in Tokenizer. 'cmd' argument is
	// only for warning display
	if (Tokenizer_CheckArgsCountAndPrintWarning(cmd, 1)) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	timeMS = Tokenizer_GetArgInteger(0);
#ifdef PLATFORM_BEKEN
	// It requires a define in SDK file:
	// OpenBK7231T\platforms\bk7231t\bk7231t_os\beken378\func\include\manual_ps_pub.h
	// define there:
	// #define     PS_SUPPORT_MANUAL_SLEEP     1
	extern void bk_wlan_ps_wakeup_with_timer(UINT32 sleep_time);
	bk_wlan_ps_wakeup_with_timer(timeMS);
	return CMD_RES_OK;
#elif defined(PLATFORM_W600)

#endif

	return CMD_RES_OK;
}

static commandResult_t CMD_ScheduleHADiscovery(const void* context, const char* cmd, const char* args, int cmdFlags) {
	int delay;

	if (args && *args) {
		delay = atoi(args);
	}
	else {
		delay = 5;
	}

	Main_ScheduleHomeAssistantDiscovery(delay);

	return CMD_RES_OK;
}
static commandResult_t CMD_Flags(const void* context, const char* cmd, const char* args, int cmdFlags) {
	union {
		long long newValue;
		struct {
			int ints[2];
			int dummy[2]; // just to be safe
		};
	} u;
	// TODO: check on other platforms, on Beken it's 8, 64 bit
	// On Windows simulator it's 8 as well
	//ADDLOG_INFO(LOG_FEATURE_CMD, "CMD_Flags: sizeof(newValue) = %i", sizeof(u.newValue));
	if (args && *args) {
		if (1 != sscanf(args, "%lld", &u.newValue)) {
			//ADDLOG_INFO(LOG_FEATURE_CMD, "Argument/sscanf error!");
			return CMD_RES_BAD_ARGUMENT;
		}
	}
	else {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	CFG_SetFlags(u.ints[0], u.ints[1]);
	ADDLOG_INFO(LOG_FEATURE_CMD, "New flags set!");

	return CMD_RES_OK;
}
static commandResult_t CMD_HTTPOTA(const void* context, const char* cmd, const char* args, int cmdFlags) {

	if (args && *args) {
		OTA_RequestDownloadFromHTTP(args);
	}
	else {
		ADDLOG_INFO(LOG_FEATURE_CMD, "Command requires 1 argument");
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	return CMD_RES_OK;
}
static void stackOverflow(int a) {
	char lala[64];
	int i;

	for (i = 0; i < sizeof(lala); i++) {
		lala[i] = a;
	}
	stackOverflow(a + 1);
}
static commandResult_t CMD_StackOverflow(const void* context, const char* cmd, const char* args, int cmdFlags) {
	ADDLOG_INFO(LOG_FEATURE_CMD, "CMD_StackOverflow: Will overflow soon");

	stackOverflow(0);

	return CMD_RES_OK;
}
static commandResult_t CMD_CrashNull(const void* context, const char* cmd, const char* args, int cmdFlags) {
	int *p = (int*)0;

	ADDLOG_INFO(LOG_FEATURE_CMD, "CMD_CrashNull: Will crash soon");

	while (1) {
		*p = 0;
		p++;
	}

	

	return CMD_RES_OK;
}

static commandResult_t CMD_Restart(const void* context, const char* cmd, const char* args, int cmdFlags) {
	int delaySeconds;

	if (args == 0 || *args == 0) {
		delaySeconds = 3;
	}
	else {
		delaySeconds = atoi(args);
	}

	ADDLOG_INFO(LOG_FEATURE_CMD, "CMD_Restart: will reboot in %i", delaySeconds);

	RESET_ScheduleModuleReset(delaySeconds);

	return CMD_RES_OK;
}
static commandResult_t CMD_ClearAll(const void* context, const char* cmd, const char* args, int cmdFlags) {

	CFG_SetDefaultConfig();
	CFG_Save_IfThereArePendingChanges();

	CHANNEL_ClearAllChannels();
	CMD_ClearAllHandlers(0, 0, 0, 0);
	RepeatingEvents_Cmd_ClearRepeatingEvents(0, 0, 0, 0);
#if defined(WINDOWS) || defined(PLATFORM_BL602) || defined(PLATFORM_BEKEN)
	CMD_resetSVM(0, 0, 0, 0);
#endif

	ADDLOG_INFO(LOG_FEATURE_CMD, "CMD_ClearAll: all clear");

	return CMD_RES_OK;
}
static commandResult_t CMD_ClearNoPingTime(const void* context, const char* cmd, const char* args, int cmdFlags) {
	g_timeSinceLastPingReply = 0;
	return CMD_RES_OK;
}
static commandResult_t CMD_ClearConfig(const void* context, const char* cmd, const char* args, int cmdFlags) {

	CFG_SetDefaultConfig();
	CFG_Save_IfThereArePendingChanges();

	ADDLOG_INFO(LOG_FEATURE_CMD, "CMD_ClearConfig: whole device config has been cleared, restart device to connect to clear AP");

	return CMD_RES_OK;
}
// setChannel 1 123
// echo First channel is $CH1 and this is the test
// will print echo First channel is 123 and this is the test
static commandResult_t CMD_Echo(const void* context, const char* cmd, const char* args, int cmdFlags) {

#if 0
	ADDLOG_INFO(LOG_FEATURE_CMD, args);
#else
	// we want $CH40 etc expanded
	Tokenizer_TokenizeString(args, TOKENIZER_ALTERNATE_EXPAND_AT_START | TOKENIZER_FORCE_SINGLE_ARGUMENT_MODE);
	ADDLOG_INFO(LOG_FEATURE_CMD, Tokenizer_GetArgFrom(0));
#endif

	return CMD_RES_OK;
}
static commandResult_t CMD_PingHost(const void* context, const char* cmd, const char* args, int cmdFlags) {
	Tokenizer_TokenizeString(args, 0);

	// following check must be done after 'Tokenizer_TokenizeString',
	// so we know arguments count in Tokenizer. 'cmd' argument is
	// only for warning display
	if (Tokenizer_CheckArgsCountAndPrintWarning(cmd, 1)) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	CFG_SetPingHost(Tokenizer_GetArg(0));

	return CMD_RES_OK;
}
static commandResult_t CMD_PingInterval(const void* context, const char* cmd, const char* args, int cmdFlags) {
	Tokenizer_TokenizeString(args, 0);

	// following check must be done after 'Tokenizer_TokenizeString',
	// so we know arguments count in Tokenizer. 'cmd' argument is
	// only for warning display
	if (Tokenizer_CheckArgsCountAndPrintWarning(cmd, 1)) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	// convert seconds to ms
	g_pingWatchDog_intervalMs = Tokenizer_GetArgInteger(0) * 1000;

	return CMD_RES_OK;
}
static commandResult_t CMD_SetStartValue(const void* context, const char* cmd, const char* args, int cmdFlags) {
	int ch, val;

	Tokenizer_TokenizeString(args, 0);

	// following check must be done after 'Tokenizer_TokenizeString',
	// so we know arguments count in Tokenizer. 'cmd' argument is
	// only for warning display
	if (Tokenizer_CheckArgsCountAndPrintWarning(cmd, 2)) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	ch = Tokenizer_GetArgInteger(0);
	val = Tokenizer_GetArgInteger(1);

	CFG_SetChannelStartupValue(ch, val);

	return CMD_RES_OK;
}
static commandResult_t CMD_OpenAP(const void* context, const char* cmd, const char* args, int cmdFlags) {

	g_openAP = 5;

	return CMD_RES_OK;
}
static commandResult_t CMD_SafeMode(const void* context, const char* cmd, const char* args, int cmdFlags) {
	int i;

	// simulate enough boots so the reboot will go into safe mode
	for (i = 0; i <= RESTARTS_REQUIRED_FOR_SAFE_MODE; i++) {
		HAL_FlashVars_IncreaseBootCount();
	}
	RESET_ScheduleModuleReset(3);

	return CMD_RES_OK;
}



void CMD_UART_Init() {
#if PLATFORM_BEKEN
	UART_InitUART(115200);
	cmd_uartInitIndex = g_uart_init_counter;
	UART_InitReceiveRingBuffer(512);
#endif
}
void CMD_UART_Run() {
#if PLATFORM_BEKEN
	char a;
	int i;
	int totalSize;
	char tmp[128];

	totalSize = UART_GetDataSize();
	while (totalSize) {
		a = UART_GetNextByte(0);
		if (a == '\n' || a == '\r' || a == ' ' || a == '\t') {
			UART_ConsumeBytes(1);
			totalSize = UART_GetDataSize();
		}
		else {
			break;
		}
	}
	if (totalSize < 2) {
		return;
	}
	// skip garbage data (should not happen)
	for (i = 0; i < totalSize; i++) {
		a = UART_GetNextByte(i);
		if (i + 1 < sizeof(tmp)) {
			tmp[i] = a;
			tmp[i + 1] = 0;
		}
		if (a == '\n') {
			break;
		}
	}
	UART_ConsumeBytes(i);
	CMD_ExecuteCommand(tmp, 0);
#endif
}
void CMD_RunUartCmndIfRequired() {
#if PLATFORM_BEKEN
	if (CFG_HasFlag(OBK_FLAG_CMD_ACCEPT_UART_COMMANDS)) {
		if (cmd_uartInitIndex && cmd_uartInitIndex == g_uart_init_counter) {
			CMD_UART_Run();
		}
	}
#endif
	}

// run an aliased command
static commandResult_t runcmd(const void* context, const char* cmd, const char* args, int cmdFlags) {
	char* c = (char*)context;
	//   char *p = c;

	//   while (*p && !isWhiteSpace(*p)) {
	//       p++;
	   //}
	//   if (*p) p++;
	return CMD_ExecuteCommand(c, cmdFlags);
}

commandResult_t CMD_CreateAliasHelper(const char *alias, const char *ocmd) {
	char* cmdMem;
	char* aliasMem;
	command_t* existing;

	existing = CMD_Find(alias);
	if (existing != 0) {
		ADDLOG_INFO(LOG_FEATURE_EVENT, "CMD_Alias: the alias you are trying to use is already in use (as an alias or as a command)");
		return CMD_RES_BAD_ARGUMENT;
	}

	cmdMem = strdup(ocmd);
	aliasMem = strdup(alias);

	ADDLOG_INFO(LOG_FEATURE_CMD, "New alias has been set: %s runs %s", alias, ocmd);

	//cmddetail:{"name":"aliasMem","args":"",
	//cmddetail:"descr":"Internal usage only. See docs for 'alias' command.",
	//cmddetail:"fn":"runcmd","file":"cmnds/cmd_test.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand(aliasMem, runcmd, cmdMem);
	return CMD_RES_OK;
}
// run an aliased command
static commandResult_t CMD_CreateAliasForCommand(const void* context, const char* cmd, const char* args, int cmdFlags) {
	const char* alias;
	const char* ocmd;

	Tokenizer_TokenizeString(args, 0);
	// following check must be done after 'Tokenizer_TokenizeString',
	// so we know arguments count in Tokenizer. 'cmd' argument is
	// only for warning display
	if (Tokenizer_CheckArgsCountAndPrintWarning(cmd, 2)) {
		return CMD_RES_NOT_ENOUGH_ARGUMENTS;
	}

	alias = Tokenizer_GetArg(0);
	ocmd = Tokenizer_GetArgFrom(1);

	return CMD_CreateAliasHelper(alias, ocmd);
}
static commandResult_t CMD_SimonTest(const void* context, const char* cmd, const char* args, int cmdFlags) {
	ADDLOG_INFO(LOG_FEATURE_CMD, "CMD_SimonTest: ir test routine");

#ifdef PLATFORM_BK7231T
	//stackCrash(0);
	//CrashMalloc();
	// anything
#endif

	return CMD_RES_OK;
}
void CMD_Init_Early() {
	//cmddetail:{"name":"alias","args":"[Alias][Command with spaces]",
	//cmddetail:"descr":"add an aliased command, so a command with spaces can be called with a short, nospaced alias",
	//cmddetail:"fn":"alias","file":"cmnds/cmd_test.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("alias", CMD_CreateAliasForCommand, NULL);
	//cmddetail:{"name":"echo","args":"[Message]",
	//cmddetail:"descr":"Sends given message back to console. This command expands variables, so writing $CH12 will print value of channel 12, etc. Remember that you can also use special channel indices to access persistant flash variables and to access LED variables like dimmer, etc.",
	//cmddetail:"fn":"CMD_Echo","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("echo", CMD_Echo, NULL);
	//cmddetail:{"name":"restart","args":"",
	//cmddetail:"descr":"Reboots the module",
	//cmddetail:"fn":"CMD_Restart","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("restart", CMD_Restart, NULL);
	//cmddetail:{"name":"reboot","args":"",
	//cmddetail:"descr":"Same as restart. Needed for bkWriter 1.60 which sends 'reboot' cmd before trying to get bus",
	//cmddetail:"fn":"CMD_Restart","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("reboot", CMD_Restart, NULL);
	//cmddetail:{"name":"clearConfig","args":"",
	//cmddetail:"descr":"Clears all config, including WiFi data",
	//cmddetail:"fn":"CMD_ClearConfig","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("clearConfig", CMD_ClearConfig, NULL);
	//cmddetail:{"name":"clearAll","args":"",
	//cmddetail:"descr":"Clears config and all remaining features, like runtime scripts, events, etc",
	//cmddetail:"fn":"CMD_ClearAll","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("clearAll", CMD_ClearAll, NULL);
	//cmddetail:{"name":"DeepSleep","args":"[Seconds]",
	//cmddetail:"descr":"Starts deep sleep for given amount of seconds. Please remember that there is also a separate command, called PinDeepSleep, which is not using a timer, but a GPIO to wake up device.",
	//cmddetail:"fn":"CMD_DeepSleep","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("DeepSleep", CMD_DeepSleep, NULL);
	//cmddetail:{"name":"PowerSave","args":"[Optional 1 or 0, by default 1 is assumed]",
	//cmddetail:"descr":"Enables dynamic power saving mode on BK and W600. You can also disable power saving with PowerSave 0.",
	//cmddetail:"fn":"CMD_PowerSave","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("PowerSave", CMD_PowerSave, NULL);
	//cmddetail:{"name":"stackOverflow","args":"",
	//cmddetail:"descr":"Causes a stack overflow",
	//cmddetail:"fn":"CMD_StackOverflow","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("stackOverflow", CMD_StackOverflow, NULL);
	//cmddetail:{"name":"crashNull","args":"",
	//cmddetail:"descr":"Causes a crash",
	//cmddetail:"fn":"CMD_CrashNull","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("crashNull", CMD_CrashNull, NULL);
	//cmddetail:{"name":"simonirtest","args":"",
	//cmddetail:"descr":"Simons Special Test",
	//cmddetail:"fn":"CMD_SimonTest","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("simonirtest", CMD_SimonTest, NULL);
	//cmddetail:{"name":"if","args":"[Condition]['then'][CommandA]['else'][CommandB]",
	//cmddetail:"descr":"Executed a conditional. Condition should be single line. You must always use 'then' after condition. 'else' is optional. Use aliases or quotes for commands with spaces",
	//cmddetail:"fn":"CMD_If","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("if", CMD_If, NULL);
	//cmddetail:{"name":"ota_http","args":"[HTTP_URL]",
	//cmddetail:"descr":"Starts the firmware update procedure, the argument should be a reachable HTTP server file. You can easily setup HTTP server with Xampp, or Visual Code, or Python, etc. Make sure you are using OTA file for a correct platform (getting N platform RBL on T will brick device, etc etc)",
	//cmddetail:"fn":"CMD_HTTPOTA","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("ota_http", CMD_HTTPOTA, NULL);
	//cmddetail:{"name":"scheduleHADiscovery","args":"[Seconds]",
	//cmddetail:"descr":"This will schedule HA discovery, the discovery will happen with given number of seconds, but timer only counts when MQTT is connected. It will not work without MQTT online, so you must set MQTT credentials first.",
	//cmddetail:"fn":"CMD_ScheduleHADiscovery","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("scheduleHADiscovery", CMD_ScheduleHADiscovery, NULL);
	//cmddetail:{"name":"flags","args":"[IntegerValue]",
	//cmddetail:"descr":"Sets the device flags",
	//cmddetail:"fn":"CMD_Flags","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("flags", CMD_Flags, NULL);
	//cmddetail:{"name":"ClearNoPingTime","args":"",
	//cmddetail:"descr":"Command for ping watchdog; it sets the 'time since last ping reply' to 0 again",
	//cmddetail:"fn":"CMD_ClearNoPingTime","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("ClearNoPingTime", CMD_ClearNoPingTime, NULL);
	//cmddetail:{"name":"SetStartValue","args":"[Channel][Value]",
	//cmddetail:"descr":"Sets the startup value for a channel. Used for start values for relays. Use 1 for High, 0 for low and -1 for 'remember last state'",
	//cmddetail:"fn":"CMD_SetStartValue","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("SetStartValue", CMD_SetStartValue, NULL);
	//cmddetail:{"name":"OpenAP","args":"",
	//cmddetail:"descr":"Temporarily disconnects from programmed WiFi network and opens Access Point",
	//cmddetail:"fn":"CMD_OpenAP","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("OpenAP", CMD_OpenAP, NULL);
	//cmddetail:{"name":"SafeMode","args":"",
	//cmddetail:"descr":"Forces device reboot into safe mode (open ap with disabled drivers)",
	//cmddetail:"fn":"CMD_SafeMode","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("SafeMode", CMD_SafeMode, NULL);
	//cmddetail:{"name":"PingInterval","args":"[IntegerSeconds]",
	//cmddetail:"descr":"Sets the interval between ping attempts for ping watchdog mechanism",
	//cmddetail:"fn":"CMD_PingInterval","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("PingInterval", CMD_PingInterval, NULL);
	//cmddetail:{"name":"PingHost","args":"[IPStr]",
	//cmddetail:"descr":"Sets the host to ping by IP watchdog",
	//cmddetail:"fn":"CMD_PingHost","file":"cmnds/cmd_main.c","requires":"",
	//cmddetail:"examples":""}
	CMD_RegisterCommand("PingHost", CMD_PingHost, NULL);
	
#if (defined WINDOWS) || (defined PLATFORM_BEKEN)
	CMD_InitScripting();
#endif
	if (!bSafeMode) {
		if (CFG_HasFlag(OBK_FLAG_CMD_ACCEPT_UART_COMMANDS)) {
			CMD_UART_Init();
		}
	}
	//DRV_InitFlashMemoryTestFunctions();
}

void CMD_Init_Delayed() {
	if (CFG_HasFlag(OBK_FLAG_CMD_ENABLETCPRAWPUTTYSERVER)) {
		CMD_StartTCPCommandLine();
	}
}


void CMD_ListAllCommands(void* userData, void (*callback)(command_t* cmd, void* userData)) {
	int i;
	command_t* newCmd;

	for (i = 0; i < HASH_SIZE; i++) {
		newCmd = g_commands[i];
		while (newCmd) {
			callback(newCmd, userData);
			newCmd = newCmd->next;
		}
	}

}
void CMD_FreeAllCommands() {
	int i;
	command_t* cmd, * next;

	for (i = 0; i < HASH_SIZE; i++) {
		cmd = g_commands[i];
		while (cmd) {
			next = cmd->next;
			free(cmd);
			cmd = next;
		}
		g_commands[i] = 0;
	}

}
void CMD_RegisterCommand(const char* name, commandHandler_t handler, void* context) {
	int hash;
	command_t* newCmd;

	// check
	newCmd = CMD_Find(name);
	if (newCmd != 0) {
		ADDLOG_ERROR(LOG_FEATURE_CMD, "command with name %s already exists!", name);
		return;
	}
	ADDLOG_DEBUG(LOG_FEATURE_CMD, "Adding command %s", name);

	hash = generateHashValue(name);
	newCmd = (command_t*)malloc(sizeof(command_t));
	newCmd->handler = handler;
	newCmd->name = name;
	newCmd->next = g_commands[hash];
	newCmd->context = context;
	g_commands[hash] = newCmd;
}

command_t* CMD_Find(const char* name) {
	int hash;
	command_t* newCmd;

	hash = generateHashValue(name);

	newCmd = g_commands[hash];
	while (newCmd != 0) {
		if (!stricmp(newCmd->name, name)) {
			return newCmd;
		}
		newCmd = newCmd->next;
	}
	return 0;
}

// get a string up to whitespace.
// if stripnum is set, stop at numbers.
int get_cmd(const char* s, char* dest, int maxlen, int stripnum) {
	int i;
	int count = 0;
	for (i = 0; i < maxlen - 1; i++) {
		if (isWhiteSpace(*s)) {
			break;
		}
		if (*s == 0) {
			break;
		}
		if (stripnum && *s >= '0' && *s <= '9') {
			break;
		}
		*(dest++) = *(s++);
		count++;
	}
	*dest = '\0';
	return count;
}


// execute a command from cmd and args - used below and in MQTT
commandResult_t CMD_ExecuteCommandArgs(const char* cmd, const char* args, int cmdFlags) {
	command_t* newCmd;
	//int len;

	// look for complete commmand
	newCmd = CMD_Find(cmd);
	if (!newCmd) {
		// not found, so...
		char nonums[32];
		// get the complete string up to numbers.
		//len =
		get_cmd(cmd, nonums, 32, 1);
		newCmd = CMD_Find(nonums);
		if (!newCmd) {
			// if still not found, then error
			ADDLOG_ERROR(LOG_FEATURE_CMD, "cmd %s NOT found (args %s)", cmd, args);
			return CMD_RES_UNKNOWN_COMMAND;
		}
	}
	else {
	}

	if (newCmd->handler) {
		commandResult_t res;
		res = newCmd->handler(newCmd->context, cmd, args, cmdFlags);
		return res;
	}
	return CMD_RES_UNKNOWN_COMMAND;
}


// execute a raw command - single string
commandResult_t CMD_ExecuteCommand(const char* s, int cmdFlags) {
	const char* p;
	const char* args;

	char copy[128];
	int len;
	//const char *org;

	if (s == 0 || *s == 0) {
		return CMD_RES_EMPTY_STRING;
	}

	while (isWhiteSpace(*s)) {
		s++;
	}
	if (*s == 0) {
		return CMD_RES_EMPTY_STRING;
	}
	if ((cmdFlags & COMMAND_FLAG_SOURCE_TCP) == 0) {
		ADDLOG_DEBUG(LOG_FEATURE_CMD, "cmd [%s]", s);
	}
	//org = s;

	// get the complete string up to whitespace.
	len = get_cmd(s, copy, sizeof(copy), 0);
	s += len;

	p = s;

	while (*p && isWhiteSpace(*p)) {
		p++;
	}
	args = p;

	return CMD_ExecuteCommandArgs(copy, args, cmdFlags);
}


