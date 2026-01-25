/* ************************************************************************
*   File: comm.cpp                                      Part of Bylins    *
*  Usage: Communication, socket handling, main(), central game loop       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  $Author$                                                        *
*  $Date$                                           *
*  $Revision$                                                       *
************************************************************************ */

/*
 * Compression support.  Currently could be used with:
 *
 *   MUD Client for Linux, by Erwin S. Andreasen
 *     http://www.andreasen.org/mcl/
 *
 *   mcclient, by Oliver 'Nemon@AR' Jowett
 *     http://homepages.ihug.co.nz/~icecube/compress/
 *
 * Contact them for help with the clients. Contact greerga@circlemud.org
 * for problems with the server end of the connection.  If you think you
 * have found a bug, please test another MUD for the same problem to see
 * if it is a client or server problem.
 */

#define __COMM_C__

#include "comm.h"

#include "engine/db/global_objects.h"
#include "gameplay/magic/magic.h"
#include "engine/db/world_objects.h"
#include "engine/db/world_characters.h"
#include "engine/entities/entities_constants.h"
#include "administration/shutdown_parameters.h"
#include "external_trigger.h"
#include "handler.h"
#include "gameplay/clans/house.h"
#include "engine/olc/olc.h"
#include "administration/ban.h"
#include "administration/proxy.h"
#include "gameplay/economics/exchange.h"
#include "gameplay/mechanics/title.h"
#include "gameplay/mechanics/depot.h"
#include "gameplay/mechanics/glory.h"
#include "utils/file_crc.h"
#include "gameplay/mechanics/corpse.h"
#include "gameplay/mechanics/glory_misc.h"
#include "gameplay/mechanics/glory_const.h"
#include "gameplay/mechanics/sets_drop.h"
#include "gameplay/communication/mail.h"
#include "gameplay/statistics/mob_stat.h"
#include "utils_char_obj.inl"
#include "utils/id_converter.h"
#include "utils/logger.h"
#include "engine/network/msdp/msdp.h"
#include "engine/network/msdp/msdp_constants.h"
#include "engine/entities/zone.h"
#include "engine/db/db.h"
#ifdef HAVE_SQLITE
#include "engine/db/sqlite_world_data_source.h"
#endif
#include "utils/utils.h"
#include "engine/core/conf.h"
#include "engine/ui/modify.h"
#include "gameplay/statistics/money_drop.h"
#include "gameplay/statistics/zone_exp.h"
#include "engine/core/iosystem.h"
#include "engine/ui/alias.h"
#include "engine/observability/perfetto_wrapper.h"

#include <third_party_libs/fmt/include/fmt/format.h>

#if defined WITH_SCRIPTING
#include "scripting.hpp"
#endif

#ifdef HAS_EPOLL
#include <sys/epoll.h>
#endif

#ifdef CIRCLE_MACINTOSH        // Includes for the Macintosh
# define SIGPIPE 13
# define SIGALRM 14
// GUSI headers
# include <sys/ioctl.h>
// Codewarrior dependant
# include <SIOUX.h>
# include <console.h>
#endif

#ifdef CIRCLE_WINDOWS        // Includes for Win32
# ifdef __BORLANDC__
#  include <dir.h>
# else				// MSVC
#  include <direct.h>
# endif
# include <mmsystem.h>
#endif                // CIRCLE_WINDOWS

#ifdef CIRCLE_AMIGA        // Includes for the Amiga
# include <sys/ioctl.h>
# include <clib/socket_protos.h>
#endif                // CIRCLE_AMIGA

#ifdef CIRCLE_ACORN        // Includes for the Acorn (RiscOS)
# include <socklib.h>
# include <inetlib.h>
# include <sys/ioctl.h>
#endif

/*
 * Note, most includes for all platforms are in sysdep.h.  The list of
 * files that is included is controlled by conf.h for that platform.
 */

#include "engine/network/telnet.h"

#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

//#include <third_party_libs/fmt/include/fmt/format.h>
//#include <sys/stat.h>

#include <string>
#include <exception>
//#include <iomanip>

#include <locale.h>

// for epoll
#ifdef HAS_EPOLL
#define MAXEVENTS 1024
#endif

// MSG_NOSIGNAL does not exists on OS X
#if defined(__APPLE__) || defined(__MACH__)
# ifndef MSG_NOSIGNAL
#   define MSG_NOSIGNAL SO_NOSIGPIPE
# endif
#endif

// п║я┌я─п╬п╨п╦

#define MXP_BEG "\x03"    /* becomes < */
#define MXP_END "\x04"    /* becomes > */
#define MXP_AMP "\x05"    /* becomes & */

// п║п╦п╪п╡п╬п╩я▀

#define MXP_BEGc '\x03'    /* becomes < */
#define MXP_ENDc '\x04'    /* becomes > */
#define MXP_AMPc '\x05'    /* becomes & */

// constructs an MXP tag with < and > around it

#define MXPTAG(arg) MXP_BEG arg MXP_END

#define ESC "\x1B"  /* esc character */

#define MXPMODE(arg) ESC "[" #arg "z"
extern void log_zone_count_reset();
//extern int perform_move(CharData *ch, int dir, int following, int checkmob, CharData *leader);
extern const char* build_datetime;
extern const char* revision;

// flags for show_list_to_char

/*
* Count number of mxp tags need converting
*    ie. < becomes &lt;
*        > becomes &gt;
*        & becomes &amp;
*/

int count_mxp_tags(const int bMXP, const char *txt, int length) {
	char c;
	const char *p;
	int count;
	int bInTag = false;
	int bInEntity = false;

	for (p = txt, count = 0;
		 length > 0;
		 p++, length--) {
		c = *p;

		if (bInTag)  /* in a tag, eg. <send> */
		{
			if (!bMXP)
				count--;     /* not output if not MXP */
			if (c == MXP_ENDc)
				bInTag = false;
		} /* end of being inside a tag */
		else if (bInEntity)  /* in a tag, eg. <send> */
		{
			if (!bMXP)
				count--;     /* not output if not MXP */
			if (c == ';')
				bInEntity = false;
		} /* end of being inside a tag */
		else
			switch (c) {

				case MXP_BEGc:bInTag = true;
					if (!bMXP)
						count--;     /* not output if not MXP */
					break;

				case MXP_ENDc:   /* shouldn't get this case */
					if (!bMXP)
						count--;     /* not output if not MXP */
					break;

				case MXP_AMPc:bInEntity = true;
					if (!bMXP)
						count--;     /* not output if not MXP */
					break;

				default:
					if (bMXP) {
						switch (c) {
							case '<':       /* < becomes &lt; */
							case '>':       /* > becomes &gt; */
								count += 3;
								break;

							case '&':count += 4;    /* & becomes &amp; */
								break;

							case '"':        /* " becomes &quot; */
								count += 5;
								break;

						} /* end of inner switch */
					}   /* end of MXP enabled */
			} /* end of switch on character */

	}   /* end of counting special characters */

	return count;
} /* end of count_mxp_tags */

void convert_mxp_tags(const int bMXP, char *dest, const char *src, int length) {
	char c;
	const char *ps;
	char *pd;
	int bInTag = false;
	int bInEntity = false;

	for (ps = src, pd = dest;
		 length > 0;
		 ps++, length--) {
		c = *ps;
		if (bInTag)  /* in a tag, eg. <send> */
		{
			if (c == MXP_ENDc) {
				bInTag = false;
				if (bMXP)
					*pd++ = '>';
			} else if (bMXP)
				*pd++ = c;  /* copy tag only in MXP mode */
		} /* end of being inside a tag */
		else if (bInEntity)  /* in a tag, eg. <send> */
		{
			if (bMXP)
				*pd++ = c;  /* copy tag only in MXP mode */
			if (c == ';')
				bInEntity = false;
		} /* end of being inside a tag */
		else
			switch (c) {
				case MXP_BEGc:bInTag = true;
					if (bMXP)
						*pd++ = '<';
					break;

				case MXP_ENDc:    /* shouldn't get this case */
					if (bMXP)
						*pd++ = '>';
					break;

				case MXP_AMPc:bInEntity = true;
					if (bMXP)
						*pd++ = '&';
					break;

				default:
					if (bMXP) {
						switch (c) {
							case '<':memcpy(pd, "&lt;", 4);
								pd += 4;
								break;

							case '>':memcpy(pd, "&gt;", 4);
								pd += 4;
								break;

							case '&':memcpy(pd, "&amp;", 5);
								pd += 5;
								break;

							case '"':memcpy(pd, "&quot;", 6);
								pd += 6;
								break;

							default:*pd++ = c;
								break;  /* end of default */

						} /* end of inner switch */
					} else
						*pd++ = c;  /* not MXP - just copy character */
					break;

			} /* end of switch on character */

	}   /* end of converting special characters */
} /* end of convert_mxp_tags */

/* ----------------------------------------- */

void our_terminate();

namespace {
static const bool SET_TERMINATE = nullptr != std::set_terminate(our_terminate);
}

void our_terminate() {
	static bool tried_throw = false;
	log("SET_TERMINATE: %s", SET_TERMINATE ? "true" : "false");

	try {
		if (!tried_throw) {
			tried_throw = true;
			throw;
		}

		log("No active exception");
	}
	catch (std::exception &e) {
		log("STD exception: %s", e.what());
	}
	catch (...) {
		log("Unknown exception :(");
	}
}

// externs
extern int num_invalid;
extern char *greetings;
extern const char *circlemud_version;
extern int circle_restrict;
extern bool no_world_checksum;
extern FILE *player_fl;
extern ush_int DFLT_PORT;
extern const char *DFLT_DIR;
extern const char *DFLT_IP;
extern const char *LOGNAME;
extern int max_playing;
extern int nameserver_is_slow;    // see config.cpp
extern int mana[];
extern const char *save_info_msg[];    // In olc.cpp
extern CharData *combat_list;
extern void tact_auction();
extern void log_code_date();

// local globals
DescriptorData *descriptor_list = nullptr;    // master desc list


int no_specials = 0;        // Suppress ass. of special routines
int max_players = 0;        // max descriptors available
int tics = 0;            // for extern checkpointing
int scheck = 0;
struct timeval null_time;    // zero-valued time structure
int dg_act_check;        // toggle for act_trigger
unsigned long cmd_cnt = 0;

// п╡п╫я┐п╪я▀ п╨п╬п╪п╫п╟я┌, пЁп╢п╣ я│я┌п╟п╡я▐я┌я│я▐ п╣п╩п╨п╦
const int vnum_room_new_year[31] =
	{4056,
	 5000,
	 6049,
	 7038,
	 8010,
	 9007,
	 66069,
	 60036,
	 18253,
	 63671,
	 34404,
	 13589,
	 27018,
	 63030,
	 30266,
	 69091,
	 77065,
	 76000,
	 49987,
	 25075,
	 72043,
	 75000,
	 85123,
	 35040,
	 73050,
	 60288,
	 24074,
	 62001,
	 32480,
	 68051,
	 85146};

const int len_array_gifts = 63;

const int vnum_gifts[len_array_gifts] = {27113,
										 2500,
										 2501,
										 2502,
										 2503,
										 2504,
										 2505,
										 2506,
										 2507,
										 2508,
										 2509,
										 2510,
										 2511,
										 2512,
										 2513,
										 2514,
										 2515,
										 2516,
										 2517,
										 2518,
										 2519,
										 2520,
										 2521,
										 2522,
										 2523,
										 2524,
										 2525,
										 2526,
										 2527,
										 2528,
										 2529,
										 2530,
										 2531,
										 2532,
										 2533,
										 2534,
										 2535,
										 2574,
										 2575,
										 2576,
										 2577,
										 2578,
										 2579,
										 2580,
										 2152,
										 2153,
										 2154,
										 2155,
										 2156,
										 2157,
										 10610,
										 10673,
										 10648,
										 10680,
										 10639,
										 10609,
										 10659,
										 10613,
										 10681,
										 10682,
										 10625,
										 10607,
										 10616
};

void gifts() {
	// п╡я▀п╠п╦я─п╟п╣п╪ я│п╩я┐я┤п╟п╧п╫я┐я▌ п╨п╬п╪п╫п╟я┌я┐ я│ п╣п╩п╨п╬п╧
	int rand_vnum_r = vnum_room_new_year[number(0, 30)];
	// п╡я▀п╠п╦я─п╟п╣п╪  я│п╩я┐я┤п╟п╧п╫я▀п╧ п©п╬п╢п╟я─п╬п╨
	int rand_vnum = vnum_gifts[number(0, len_array_gifts - 1)];
	ObjRnum rnum;
	if ((rnum = GetObjRnum(rand_vnum)) < 0) {
		log("п·я┬п╦п╠п╨п╟ п╡ я┌п╟п╠п╩п╦я├п╣ п²п⌠ п©п╬п╢п╟я─п╨п╬п╡!");
		return;
	}

	const auto obj_gift = world_objects.create_from_prototype_by_rnum(rnum);
	const auto obj_cont = world_objects.create_from_prototype_by_vnum(2594);

	// я│п╬п╥п╢п╟п╣п╪ я┐п©п╟п╨п╬п╡п╨я┐ п╢п╩я▐ п©п╬п╢п╟я─п╨п╟
	PlaceObjToRoom(obj_cont.get(), GetRoomRnum(rand_vnum_r));
	PlaceObjIntoObj(obj_gift.get(), obj_cont.get());
	CheckObjDecay(obj_gift.get());
	CheckObjDecay(obj_cont.get());
	log("п≈п╟пЁя─я┐п╤п╣п╫ п©п╬п╢п╟я─п╬п╨ п╡ п╨п╬п╪п╫п╟я┌я┐: %d, п╬п╠я┼п╣п╨я┌: %d", rand_vnum_r, rand_vnum);
}

// functions in this file
RETSIGTYPE reap(int sig);
RETSIGTYPE checkpointing(int sig);
RETSIGTYPE hupsig(int sig);
ssize_t perform_socket_read(socket_t desc, char *read_point, size_t space_left);
ssize_t perform_socket_write(socket_t desc, const char *txt, size_t length);
void sanity_check(void);
void circle_sleep(struct timeval *timeout);
void stop_game(ush_int port);
void signal_setup(void);
#ifdef HAS_EPOLL
void game_loop(int epoll, socket_t mother_desc);
int new_descriptor(int epoll, socket_t s);
#else
void game_loop(socket_t mother_desc);
int new_descriptor(socket_t s);
#endif

socket_t init_socket(ush_int port);

int get_max_players();
void timeadd(struct timeval *sum, struct timeval *a, struct timeval *b);
void nonblock(socket_t s);
struct in_addr *get_bind_addr();
int parse_ip(const char *addr, struct in_addr *inaddr);
int set_sendbuf(socket_t s);

#if defined(POSIX)
sigfunc *my_signal(int signo, sigfunc *func);
#endif

void show_string(DescriptorData *d, char *input);
void redit_save_to_disk(int zone_num);
void oedit_save_to_disk(int zone_num);
void medit_save_to_disk(int zone_num);
void zedit_save_to_disk(int zone_num);
void Crash_ldsave(CharData *ch);
void Crash_save_all_rent();
long GetExpUntilNextLvl(CharData *ch, int level);
unsigned long TxtToIp(const char *text);

#ifdef __CXREF__
#undef FD_ZERO
#undef FD_SET
#undef FD_ISSET
#undef FD_CLR
#define FD_ZERO(x)
#define FD_SET(x, y) 0
#define FD_ISSET(x, y) 0
#define FD_CLR(x, y)
#endif

const char will_msdp[] = {char(IAC), char(WILL), char(::msdp::constants::TELOPT_MSDP)};


/***********************************************************************
*  main game loop and related stuff                                    *
***********************************************************************/

#if defined(_MSC_VER) || defined(CIRCLE_MACINTOSH)

/*
 * Windows doesn't have gettimeofday, so we'll simulate it.
 * The Mac doesn't have gettimeofday either.
 * Borland C++ warns: "Undefined structure 'timezone'"
 */
void gettimeofday(struct timeval *t, void *dummy)
{
#if defined(_MSC_VER)
	DWORD millisec = GetTickCount();
#elif defined(CIRCLE_MACINTOSH)
	unsigned long int millisec;
	millisec = (int)((float) TickCount() * 1000.0 / 60.0);
#endif

	t->tv_sec = (int)(millisec / 1000);
	t->tv_usec = (millisec % 1000) * 1000;
}

#endif                // CIRCLE_WINDOWS || CIRCLE_MACINTOSH

#include <iostream>

int main_function(int argc, char **argv) {
#ifdef TEST_BUILD
	// п╢п╩я▐ п╫п╬я─п╪п╟п╩я▄п╫п╬пЁп╬ п╡я▀п╡п╬п╢п╟ я─я┐я│я│п╨п╬пЁп╬ я┌п╣п╨я│я┌п╟ п©п╬п╢ cygwin 1.7 п╦ п╡я▀я┬п╣
	setlocale(LC_CTYPE, "ru_RU.KOI8-R");
#endif

#ifdef _MSC_VER        // Includes MSVC
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG); //assert in debug window
#endif

#ifdef OS_UNIX
	extern char *malloc_options;
	malloc_options = "A";
#endif

	ush_int port;
	int pos = 1;
	const char *dir;
	char cwd[256];

	// Initialize these to check for overruns later.
	plant_magic(buf);
	plant_magic(buf1);
	plant_magic(buf2);
	plant_magic(arg);

	port = DFLT_PORT;
	dir = "lib";


	while ((pos < argc) && (*(argv[pos]) == '-')) {
		switch (*(argv[pos] + 1)) {
			case 'o':
				if (*(argv[pos] + 2))
					LOGNAME = argv[pos] + 2;
				else if (++pos < argc)
					LOGNAME = argv[pos];
				else {
					puts("SYSERR: File name to log to expected after option -o.");
					exit(1);
				}
				break;
			case 'c': scheck = 1;
				puts("Syntax check mode enabled.");
				break;

			case 'r': circle_restrict = 1;
				puts("Restricting game -- no new players allowed.");
				break;

			case 's': no_specials = 1;
				puts("Suppressing assignment of special routines.");
				break;
			case 'C':
				no_world_checksum = true;
				puts("World checksum calculation disabled.");
				break;
			case 'd':
				if (*(argv[pos] + 2))
					dir = argv[pos] + 2;
				else if (++pos < argc)
					dir = argv[pos];
				else {
					puts("SYSERR: Directory arg expected after option -d.");
					exit(1);
				}
			break;
			case 'h':
				// From: Anil Mahajan <amahajan@proxicom.com>
				printf("Usage: %s [-c] [-q] [-r] [-s] [port #] [-D msdp]\n"
					   "  -c             Enable syntax check mode.\n"
					   "  -d <directory> Specify library directory (defaults to 'lib').\n"
					   "  -h             Print this command line argument help.\n"
					   "  -o <file>      Write log to <file> instead of stderr.\n"
					   "  -r             Restrict MUD -- no new players allowed.\n"
					   "  -s             Suppress special procedure assignments.\n"
				   "\n"
				   	   "  -S <database>  Use SQLite database for world loading.\n", argv[0]);
				exit(0);

			default: printf("SYSERR: Unknown option -%c in argument string.\n", *(argv[pos] + 1));
				printf("Usage: %s [-c] [-q] [-r] [-s] [port #] [-D msdp]\n"
					   "  -c             Enable syntax check mode.\n"
					   "  -d <directory> Specify library directory (defaults to 'lib').\n"
					   "  -h             Print this command line argument help.\n"
					   "  -o <file>      Write log to <file> instead of stderr.\n"
					   "  -r             Restrict MUD -- no new players allowed.\n"
					   "  -s             Suppress special procedure assignments.\n"
				   "\n"
				   	   "  -S <database>  Use SQLite database for world loading.\n", argv[0]);
				exit(1);
			break;
		}
		pos++;
	}

	if (pos < argc) {
		if (!a_isdigit(*argv[pos])) {
			printf("Usage: %s [-c] [-q] [-r] [-s] [port #] [-D msdp]\r\n", argv[0]);
			exit(1);
		} else if ((port = atoi(argv[pos])) <= 1024) {
			printf("SYSERR: Illegal port number %d.\r\n", port);
			exit(1);
		}
	}

	/*
	 * Moved here to distinguish command line options and to show up
	 * in the log if stderr is redirected to a file.
	 */
	printf("%s\r\n", circlemud_version);
	printf("%s\r\n", DG_SCRIPT_VERSION);
	if (getcwd(cwd, sizeof(cwd))) {};
	printf("Current directory '%s' using '%s' as data directory.\r\n", cwd, dir);
	if (chdir(dir) < 0) {
		perror("\r\nSYSERR: Fatal error changing to data directory");
		exit(1);
	}
	runtime_config.load();
	if (runtime_config.msdp_debug()) {
		msdp::debug(true);
	}
	// All arguments have been parsed, try to open log file.
	runtime_config.setup_logs();
	logfile = runtime_config.logs(SYSLOG).handle();
	
	// Initialize Perfetto if enabled
#ifdef WITH_PERFETTO
	if (runtime_config.perfetto().enabled()) {
		auto &perfetto = observability::PerfettoWrapper::instance();
		std::string trace_file = runtime_config.perfetto().trace_file();
		
		// Support environment variable override
		const char* env_trace = getenv("PERFETTO_TRACE_FILE");
		if (env_trace) {
			trace_file = env_trace;
		}
		
		if (perfetto.initialize(trace_file)) {
			log("Perfetto initialized successfully: trace_file=%s", trace_file.c_str());
		} else {
			log("WARNING: Failed to initialize Perfetto");
		}
	}
#endif
	
	log_code_date();
	printf("Code version %s, revision: %s\r\n", build_datetime, revision);
	if (scheck) {
		game_loader.BootWorld();
		printf("Done.");
	} else {
		printf("Running game on port %d.\r\n", port);

		// я│я┌п╩я▄ п╦ п╠я┐я│я┌ я▌п╥п╟я▌я┌я│я▐ я┐п╤п╣ п╫п╣п╪п╟п╩п╬ пЁп╢п╣, п╟ п©я─п╬ п╦я┘ я█п╨я│п©п╣я┬п╣п╫я▀ п╫п╦п╨я┌п╬ п╫п╣ п╢я┐п╪п╟п╣я┌
		// п©п╬п╨п╟ я┘п╬я┌я▐ п╠я▀ я│я┌п╩я▄п╫я▀п╣ п╩п╬п╡п╦я┌я▄ п╦ п©я─п╬я│я┌п╬ п╩п╬пЁп╦я─п╬п╡п╟я┌я▄ я└п╟п╨я┌ я┌п╬пЁп╬, я┤я┌п╬ п╪я▀ п╡я▀я┬п╩п╦
		// п©п╬ я█п╨я│п╣п©я┬п╣п╫я┐ п╢п╩я▐ я┐п╢п╬п╠я│я┌п╡п╟ п╬я┌п╩п╟п╢п╨п╦ п╦ я┬я┌п╟я┌п╫п╬пЁп╬ я│п╠я─п╬я│п╟ я│п╦я│п╩п╬пЁп╟ п╡ я└п╟п╧п╩, я┌.п╨. п╡ п╨п╬я─п╣ п╠я┐п╢п╣я┌ я└п╦пЁ
		stop_game(port);
	}

	return 0;
}

void stop_game(ush_int port) {
	socket_t mother_desc;
#ifdef HAS_EPOLL
	int epoll;
	struct epoll_event event;
	DescriptorData *mother_d;
#endif

	// We don't want to restart if we crash before we get up.
	touch(KILLSCRIPT_FILE);
	touch("../.crash");

	log("Finding player limit.");
	max_players = get_max_players();

	log("Opening mother connection.");
	mother_desc = init_socket(port);
#if defined WITH_SCRIPTING
	scripting::init();
#endif
	BootMudDataBase();
#if defined(CIRCLE_UNIX) || defined(CIRCLE_MACINTOSH)
	log("Signal trapping.");
	signal_setup();
#endif

	// If we made it this far, we will be able to restart without problem.
	remove(KILLSCRIPT_FILE);

	log("Entering game loop.");

#ifdef HAS_EPOLL
	log("Polling using epoll.");
	epoll = epoll_create1(0);
	if (epoll == -1) {
		perror(fmt::format("EPOLL: epoll_create1() failed in {}() at {}:{}",
							  __func__, __FILE__, __LINE__).c_str());
		return;
	}
	// п╫п╣п╬п╠я┘п╬п╢п╦п╪п╬, я┌.п╨. п╡ event.data п╪я▀ п╪п╬п╤п╣п╪ я┘я─п╟п╫п╦я┌я▄ п╩п╦п╠п╬ ptr, п╩п╦п╠п╬ fd.
	// п╟ п©п╬я│п╨п╬п╩я▄п╨я┐ п╢п╩я▐ п╨п╩п╦п╣п╫я┌я│п╨п╦я┘ я│п╬п╨п╣я┌п╬п╡ п╫п╟п╪ п╫я┐п╤п╫я▀ ptr, я┌п╬ п╦ п╢п╩я▐ я─п╬п╢п╦я┌п╣п╩я▄я│п╨п╬пЁп╬
	// п╢п╣я│п╨я─п╦п©я┌п╬я─п╟, пЁп╢п╣ п╫п╟п╪ п╫п╟п╬п╠п╬я─п╬я┌ п╫я┐п╤п╣п╫ fd, п©я─п╦п╢п╣я┌я│я▐ я│п╬п╥п╢п╟я┌я▄ п©я│п╣п╡п╢п╬я│я┌я─я┐п╨я┌я┐я─я┐,
	// п╡ п╨п╬я┌п╬я─п╬п╧ п╦п╫п╦я├п╦п╟п╩п╦п╥п╦я─я┐п╣п╪ я┌п╬п╩я▄п╨п╬ п©п╬п╩п╣ descriptor
	mother_d = (DescriptorData *) calloc(1, sizeof(DescriptorData));
	mother_d->descriptor = mother_desc;
	event.data.ptr = mother_d;
	event.events = EPOLLIN;
	if (epoll_ctl(epoll, EPOLL_CTL_ADD, mother_desc, &event) == -1) {
		perror(fmt::format("EPOLL: epoll_ctl() failed on EPOLL_CTL_ADD mother_desc in {}() at {}:{}",
							  __func__, __FILE__, __LINE__).c_str());
		return;
	}

	game_loop(epoll, mother_desc);
#else
	log("Polling using select().");
	game_loop(mother_desc);
#endif

	FlushPlayerIndex();

	// я┘я─п╟п╫я▀ п╫п╟п╢п╬ я│п╣п╧п╡п╦я┌я▄ п╢п╬ Crash_save_all_rent(), п╦п╫п╟я┤п╣ п╠я┐п╢п╣п╪ п╠я─п╟я┌я▄ п╠п╟п╠п╩п╬ я┐ я┤п╟я─п╟ п©я─п╦ п╥п╟п©п╦я│п╦
	// я┐п╤п╣ п©п╬я│п╩п╣ п╣пЁп╬ я█п╨я│я┌я─п╟п╨я┌п╟, п╦ я┤я┌п╬ я┌п╟п╪ п╠я┐п╢п╣я┌ я┘п╥...
	Depot::save_all_online_objs();
	Depot::save_timedata();

	if (shutdown_parameters.need_normal_shutdown()) {
		log("Entering Crash_save_all_rent");
		Crash_save_all_rent();    //save all
	}

	SaveGlobalUID();
	exchange_database_save();    //exchange database save

	Clan::ChestUpdate();
	Clan::SaveChestAll();
	Clan::ClanSave();
	save_clan_exp();
	ClanSystem::save_ingr_chests();
	ClanSystem::save_chest_log();

	TitleSystem::save_title_list();
	RegisterSystem::save();
	Glory::save_glory();
	GloryConst::save();
	GloryMisc::save_log();
	GlobalDrop::save();// я│п╬я┘я─п╟п╫я▐п╣п╪ я│я┤п╣я┌я┤п╦п╨п╦ пЁп╩п╬п╠п╟п╩п╢я─п╬п©п╟
	MoneyDropStat::print_log();
	ZoneExpStat::print_log();
	print_rune_log();
	ZoneTrafficSave();
#if defined WITH_SCRIPTING
	//scripting::terminate();
#endif
	mob_stat::Save();
	SetsDrop::save_drop_table();
	mail::save();
	char_stat::LogClassesExpStat();

	log("Closing all sockets.");
#ifdef HAS_EPOLL
	while (descriptor_list)
		close_socket(descriptor_list, true, epoll, nullptr, 0);
#else
	while (descriptor_list)
		close_socket(descriptor_list, true);
#endif
	// п╢п╬п╩п╤п╫п╬ п╦п╢я┌п╦ п©п╬я│п╩п╣ п╢п╦я│п╨п╬п╫п╣п╨я┌п╟ п©п╩п╣п╣я─п╬п╡
	FileCRC::save(true);

	CLOSE_SOCKET(mother_desc);
#ifdef HAS_EPOLL
	free(mother_d);
#endif
	if (!shutdown_parameters.reboot_is_2()
		&& olc_save_list)    // Don't save zones.
	{
		struct olc_save_info *entry, *next_entry;
		int rznum;

		for (entry = olc_save_list; entry; entry = next_entry) {
			next_entry = entry->next;
			if (entry->type < 0 || entry->type > 4) {
				sprintf(buf, "OLC: Illegal save type %d!", entry->type);
				log("%s", buf);
			} else if ((rznum = get_zone_rnum_by_zone_vnum(entry->zone)) == -1) {
				sprintf(buf, "OLC: Illegal save zone %d!", entry->zone);
				log("%s", buf);
			} else if (rznum < 0 || rznum >= static_cast<int>(zone_table.size())) {
				sprintf(buf, "OLC: Invalid real zone number %d!", rznum);
				log("%s", buf);
			} else {
				sprintf(buf, "OLC: Reboot saving %s for zone %d.",
						save_info_msg[(int) entry->type], zone_table[rznum].vnum);
				log("%s", buf);
				switch (entry->type) {
					case OLC_SAVE_ROOM: redit_save_to_disk(rznum);
						break;
					case OLC_SAVE_OBJ: oedit_save_to_disk(rznum);
						break;
					case OLC_SAVE_MOB: medit_save_to_disk(rznum);
						break;
					case OLC_SAVE_ZONE: zedit_save_to_disk(rznum);
						break;
					default: log("Unexpected olc_save_list->type");
						break;
				}
			}
		}
	}
	if (shutdown_parameters.reboot_after_shutdown()) {
		log("Rebooting.");
		exit(52);    // what's so great about HHGTTG, anyhow?
	}
	
	// Shutdown Perfetto
#ifdef WITH_PERFETTO
	if (runtime_config.perfetto().enabled()) {
		log("Shutting down Perfetto...");
		observability::PerfettoWrapper::instance().shutdown();
	}
#endif
	
	log("Normal termination of game.");
}

/*
 * init_socket sets up the mother descriptor - creates the socket, sets
 * its options up, binds it, and listens.
 */
socket_t init_socket(ush_int port) {
	socket_t s;
	int opt;
	struct sockaddr_in sa;

#ifdef CIRCLE_WINDOWS
	{
		WORD wVersionRequested;
		WSADATA wsaData;

		wVersionRequested = MAKEWORD(1, 1);

		if (WSAStartup(wVersionRequested, &wsaData) != 0)
		{
			log("SYSERR: WinSock not available!");
			exit(1);
		}
		if ((wsaData.iMaxSockets - 4) < max_players)
		{
			max_players = wsaData.iMaxSockets - 4;
		}
		log("Max players set to %d", max_players);

		if ((s = socket(PF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR)
		{
			log("SYSERR: Error opening network connection: Winsock error #%d", WSAGetLastError());
			exit(1);
		}
	}
#else
	/*
	 * Should the first argument to socket() be AF_INET or PF_INET?  I don't
	 * know, take your pick.  PF_INET seems to be more widely adopted, and
	 * Comer (_Internetworking with TCP/IP_) even makes a point to say that
	 * people erroneously use AF_INET with socket() when they should be using
	 * PF_INET.  However, the man pages of some systems indicate that AF_INET
	 * is correct; some such as ConvexOS even say that you can use either one.
	 * All implementations I've seen define AF_INET and PF_INET to be the same
	 * number anyway, so the point is (hopefully) moot.
	 */

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("SYSERR: Error creating socket");
		exit(1);
	}
#endif                // CIRCLE_WINDOWS

#if defined(SO_REUSEADDR) && !defined(CIRCLE_MACINTOSH)
	opt = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
		perror("SYSERR: setsockopt REUSEADDR");
		exit(1);
	}
#endif

	set_sendbuf(s);

	/*
	 * The GUSI sockets library is derived from BSD, so it defines
	 * SO_LINGER, even though setsockopt() is unimplimented.
	 *	(from Dean Takemori <dean@UHHEPH.PHYS.HAWAII.EDU>)
	 */
#if defined(SO_LINGER) && !defined(CIRCLE_MACINTOSH)
	{
		struct linger ld;

		ld.l_onoff = 0;
		ld.l_linger = 0;
		if (setsockopt(s, SOL_SOCKET, SO_LINGER, (char *) &ld, sizeof(ld)) < 0)
			perror("SYSERR: setsockopt SO_LINGER");    // Not fatal I suppose.
	}
#endif

	// Clear the structure
	memset((char *) &sa, 0, sizeof(sa));

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr = *(get_bind_addr());

	if (bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("SYSERR: bind");
		CLOSE_SOCKET(s);
		exit(1);
	}
	nonblock(s);
	listen(s, 5);
	return (s);
}

int get_max_players(void) {
	return (max_playing);
}

int shutting_down(void) {
	static int lastmessage = 0;
	int wait;

	if (shutdown_parameters.no_shutdown()) {
		return false;
	}

	if (!shutdown_parameters.get_shutdown_timeout()
		|| time(nullptr) >= shutdown_parameters.get_shutdown_timeout()) {
		return true;
	}

	if (lastmessage == shutdown_parameters.get_shutdown_timeout()
		|| lastmessage == time(nullptr)) {
		return false;
	}
	wait = shutdown_parameters.get_shutdown_timeout() - time(nullptr);

	if (wait == 10 || wait == 30 || wait == 60 || wait == 120 || wait % 300 == 0) {
		if (shutdown_parameters.reboot_after_shutdown()) {
			remove("../.crash");
			sprintf(buf, "п÷п∙п═п∙п≈п░п⌠п═пёп≈п п░ я┤п╣я─п╣п╥ ");
		} else {
			remove("../.crash");
			sprintf(buf, "п·п║п╒п░п²п·п▓п п░ я┤п╣я─п╣п╥ ");
		}
		if (wait < 60)
			sprintf(buf + strlen(buf), "%d %s.\r\n", wait, GetDeclensionInNumber(wait, EWhat::kSec));
		else
			sprintf(buf + strlen(buf), "%d %s.\r\n", wait / 60, GetDeclensionInNumber(wait / 60, EWhat::kMinU));
		SendMsgToAll(buf);
		lastmessage = time(nullptr);
		// п╫п╟ п╢п╣я│я▐я┌п╬п╧ я│п╣п╨я┐п╫п╢п╣ п╥п╟я│п╣п╧п╡п╦п╪ п╫я┐п╤п╫п╬п╣ п╫п╟п╪ п╡ я│п╦я│п╩п╬пЁ
		if (wait == 10)
			log_zone_count_reset();
	}
	return (false);
}

void log_zone_count_reset() {
	for (auto & i : zone_table) {
		sprintf(buf, "Zone: %d, count_reset: %d", i.vnum, i.count_reset);
		log("%s", buf);
	}
}

#ifdef HAS_EPOLL
inline void process_io(int epoll, socket_t mother_desc, struct epoll_event *events)
#else
inline void process_io(fd_set input_set, fd_set output_set, fd_set exc_set, fd_set,
					   socket_t mother_desc, int maxdesc)
#endif
{
	DescriptorData *d, *next_d;
	char comm[kMaxInputLength];
	int aliased;

#ifdef HAS_EPOLL
	int n, i;

	// п╫п╣п╠п╩п╬п╨п╦я─я┐я▌я┴п╣ п©п╬п╩я┐я┤п╟п╣п╪ п╫п╬п╡я▀п╣ я│п╬п╠я▀я┌п╦я▐
	n = epoll_wait(epoll, events, MAXEVENTS, 0);
	if (n == -1) {
		perror(fmt::format("EPOLL: epoll_ctl() failed on EPOLL_CTL_ADD mother_desc in {}() at {}:{}",
						   __func__, __FILE__, __LINE__).c_str());
		std::string err = fmt::format("EPOLL: epoll_wait() failed in {}() at {}:{}",
										 __func__, __FILE__, __LINE__);
		log("%s", err.c_str());
		perror(err.c_str());
		return;
	}

	for (i = 0; i < n; i++)
		if (events[i].events & EPOLLIN) {
			d = (DescriptorData *) events[i].data.ptr;
			if (d == nullptr)
				continue;
			if (mother_desc == d->descriptor) // я│п╬п╠я▀я┌п╦п╣ п╫п╟ mother_desc: п©я─п╦п╫п╦п╪п╟п╣п╪ п╡я│п╣ п╤п╢я┐я┴п╦п╣ я│п╬п╣п╢п╦п╫п╣п╫п╦я▐
			{
				int desc;
				do
					desc = new_descriptor(epoll, mother_desc);
				while (desc > 0 || desc == -3);
			} else // я│п╬п╠я▀я┌п╦п╣ п╫п╟ п╨п╩п╦п╣п╫я┌я│п╨п╬п╪ п╢п╣я│п╨я─п╦п©я┌п╬я─п╣: п©п╬п╩я┐я┤п╟п╣п╪ п╢п╟п╫п╫я▀п╣ п╦ п╥п╟п╨я─я▀п╡п╟п╣п╪ я│п╬п╨п╣я┌, п╣я│п╩п╦ EOF
			if (iosystem::process_input(d) < 0)
				close_socket(d, false, epoll, events, n);
		} else if (events[i].events & !EPOLLOUT & !EPOLLIN) // я┌я┐я┌ п╩п╬п╡п╦п╪ п╡я│п╣ я│п╬п╠я▀я┌п╦я▐, п╦п╪п╣я▌я┴п╦п╣ я└п╩п╟пЁп╦ п╨я─п╬п╪п╣ in п╦ out
		{
			// п╫п╟п╢п╬ п╠я┐п╢п╣я┌ п©п╬п╪п╬п╫п╦я┌п╬я─п╦я┌я▄ я│п╦я│п╩п╬пЁ п╫п╟ п©я─п╣п╢п╪п╣я┌ я█я┌п╦я┘ я│п╬п╬п╠я┴п╣п╫п╦п╧
			char tmp[kMaxInputLength];
			snprintf(tmp, sizeof(tmp), "EPOLL: Got event %u in {}() at %s:%s:%d",
					 static_cast<unsigned>(events[i].events),
					 __func__, __FILE__, __LINE__);
			log("%s", tmp);
		}
#else
	// Poll (without blocking) for new input, output, and exceptions
	if (select(maxdesc + 1, &input_set, &output_set, &exc_set, &null_time)
			< 0)
	{
		perror("SYSERR: Select poll");
		return;
	}
	// If there are new connections waiting, accept them.
	if (FD_ISSET(mother_desc, &input_set))
		new_descriptor(mother_desc);

	// Kick out the freaky folks in the exception set and marked for close
	for (d = descriptor_list; d; d = next_d)
	{
		next_d = d->next;
		if (FD_ISSET(d->descriptor, &exc_set))
		{
			FD_CLR(d->descriptor, &input_set);
			FD_CLR(d->descriptor, &output_set);
			close_socket(d, true);
		}
	}

	// Process descriptors with input pending
	for (d = descriptor_list; d; d = next_d)
	{
		next_d = d->next;
		if (FD_ISSET(d->descriptor, &input_set))
			if (iosystem::process_input(d) < 0)
				close_socket(d, false);
	}
#endif

	// Process commands we just read from process_input
	for (d = descriptor_list; d; d = next_d) {
		next_d = d->next;

		/*
		 * Not combined to retain --(d->wait) behavior. -gg 2/20/98
		 * If no wait state, no subtraction.  If there is a wait
		 * state then 1 is subtracted. Therefore we don't go less
		 * than 0 ever and don't require an 'if' bracket. -gg 2/27/99
		 */
		if (d->character) {
			d->character->punctual_wait -=
				(d->character->punctual_wait > 0 ? 1 : 0);
			if (IS_IMMORTAL(d->character)) {
				d->character->zero_wait();
			}
			if (IS_IMMORTAL(d->character)
				|| d->character->punctual_wait < 0) {
				d->character->punctual_wait = 0;
			}
			if (d->character->get_wait()) {
				continue;
			}
		}
		// п╗п╬п╠ п╡ п╪п╣п╫я▌ п╢п╬п╩пЁп╬ п╫п╣ я│п╦п╢п╣п╩п╦ !
		if (!get_from_q(&d->input, comm, &aliased)) {
			if (d->state != EConState::kPlaying &&
				d->state != EConState::kDisconnect &&
				time(nullptr) - d->input_time > 300 && d->character && !IS_GOD(d->character))
#ifdef HAS_EPOLL
				close_socket(d, true, epoll, events, n);
#else
			close_socket(d, true);
#endif
			continue;
		}
		d->input_time = time(nullptr);
		if (d->character)    // Reset the idle timer & pull char back from void if necessary
		{
			d->character->char_specials.timer = 0;
			if (d->state == EConState::kPlaying && d->character->get_was_in_room() != kNowhere) {
				if (d->character->in_room != kNowhere)
					char_from_room(d->character);
				char_to_room(d->character, d->character->get_was_in_room());
				d->character->set_was_in_room(kNowhere);
				act("$n п╡п╣я─п╫я┐п╩$u.", true, d->character.get(), 0, 0, kToRoom | kToArenaListen);
				d->character->set_wait(1u);
			}
		}
		d->has_prompt = 0;
		if (d->showstr_count && d->state != EConState::kDisconnect && d->state != EConState::kClose)    // Reading something w/ pager
		{
			show_string(d, comm);
		} else if (d->writer && d->state != EConState::kDisconnect && d->state != EConState::kClose) {
			string_add(d, comm);
		} else if (d->state != EConState::kPlaying)    // In menus, etc.
		{
			nanny(d, comm);
		} else    // else: we're playing normally.
		{
			if (aliased)    // To prevent recursive aliases.
				d->has_prompt = 1;    // To get newline before next cmd output.
			else if (PerformAlias(d, comm))    // Run it through aliasing system
				get_from_q(&d->input, comm, &aliased);
			command_interpreter(d->character.get(), comm);    // Send it to interpreter
			cmd_cnt++;
		}
	}

#ifdef HAS_EPOLL
	for (i = 0; i < n; i++) {
		d = (DescriptorData *) events[i].data.ptr;
		if (d == nullptr)
			continue;
		if ((events[i].events & EPOLLOUT) && (!d->has_prompt || *(d->output))) {
			if (iosystem::process_output(d) < 0) // я│п╬п╨п╣я┌ я┐п╪п╣я─
				close_socket(d, false, epoll, events, n);
			else
				d->has_prompt = 1;   // п©я─п╦п╥п╫п╟п╨ я┌п╬пЁп╬, я┤я┌п╬ п©я─п╬п╪п©я┌ я┐п╤п╣ п╡я▀п╡п╬п╢п╦п╩
			// я│п╩п╣п╢я┐я▌я┴п╦п╧ п©п╬я│п╩п╣ п╨п╬п╪п╟п╫п╢я▀ п╦п╩п╦ п╬я┤п╣я─п╣п╢п╫п╬п╧
			// п©п╬я─я├п╦п╦ п╡я▀п╡п╬п╢п╟
		}
	}
#else
	for (d = descriptor_list; d; d = next_d)
	{
		next_d = d->next;
		if ((!d->has_prompt || *(d->output)) && FD_ISSET(d->descriptor, &output_set))
		{
			if (iosystem::process_output(d) < 0)
				close_socket(d, false);	// п╥п╟п╨я─я▀п╩ я│п╬п╣п╢п╦п╫п╣п╫п╦п╣
			else
				d->has_prompt = 1;	// п©я─п╦п╥п╫п╟п╨ я┌п╬пЁп╬, я┤я┌п╬ п©я─п╬п╪п©я┌ я┐п╤п╣ п╡я▀п╡п╬п╢п╦п╩
			// я│п╩п╣п╢я┐я▌я┴п╦п╧ п©п╬я│п╩п╣ п╨п╬п╪п╟п╫п╢я▀ п╦п╩п╦ п╬я┤п╣я─п╣п╢п╫п╬п╧
			// п©п╬я─я├п╦п╦ п╡я▀п╡п╬п╢п╟
		}
	}
#endif

// я┌я┐я┌ п╠я▀п╩ п╨я┐я│п╬п╨ я│я┌п╟я─п╬пЁп╬ п╨п╬п╢п╟ п╡ #if 0 ... #endif. я┐п╠я─п╟п╩, я┤я┌п╬п╠я▀ п╪п╣п╫я▄я┬п╣ я┘п╩п╟п╪п╟ п╠я▀п╩п╬.
// п╣я│п╩п╦ п©п╬п╫п╟п╢п╬п╠п╦я┌я│я▐, п╡п╣я─п╫п╣п╪ п╦п╥ п╦я│я┌п╬я─п╦п╦.

	// Kick out folks in the CON_CLOSE or CON_DISCONNECT state
	for (d = descriptor_list; d; d = next_d) {
		next_d = d->next;
		if (d->state == EConState::kClose || d->state == EConState::kDisconnect)
#ifdef HAS_EPOLL
			close_socket(d, false, epoll, events, n);
#else
		close_socket(d, false);
#endif
	}

}

/*
 * game_loop contains the main loop which drives the entire MUD.  It
 * cycles once every 0.10 seconds and is responsible for accepting new
 * connections, polling existing connections for input, dequeueing
 * output and sending it out to players, and calling "heartbeat" functions
 * such as mobile_activity().
 */
#ifdef HAS_EPOLL
void game_loop(int epoll, socket_t mother_desc)
#else
void game_loop(socket_t mother_desc)
#endif
{
	printf("Game started.\n");

#ifdef HAS_EPOLL
	struct epoll_event *events;
#else
	DescriptorData *d;
	fd_set input_set, output_set, exc_set, null_set;
	int maxdesc;
#endif

	struct timeval last_time, opt_time, process_time, temp_time;
	int missed_pulses = 0;
	struct timeval before_sleep, now, timeout;

	// initialize various time values
	null_time.tv_sec = 0;
	null_time.tv_usec = 0;
	opt_time.tv_usec = kOptUsec;
	opt_time.tv_sec = 0;

#ifdef HAS_EPOLL
	events = (struct epoll_event *) calloc(1, MAXEVENTS * sizeof(struct epoll_event));
#else
	FD_ZERO(&null_set);
#endif

	gettimeofday(&last_time, (struct timezone *) 0);

	// The Main Loop.  The Big Cheese.  The Top Dog.  The Head Honcho.  The..
	while (!shutting_down())    // Sleep if we don't have any connections
	{
		if (descriptor_list == nullptr) {
			log("No connections.  Going to sleep.");
#ifdef HAS_EPOLL
			if (epoll_wait(epoll, events, MAXEVENTS, -1) == -1)
#else
				FD_ZERO(&input_set);
				FD_SET(mother_desc, &input_set);
				if (select(mother_desc + 1, &input_set, (fd_set *) 0, (fd_set *) 0, nullptr) < 0)
#endif
			{
				if (errno == EINTR)
					log("Waking up to process signal.");
				else
#ifdef HAS_EPOLL
					perror(fmt::format("EPOLL: blocking epoll_wait() failed in {}() at {}:{}",
										  __func__, __FILE__, __LINE__).c_str());
#else
				perror("SYSERR: Select coma");
#endif
			} else
				log("New connection.  Waking up.");
			gettimeofday(&last_time, (struct timezone *) 0);
		}

#ifndef HAS_EPOLL
		// Set up the input, output, and exception sets for select().
		FD_ZERO(&input_set);
		FD_ZERO(&output_set);
		FD_ZERO(&exc_set);
		FD_SET(mother_desc, &input_set);

		maxdesc = mother_desc;
		for (d = descriptor_list; d; d = d->next)
		{
#ifndef CIRCLE_WINDOWS
			if (d->descriptor > maxdesc)
				maxdesc = d->descriptor;
#endif
			FD_SET(d->descriptor, &input_set);
			FD_SET(d->descriptor, &output_set);
			FD_SET(d->descriptor, &exc_set);
		}
#endif

		{
			/*
			 * At this point, we have completed all input, output and heartbeat
			 * activity from the previous iteration, so we have to put ourselves
			 * to sleep until the next 0.1 second tick.  The first step is to
			 * calculate how long we took processing the previous iteration.
			 */

			gettimeofday(&before_sleep, (struct timezone *) 0);    // current time
			timediff(&process_time, &before_sleep, &last_time);

			/*
			 * If we were asleep for more than one pass, count missed pulses and sleep
			 * until we're resynchronized with the next upcoming pulse.
			 */
			if (process_time.tv_sec == 0 && process_time.tv_usec < kOptUsec) {
				missed_pulses = 0;
			} else {
				missed_pulses = process_time.tv_sec * kPassesPerSec;
				missed_pulses += process_time.tv_usec / kOptUsec;
				process_time.tv_sec = 0;
				process_time.tv_usec = process_time.tv_usec % kOptUsec;
			}

			// Calculate the time we should wake up
			timediff(&temp_time, &opt_time, &process_time);
			timeadd(&last_time, &before_sleep, &temp_time);

			// Now keep sleeping until that time has come
			gettimeofday(&now, (struct timezone *) 0);
			timediff(&timeout, &last_time, &now);

			// Go to sleep
			{
				do {
					circle_sleep(&timeout);
					gettimeofday(&now, (struct timezone *) 0);
					timediff(&timeout, &last_time, &now);
				} while (timeout.tv_usec || timeout.tv_sec);
			}

			/*
			 * Now, we execute as many pulses as necessary--just one if we haven't
			 * missed any pulses, or make up for lost time if we missed a few
			 * pulses by sleeping for too long.
			 */
			missed_pulses++;
		}

		if (missed_pulses <= 0) {
			log("SYSERR: **BAD** MISSED_PULSES NONPOSITIVE (%d), TIME GOING BACKWARDS!!", missed_pulses);
			missed_pulses = 1;
		}

		// If we missed more than 30 seconds worth of pulses, just do 30 secs
		// п╦п╥п╪п╣п╫п╦п╩п╦ п╫п╟ 4 я│п╣п╨
		// п╦п╥п╪п╣п╫п╦п╩п╦ п╫п╟ 1 я│п╣п╨ -- я│п╩п╦я┬п╨п╬п╪ я┐п╤ п╬п©п╟я│п╫п╬ п╩п╟пЁп╟п╣я┌ :)
		if (missed_pulses > (1 * kPassesPerSec)) {
			const auto missed_seconds = missed_pulses / kPassesPerSec;
			const auto current_pulse = GlobalObjects::heartbeat().pulse_number();
			char tmpbuf[256];
			sprintf(tmpbuf,"WARNING: Missed %d seconds worth of pulses (%d) on the pulse %d.",
				static_cast<int>(missed_seconds), missed_pulses, current_pulse);
			mudlog(tmpbuf, LGH, kLvlImmortal, SYSLOG, true);
			missed_pulses = 1 * kPassesPerSec;
		}

		// Now execute the heartbeat functions
		while (missed_pulses--) {
#ifdef HAS_EPOLL
			process_io(epoll, mother_desc, events);
#else
			process_io(input_set, output_set, exc_set, null_set, mother_desc, maxdesc);
#endif
			GlobalObjects::heartbeat()(missed_pulses);
		}

#ifdef CIRCLE_UNIX
		// Update tics for deadlock protection (UNIX only)
		tics++;
#endif
	}
#ifdef HAS_EPOLL
	free(events);
#endif
}

/* ******************************************************************
*  general utility stuff (for local use)                            *
****************************************************************** */

/*
 *  new code to calculate time differences, which works on systems
 *  for which tv_usec is unsigned (and thus comparisons for something
 *  being < 0 fail).  Based on code submitted by ss@sirocco.cup.hp.com.
 */

/*
 * code to return the time difference between a and b (a-b).
 * always returns a nonnegative value (floors at 0).
 */
void timediff(struct timeval *rslt, struct timeval *a, struct timeval *b) {
	if (a->tv_sec < b->tv_sec)
		*rslt = null_time;
	else if (a->tv_sec == b->tv_sec) {
		if (a->tv_usec < b->tv_usec)
			*rslt = null_time;
		else {
			rslt->tv_sec = 0;
			rslt->tv_usec = a->tv_usec - b->tv_usec;
		}
	} else        // a->tv_sec > b->tv_sec
	{
		rslt->tv_sec = a->tv_sec - b->tv_sec;
		if (a->tv_usec < b->tv_usec) {
			rslt->tv_usec = a->tv_usec + 1000000 - b->tv_usec;
			rslt->tv_sec--;
		} else
			rslt->tv_usec = a->tv_usec - b->tv_usec;
	}
}

/*
 * Add 2 time values.
 *
 * Patch sent by "d. hall" <dhall@OOI.NET> to fix 'static' usage.
 */
void timeadd(struct timeval *rslt, struct timeval *a, struct timeval *b) {
	rslt->tv_sec = a->tv_sec + b->tv_sec;
	rslt->tv_usec = a->tv_usec + b->tv_usec;

	while (rslt->tv_usec >= 1000000) {
		rslt->tv_usec -= 1000000;
		rslt->tv_sec++;
	}
}

/* ******************************************************************
*  socket handling                                                  *
****************************************************************** */


/*
 * get_bind_addr: Return a struct in_addr that should be used in our
 * call to bind().  If the user has specified a desired binding
 * address, we try to bind to it; otherwise, we bind to INADDR_ANY.
 * Note that inet_aton() is preferred over inet_addr() so we use it if
 * we can.  If neither is available, we always bind to INADDR_ANY.
 */

struct in_addr *get_bind_addr() {
	static struct in_addr bind_addr;

	// Clear the structure
	memset((char *) &bind_addr, 0, sizeof(bind_addr));

	// If DLFT_IP is unspecified, use INADDR_ANY
	if (DFLT_IP == nullptr) {
		bind_addr.s_addr = htonl(INADDR_ANY);
	} else {
		// If the parsing fails, use INADDR_ANY
		if (!parse_ip(DFLT_IP, &bind_addr)) {
			log("SYSERR: DFLT_IP of %s appears to be an invalid IP address", DFLT_IP);
			bind_addr.s_addr = htonl(INADDR_ANY);
		}
	}

	// Put the address that we've finally decided on into the logs
	if (bind_addr.s_addr == htonl(INADDR_ANY))
		log("Binding to all IP interfaces on this host.");
	else
		log("Binding only to IP address %s", inet_ntoa(bind_addr));

	return (&bind_addr);
}

#ifdef HAVE_INET_ATON

// * inet_aton's interface is the same as parse_ip's: 0 on failure, non-0 if successful
int parse_ip(const char *addr, struct in_addr *inaddr)
{
	return (inet_aton(addr, inaddr));
}

#elif HAVE_INET_ADDR

// inet_addr has a different interface, so we emulate inet_aton's
int parse_ip(const char *addr, struct in_addr *inaddr) {
	long ip;

	if ((ip = inet_addr(addr)) == -1) {
		return (0);
	} else {
		inaddr->s_addr = (unsigned long) ip;
		return (1);
	}
}

#else

// If you have neither function - sorry, you can't do specific binding.
int parse_ip(const char *addr, struct in_addr *inaddr)
{
	log("SYSERR: warning: you're trying to set DFLT_IP but your system has no\n"
		"functions to parse IP addresses (how bizarre!)");
	return (0);
}

#endif                // INET_ATON and INET_ADDR

unsigned long get_ip(const char *addr) {
	static struct in_addr ip;
	parse_ip(addr, &ip);
	return (ip.s_addr);
}

// Sets the kernel's send buffer size for the descriptor
int set_sendbuf(socket_t s) {
#if defined(SO_SNDBUF) && !defined(CIRCLE_MACINTOSH)
	int opt = kMaxSockBuf;

	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *) &opt, sizeof(opt)) < 0) {
		perror("SYSERR: setsockopt SNDBUF");
		return (-1);
	}
#if 0
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *) &opt, sizeof(opt)) < 0)
	{
		perror("SYSERR: setsockopt RCVBUF");
		return (-1);
	}
#endif

#endif

	return (0);
}

// п╡п╬п╥п╡я─п╟я┴п╟п╣я┌ п╫п╣п╬я┌я─п╦я├п╟я┌п╣п╩я▄п╫п╬п╣ я├п╣п╩п╬п╣, п╣я│п╩п╦ я┐п╢п╟п╩п╬я│я▄ я│п╬п╥п╢п╟я┌я▄ я│п╬п╨п╣я┌
// п╡п╬п╥п╡я─п╟я┴п╟п╣я┌ -1, п╣я│п╩п╦ accept() п╡п╣я─п╫я┐п╩ EINTR, EAGAIN п╦п╩п╦ EWOULDBLOCK
// п╡п╬п╥п╡я─п╟я┴п╟п╣я┌ -2 п©я─п╦ п╢я─я┐пЁп╦я┘ п╬я┬п╦п╠п╨п╟я┘ я│п╬п╨п╣я┌п╟
// п╡п╬п╥п╡я─п╟я┴п╟п╣я┌ -3, п╣я│п╩п╦ п╡ я│п╬п╣п╢п╦п╫п╣п╫п╦п╦ п╠я▀п╩п╬ п╬я┌п╨п╟п╥п╟п╫п╬ п╢п╡п╦п╤п╨п╬п╪
#ifdef HAS_EPOLL
int new_descriptor(int epoll, socket_t s)
#else
int new_descriptor(socket_t s)
#endif
{
	socket_t desc;
	int sockets_connected = 0;
	socklen_t i;
	static int last_desc = 0;    // last descriptor number
	DescriptorData *newd;
	struct sockaddr_in peer;
	struct hostent *from;
#ifdef HAS_EPOLL
	struct epoll_event event;
#endif

	// accept the new connection
	i = sizeof(peer);
	if ((desc = accept(s, (struct sockaddr *) &peer, &i)) == SOCKET_ERROR) {
#ifdef EWOULDBLOCK
		if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
#else
			if (errno != EINTR && errno != EAGAIN)
#endif
		{
			perror("SYSERR: accept");
			return -2;
		} else
			return (-1);
	}
	// keep it from blocking
	nonblock(desc);

	// set the send buffer size
	if (set_sendbuf(desc) < 0) {
		CLOSE_SOCKET(desc);
		return (-2);
	}

	// make sure we have room for it
	for (newd = descriptor_list; newd; newd = newd->next)
		sockets_connected++;

	if (sockets_connected >= max_players) {
		std::string msg{"Sorry, RUS MUD is full right now... please try again later!\r\n"};
		iosystem::write_to_descriptor(desc, msg.c_str(), strlen(msg.c_str()));
		CLOSE_SOCKET(desc);
		return (-3);
	}
	// create a new descriptor
	NEWCREATE(newd);

	// find the sitename
	if (nameserver_is_slow
		|| !(from = gethostbyaddr((char *) &peer.sin_addr, sizeof(peer.sin_addr), AF_INET)))    // resolution failed
	{
		if (!nameserver_is_slow)
			perror("SYSERR: gethostbyaddr");

		// find the numeric site address
		strncpy(newd->host, (char *) inet_ntoa(peer.sin_addr), kHostLength);
		*(newd->host + kHostLength) = '\0';
	} else {
		strncpy(newd->host, from->h_name, kHostLength);
		*(newd->host + kHostLength) = '\0';
	}

	// п╦п© п╡ п╡п╦п╢п╣ я┤п╦я│п╩п╟
	newd->ip = TxtToIp(newd->host);

	// determine if the site is banned
#if 0
	/*
	 * Log new connections - probably unnecessary, but you may want it.
	 * Note that your immortals may wonder if they see a connection from
	 * your site, but you are wizinvis upon login.
	 */
	sprintf(buf2, "New connection from [%s]", newd->host);
	mudlog(buf2, CMP, kLevelGod, SYSLOG, false);
#endif
	if (ban->IsBanned(newd->host) == BanList::BAN_ALL) {
		time_t bantime = ban->GetBanDate(newd->host);
		sprintf(buf, "Sorry, your IP is banned till %s",
				bantime == -1 ? "Infinite duration\r\n" : asctime(localtime(&bantime)));
		iosystem::write_to_descriptor(desc, buf, strlen(buf));
		CLOSE_SOCKET(desc);
		// sprintf(buf2, "Connection attempt denied from [%s]", newd->host);
		// mudlog(buf2, CMP, kLevelGod, SYSLOG, true);
		delete newd;
		return (-3);
	}

#ifdef HAS_EPOLL
	//
	// п║п╬ я│п╩п╣п╢я┐я▌я┴п╣п╧ я│я┌я─п╬п╨п╬п╧ я│п╡я▐п╥п╟п╫я▀ п╬п©я─п╣п╢п╣п╩п╣п╫п╫я▀п╣ п©я─п╬п╠п╩п╣п╪я▀.
	//
	// п п╬пЁп╢п╟ я│п╩я┐я┤п╟п╣я┌я│я▐ п╬я┤п╣я─п╣п╢п╫п╬п╣ я│п╬п╠я▀я┌п╦п╣, я┌п╬ п╣п╪я┐ п╡ п©п╬п╩п╣ data.ptr п╥п╟п©п╦я│я▀п╡п╟п╣я┌я│я▐
	// я┌п╬ п╥п╫п╟я┤п╣п╫п╦п╣, п╨п╬я┌п╬я─п╬п╣ п╪я▀ п╣п╪я┐ п╥п╢п╣я│я▄ п©я─п╦я│п╡п╟п╦п╡п╟п╣п╪. п▓ п╢п╟п╫п╫п╬п╪ я│п╩я┐я┤п╟п╣ я█я┌п╬ я│я│я▀п╩п╨п╟
	// п╫п╟ п╬п╠п╩п╟я│я┌я▄ п©п╟п╪я▐я┌п╦, п╡я▀п╢п╣п╩п╣п╫п╫я┐я▌ п©п╬п╢ я│я┌я─я┐п╨я┌я┐я─я┐ п╢п╟п╫п╫п╬пЁп╬ п╢п╣я│п╨я─п╦п©я┌п╬я─п╟.
	//
	// п÷я─п╬п╠п╩п╣п╪п╟ п╥п╢п╣я│я▄ п╥п╟п╨п╩я▌я┤п╟п╣я┌я│я▐ п╡ я┌п╬п╪, я┤я┌п╬ п╡ п©я─п╬я├п╣я│я│п╣ п╡я▀п©п╬п╩п╫п╣п╫п╦я▐ я├п╦п╨п╩п╟,
	// п╬п╠я─п╟п╠п╟я┌я▀п╡п╟я▌я┴п╣пЁп╬ п©п╬п╩я┐я┤п╣п╫п╫я▀п╣ п╡ я─п╣п╥я┐п╩я▄я┌п╟я┌п╣ epoll_wait() я│п╬п╠я▀я┌п╦я▐, п╪я▀
	// п©п╬я┌п╣п╫я├п╦п╟п╩я▄п╫п╬ п╪п╬п╤п╣п╪ п╬п╨п╟п╥п╟я┌я▄я│я▐ п╡ я│п╦я┌я┐п╟я├п╦п╦, п╨п╬пЁп╢п╟ п╡ я─п╣п╥я┐п╩я▄я┌п╟я┌п╣ п╬п╠я─п╟п╠п╬я┌п╨п╦
	// п©п╣я─п╡п╬пЁп╬ я│п╬п╠я▀я┌п╦я▐ я│п╬п╨п╣я┌ п╠я▀п╩ п╥п╟п╨я─я▀я┌ п╦ п©п╟п╪я▐я┌я▄ п©п╬п╢ я│я┌я─я┐п╨я┌я┐я─я┐ п╢п╣я│п╨я─п╦п©я┌п╬я─п╟
	// п╬я│п╡п╬п╠п╬п╤п╢п╣п╫п╟. п▓ я█я┌п╬п╪ я│п╩я┐я┤п╟п╣ п╥п╫п╟я┤п╣п╫п╦п╣ data.ptr п╡п╬ п╡я│п╣я┘ п©п╬я│п╩п╣п╢я┐я▌я┴п╦я┘
	// я│п╬п╠я▀я┌п╦я▐я┘ п╢п╩я▐ п╢п╟п╫п╫п╬пЁп╬ я│п╬п╨п╣я┌п╟ я│я┌п╟п╫п╬п╡п╦я┌я│я▐ я┐п╤п╣ п╫п╣п╡п╟п╩п╦п╢п╫я▀п╪, п╦ п©я─п╦ п©п╬п©я▀я┌п╨п╣
	// п╬п╠я─п╟п╠п╬я┌п╨п╦ я█я┌п╦я┘ я│п╬п╠я▀я┌п╦п╧ п©я─п╬п╦п╥п╬п╧п╢п╣я┌ я┤я┐п╢п╣я│п╫я▀п╧ п╨я─п╣я┬.
	//
	// п÷я─п╣п╢п╬я┌п╡я─п╟я┴п╟п╣я┌я│я▐ я█я┌п╬я┌ п╡п╬п╥п╪п╬п╤п╫я▀п╧ п╨я─п╣я┬ п©я─п╦п╫я┐п╢п╦я┌п╣п╩я▄п╫п╬п╧ я┐я│я┌п╟п╫п╬п╡п╨п╬п╧ data.ptr п╡ nullptr
	// п╢п╩я▐ п╡я│п╣я┘ я│п╬п╠я▀я┌п╦п╧, п©я─п╦я┬п╣п╢я┬п╦я┘ п╬я┌ п╢п╟п╫п╫п╬пЁп╬ я│п╬п╨п╣я┌п╟. п╜я┌п╬ п╢п╣п╩п╟п╣я┌я│я▐ п╡ close_socket(),
	// п╨п╬я┌п╬я─п╬п╪я┐ п╢п╩я▐ я█я┌п╬п╧ я├п╣п╩п╦ я┌п╣п©п╣я─я▄ п©п╣я─п╣п╢п╟п╣я┌я│я▐ я│я│я▀п╩п╨п╟ п╫п╟ п╪п╟я│я│п╦п╡ я│п╬п╠я▀я┌п╦п╧.
	// п╒п╟п╨п╤п╣ п╢п╬п╠п╟п╡п╩п╣п╫п╟ п©я─п╬п╡п╣я─п╨п╟ п╟я─пЁя┐п╪п╣п╫я┌п╟ п╫п╟ nullptr п╡ close_socket(), process_input()
	// п╦ process_output().
	//
	// п■п╩я▐ п╟п╩пЁп╬я─п╦я┌п╪п╟ я│ п╦я│п©п╬п╩я▄п╥п╬п╡п╟п╫п╦п╣п╪ select() я█я┌п╬ п╠я▀п╩п╬ п╫п╣п╟п╨я┌я┐п╟п╩я▄п╫п╬, п©п╬я│п╨п╬п╩я▄п╨я┐
	// п©п╬я│п╩п╣ п╡я▀п╥п╬п╡п╟ select() я├п╦п╨п╩ п©я─п╬я┘п╬п╢п╦п╩ п©п╬ я│п©п╦я│п╨я┐ п╢п╣я│п╨я─п╦п©я┌п╬я─п╬п╡, пЁп╢п╣ п╬п╫п╦ п╡я│п╣ п╥п╟п╡п╣п╢п╬п╪п╬
	// п╡п╟п╩п╦п╢п╫я▀, п╟ я│ epoll п╪я▀ п©я─п╬я┘п╬п╢п╦п╪ п©п╬ я│п©п╦я│п╨я┐ я│п╬п╠я▀я┌п╦п╧, п╡п╟п╩п╦п╢п╫п╬я│я┌я▄ я│п╬я┘я─п╟п╫п╣п╫п╫п╬пЁп╬ п╡
	// п╨п╬я┌п╬я─я▀я┘ п╢п╣я│п╨я─п╦п©я┌п╬я─п╟ п╫п╟п╢п╬ п╨п╬п╫я┌я─п╬п╩п╦я─п╬п╡п╟я┌я▄ п╢п╬п©п╬п╩п╫п╦я┌п╣п╩я▄п╫п╬.
	//
	event.data.ptr = newd;
	//
	//
	event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
	if (epoll_ctl(epoll, EPOLL_CTL_ADD, desc, &event) == -1) {
		log("%s", fmt::format("EPOLL: epoll_ctl() failed on EPOLL_CTL_ADD in {}() at {}:{}",
								 __func__, __FILE__, __LINE__).c_str());
		CLOSE_SOCKET(desc);
		delete newd;
		return -2;
	}
#endif

	// initialize descriptor data
	newd->descriptor = desc;
	newd->idle_tics = 0;
	newd->output = newd->small_outbuf;
	newd->bufspace = kSmallBufsize - 1;
	newd->login_time = newd->input_time = time(0);
	*newd->output = '\0';
	newd->bufptr = 0;
	newd->mxp = false;
	newd->has_prompt = 1;    // prompt is part of greetings
	newd->keytable = kKtSelectmenu;
	newd->state = EConState::kInit;
	/*
	 * This isn't exactly optimal but allows us to make a design choice.
	 * Do we embed the history in descriptor_data or keep it dynamically
	 * allocated and allow a user defined history size?
	 */
	CREATE(newd->history, kHistorySize);

	if (++last_desc == 1000)
		last_desc = 1;
	newd->desc_num = last_desc;

	// prepend to list
	newd->next = descriptor_list;
	descriptor_list = newd;

	// let interpreter to set the ball rolling
	char dummyArg[] = {""};
	nanny(newd, dummyArg);

	// trying to turn on MSDP
	iosystem::write_to_descriptor(newd->descriptor, will_msdp, sizeof(will_msdp));

#if defined(HAVE_ZLIB)
	iosystem::write_to_descriptor(newd->descriptor, iosystem::compress_will, sizeof(iosystem::compress_will));
#endif

	return newd->descriptor;
}

/**
* п≤я┴п╣п╪ п╨п╬п©п╦п╦ я┤п╟я─п╟ п╡ пЁп╩п╬п╠п╟п╩я▄п╫п╬п╪ я┤п╟я─п╟п╨я┌п╣я─-п╩п╦я│я┌п╣, п╬п╫п╦ п╪п╬пЁя┐я┌ я┌п╟п╪ п©п╬я▐п╡п╦я┌я▄я│я▐ п╫п╟п©я─п╦п╪п╣я─
* п©я─п╦ п╡п╡п╬п╢п╣ п©п╟я─п╬п╩я▐ (я─п╣п╩п╬пЁп╦п╫п╣). п▓ п╢п╟п╫п╫п╬п╪ я│п╩я┐я┤п╟п╣ я█я┌п╬ п╫п╟п╢п╬ п╢п╩я▐ п╬п©я─п╣п╢п╣п╩п╣п╫п╦я▐, я┐п╡п╬п╢п╦я┌я▄
* п╡ п╬я└я└п╩п╟п╧п╫ я┘я─п╟п╫п╦п╩п╦я┴п╣ я┤п╟я─п╟ п╦п╩п╦ п╫п╣я┌, п©п╬я┌п╬п╪я┐ я┤я┌п╬ п╡я┌я▀п╨п╟я┌я▄ я█я┌п╬ п╡п╬ п╡я│п╣я┘ я│п╩я┐я┤п╟я▐я┘ я┌я┐п©п╬,
* п╟ п╪п╣п╫я▐я┌я▄ я│п╦я│я┌п╣п╪я┐ я│ п©п╟я│п╟п╪п╦/п╢я┐п╠п╩п╦п╨п╟я┌п╟п╪п╦ п╬п╠п╩п╟п╪я▀п╡п╟п╣я┌.
*/
bool any_other_ch(CharData *ch) {
	for (const auto &vict : character_list) {
		if (!vict->IsNpc()
			&& vict.get() != ch
			&& vict->get_uid() == ch->get_uid()) {
			return true;
		}
	}

	return false;
}

#ifdef HAS_EPOLL
void close_socket(DescriptorData *d, int direct, int epoll, struct epoll_event *events, int n_ev)
#else
void close_socket(DescriptorData * d, int direct)
#endif
{
	if (d == nullptr) {
		log("%s", fmt::format("SYSERR: NULL descriptor in {}() at {}:{}",
								 __func__, __FILE__, __LINE__).c_str());
		return;
	}

	//if (!direct && d->character && NORENTABLE(d->character))
	//	return;
	// п²п╣п╩я▄п╥я▐ п╢п╣п╩п╟я┌я▄ п╩п╢ п©я─п╦ wait_state
	if (d->character && !direct) {
		if (d->character->get_wait() > 0)
			return;
	}

	REMOVE_FROM_LIST(d, descriptor_list);
#ifdef HAS_EPOLL
	if (epoll_ctl(epoll, EPOLL_CTL_DEL, d->descriptor, nullptr) == -1)
		log("SYSERR: EPOLL_CTL_DEL failed in close_socket()");
	// я│п╪. п╨п╬п╪п╪п╣п╫я┌п╟я─п╦п╦ п╡ new_descriptor()
	int i;
	if (events != nullptr)
		for (i = 0; i < n_ev; i++)
			if (events[i].data.ptr == d)
				events[i].data.ptr = nullptr;
#endif
	CLOSE_SOCKET(d->descriptor);
	iosystem::flush_queues(d);

	// Forget snooping
	if (d->snooping)
		d->snooping->snoop_by = nullptr;

	if (d->snoop_by) {
		iosystem::write_to_output("п▓п╟я┬ п©п╬п╢п╬п©п╣я┤п╫я▀п╧ п╡я▀п╨п╩я▌я┤п╦п╩ п╨п╬п╪п©я▄я▌я┌п╣я─.\r\n", d->snoop_by);
		d->snoop_by->snooping = nullptr;
	}
	//. Kill any OLC stuff .
	switch (d->state) {
		case EConState::kOedit:
		case EConState::kRedit:
		case EConState::kZedit:
		case EConState::kMedit:
		case EConState::kMredit:
		case EConState::kTrigedit: cleanup_olc(d, CLEANUP_ALL);
			break;
			/*case CON_CONSOLE:
				d->console.reset();
				break;*/
		default: break;
	}

	if (d->character) {
		// Plug memory leak, from Eric Green.
		if (!d->character->IsNpc()
			&& (d->character->IsFlagged(EPlrFlag::kMailing)
				|| d->state == EConState::kWriteboard
				|| d->state == EConState::kWriteMod
				|| d->state == EConState::kWriteNote)
			&& d->writer) {
			d->writer->clear();
			d->writer.reset();
		}

		if (d->state == EConState::kWriteboard
			|| d->state == EConState::kClanedit
			|| d->state == EConState::kSpendGlory
			|| d->state == EConState::kWriteMod
			|| d->state == EConState::kWriteNote
			|| d->state == EConState::kGloryConst
			|| d->state == EConState::kNamedStuff
			|| d->state == EConState::kMapMenu
			|| d->state == EConState::kTorcExch
			|| d->state == EConState::kSedit || d->state == EConState::kConsole) {
			d->state = EConState::kPlaying;
		}

		if (d->state == EConState::kPlaying || d->state == EConState::kDisconnect) {
			act("$n п©п╬я┌п╣я─я▐п╩$g я│п╡я▐п╥я▄.", true, d->character.get(), 0, 0, kToRoom | kToArenaListen);
			if (d->character->GetEnemy() && d->character->IsFlagged(EPrf::kAntiDcMode)) {
				snprintf(buf2, sizeof(buf2), "п╥п╟я┤п╦я┌п╟я┌я▄ я│п╡п╦я┌п╬п╨.п╡п╬п╥п╡я─п╟я┌п╟");
				command_interpreter(d->character.get(), buf2);
			}
			if (!d->character->IsNpc()) {
				d->character->save_char();
				CheckLight(d->character.get(), kLightNo, kLightNo, kLightNo, kLightNo, -1);
				Crash_ldsave(d->character.get());

				sprintf(buf, "Closing link to: %s.", GET_NAME(d->character));
				mudlog(buf, NRM, std::max(kLvlGod, GET_INVIS_LEV(d->character)), SYSLOG, true);
			}
			d->character->desc = nullptr;
		} else {
			if (!any_other_ch(d->character.get())) {
				Depot::exit_char(d->character.get());
			}
			if (character_list.get_character_by_address(d->character.get())) {
				sprintf(buf, "Remove from character list to: %s.", GET_NAME(d->character));
				log("%s", buf);
				character_list.remove(d->character);
			}
		}
	}

	// JE 2/22/95 -- part of my unending quest to make switch stable
	if (d->original && d->original->desc)
		d->original->desc = nullptr;

	// Clear the command history.
	if (d->history) {
		int cnt;
		for (cnt = 0; cnt < kHistorySize; cnt++)
			if (d->history[cnt])
				free(d->history[cnt]);
		free(d->history);
	}

	if (d->showstr_head)
		free(d->showstr_head);
	if (d->showstr_count)
		free(d->showstr_vector);
#if defined(HAVE_ZLIB)
	if (d->deflate) {
		deflateEnd(d->deflate);
		free(d->deflate);
	}
#endif

	// TODO: п╢п╣я│я┌я─я┐п╨я┌я┐я─ п╫п╣ п╡я▀п╥я▀п╡п╟п╣я┌я│я▐, п©п╬п╨п╟ я┐ п╫п╟я│ п╢п╣я│п╨я─п╦п©я┌п╬я─ п╫п╣ я│я┌п╟п╩ п╨п╩п╟я│я│п╬п╪
	d->board.reset();
	d->message.reset();
	d->clan_olc.reset();
	d->clan_invite.reset();
	d->glory.reset();
	d->map_options.reset();
	d->sedit.reset();

	if (d->pers_log) {
		opened_files.remove(d->pers_log);
		fclose(d->pers_log); // п╫п╣ п╥п╟п╠я▀п╡п╟п╣п╪ п╥п╟п╨я─я▀я┌я▄ п©п╣я─я│п╬п╫п╟п╩я▄п╫я▀п╧ п╩п╬пЁ
	}

	delete d;
}

/*
 * I tried to universally convert Circle over to POSIX compliance, but
 * alas, some systems are still straggling behind and don't have all the
 * appropriate defines.  In particular, NeXT 2.x defines O_NDELAY but not
 * O_NONBLOCK.  Krusty old NeXT machines!  (Thanks to Michael Jones for
 * this and various other NeXT fixes.)
 */

#if defined(CIRCLE_WINDOWS)

void nonblock(socket_t s)
{
	unsigned long val = 1;
	ioctlsocket(s, FIONBIO, &val);
}

#elif defined(CIRCLE_AMIGA)

void nonblock(socket_t s)
{
	long val = 1;
	IoctlSocket(s, FIONBIO, &val);
}

#elif defined(CIRCLE_ACORN)

void nonblock(socket_t s)
{
	int val = 1;
	socket_ioctl(s, FIONBIO, &val);
}

#elif defined(CIRCLE_VMS)

void nonblock(socket_t s)
{
	int val = 1;

	if (ioctl(s, FIONBIO, &val) < 0)
	{
		perror("SYSERR: Fatal error executing nonblock (comm.c)");
		exit(1);
	}
}

#elif defined(CIRCLE_UNIX) || defined(CIRCLE_OS2) || defined(CIRCLE_MACINTOSH)

#ifndef O_NONBLOCK
#define O_NONBLOCK O_NDELAY
#endif

void nonblock(socket_t s) {
	int flags;

	flags = fcntl(s, F_GETFL, 0);
	flags |= O_NONBLOCK;
	if (fcntl(s, F_SETFL, flags) < 0) {
		perror("SYSERR: Fatal error executing nonblock (comm.c)");
		exit(1);
	}
}

#endif                // CIRCLE_UNIX || CIRCLE_OS2 || CIRCLE_MACINTOSH


/* ******************************************************************
*  signal-handling functions (formerly signals.c).  UNIX only.      *
****************************************************************** */
#ifndef POSIX
#define my_signal(signo, func) signal(signo, func)
#else
sigfunc *my_signal(int signo, sigfunc *func) {
	struct sigaction act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;    // SunOS
#endif

	if (sigaction(signo, &act, &oact) < 0)
		return (SIG_ERR);

	return (oact.sa_handler);
}
#endif                // POSIX

#if defined(CIRCLE_UNIX) || defined(CIRCLE_MACINTOSH)
#ifdef CIRCLE_UNIX

// clean up our zombie kids to avoid defunct processes
RETSIGTYPE reap(int/* sig*/) {
	while (waitpid(-1, (int *) nullptr, WNOHANG) > 0);

	my_signal(SIGCHLD, reap);
}

RETSIGTYPE crash_handle(int/* sig*/) {
	log("Crash detected !");
	// п║п╩п╦п╡п╟п╣п╪ я└п╟п╧п╩п╬п╡я▀п╣ п╠я┐я└п╣я─я▀.
	fflush(stdout);
	fflush(stderr);

	for (int i = 0; i < 1 + LAST_LOG; ++i) {
		fflush(runtime_config.logs(static_cast<EOutputStream>(i)).handle());
	}
	for (std::list<FILE *>::const_iterator it = opened_files.begin(); it != opened_files.end(); ++it)
		fflush(*it);

	my_signal(SIGSEGV, SIG_DFL);
	my_signal(SIGBUS, SIG_DFL);
}

RETSIGTYPE checkpointing(int/* sig*/) {
	if (!tics) {
		log("SYSERR: CHECKPOINT shutdown: tics not updated. (Infinite loop suspected)");
		abort();
	} else
		tics = 0;
}

RETSIGTYPE hupsig(int/* sig*/) {
	log("SYSERR: Received SIGHUP, SIGINT, or SIGTERM.  Shutting down...");
	exit(1);        // perhaps something more elegant should substituted
}

#endif                // CIRCLE_UNIX

/*
 * This is an implementation of signal() using sigaction() for portability.
 * (sigaction() is POSIX; signal() is not.)  Taken from Stevens' _Advanced
 * Programming in the UNIX Environment_.  We are specifying that all system
 * calls _not_ be automatically restarted for uniformity, because BSD systems
 * do not restart select(), even if SA_RESTART is used.
 *
 * Note that NeXT 2.x is not POSIX and does not have sigaction; therefore,
 * I just define it to be the old signal.  If your system doesn't have
 * sigaction either, you can use the same fix.
 *
 * SunOS Release 4.0.2 (sun386) needs this too, according to Tim Aldric.
 */

void signal_setup(void) {
	my_signal(SIGINT, hupsig);
	my_signal(SIGTERM, hupsig);
	my_signal(SIGPIPE, SIG_IGN);
	my_signal(SIGALRM, SIG_IGN);
	my_signal(SIGSEGV, crash_handle);
	my_signal(SIGBUS, crash_handle);

}

#endif                // CIRCLE_UNIX || CIRCLE_MACINTOSH

/* ****************************************************************
*       Public routines for system-to-player-communication        *
**************************************************************** */
void send_stat_char(const CharData *ch) {
	char fline[256];
	sprintf(fline, "%d[%d]HP %d[%d]Mv %ldG %dL ",
			ch->get_hit(), ch->get_real_max_hit(), ch->get_move(), ch->get_real_max_move(), ch->get_gold(), GetRealLevel(ch));
	iosystem::write_to_output(fline, ch->desc);
}

void SendMsgToChar(const char *msg, const CharData *ch) {
	if (ch->desc && msg)
		iosystem::write_to_output(msg, ch->desc);
}

void SendMsgToChar(const CharData *ch, const char *msg, ...) {
	va_list args;
	char tmpbuf[kMaxStringLength];

	va_start(args, msg);
	vsnprintf(tmpbuf, sizeof(tmpbuf), msg, args);
	va_end(args);
	SendMsgToChar(tmpbuf, ch);
}

void SendMsgToChar(const std::string &msg, const CharData *ch) {
	if (ch->desc && !msg.empty())
		SendMsgToChar(msg.c_str(), ch);
}

void SendMsgToAll(const char *msg) {
	if (msg == nullptr) {
		return;
	}
	for (auto i = descriptor_list; i; i = i->next) {
		if  (i->state == EConState::kPlaying) {
			iosystem::write_to_output(msg, i);
		}
	}
}

void SendMsgToOutdoor(const char *msg, int control) {
	int room;
	DescriptorData *i;

	if (!msg || !*msg)
		return;

	for (i = descriptor_list; i; i = i->next) {
		if  (i->state != EConState::kPlaying || i->character == nullptr)
			continue;
		if (!AWAKE(i->character) || !OUTSIDE(i->character))
			continue;
		room = i->character->in_room;
		if (!control
			|| (IS_SET(control, SUN_CONTROL)
				&& room != kNowhere
				&& SECT(room) != ESector::kUnderwater
				&& !AFF_FLAGGED(i->character, EAffect::kBlind))
			|| (IS_SET(control, WEATHER_CONTROL)
				&& room != kNowhere
				&& SECT(room) != ESector::kUnderwater
				&& !ROOM_FLAGGED(room, ERoomFlag::kNoWeather)
				&& world[i->character->in_room]->weather.duration <= 0)) {
			iosystem::write_to_output(msg, i);
		}
	}
}

void SendMsgToGods(char *text, bool include_demigod) {
	DescriptorData *d;
	for (d = descriptor_list; d; d = d->next) {
		if (d->state == EConState::kPlaying) {
			if ((GetRealLevel(d->character) >= kLvlGod) ||
				(GET_GOD_FLAG(d->character, EGf::kDemigod) && include_demigod)) {
				SendMsgToChar(text, d->character.get());
			}
		}
	}
}

void SendMsgToGods(const char *msg) {
	DescriptorData *i;

	if (!msg || !*msg) {
		return;
	}

	for (i = descriptor_list; i; i = i->next) {
		if  (i->state != EConState::kPlaying || i->character == nullptr || !IS_GOD(i->character)) {
			continue;
		}
		iosystem::write_to_output(msg, i);
	}
}

void SendMsgToRoom(const char *msg, RoomRnum room, int to_awake) {
	if (msg == nullptr) {
		return;
	}

	for (const auto i : world[room]->people) {
		if (i->desc &&
			!i->IsNpc()
			&& (!to_awake
				|| AWAKE(i))) {
			iosystem::write_to_output(msg, i->desc);
			iosystem::write_to_output("\r\n", i->desc);
		}
	}
}

#define CHK_NULL(pointer, expression) \
  ((pointer) == nullptr) ? ACTNULL : (expression)

// higher-level communication: the act() function
void perform_act(const char *orig,
				 CharData *ch,
				 const ObjData *obj,
				 const void *vict_obj,
				 CharData *to,
				 const int arena,
				 const std::string &kick_type) {
	const char *i = nullptr;
	char nbuf[256];
	char lbuf[kMaxStringLength], *buf;
	ubyte padis;
	int stopbyte, cap = 0;
	CharData *dg_victim = nullptr;
	ObjData *dg_target = nullptr;
	char *dg_arg = nullptr;

	buf = lbuf;

	if (orig == nullptr)
		return mudlog("perform_act: NULL *orig string", BRF, -1, ERRLOG, true);
	for (stopbyte = 0; stopbyte < kMaxStringLength; stopbyte++) {
		if (*orig == '$') {
			switch (*(++orig)) {
				case 'n':
					if (*(orig + 1) < '0' || *(orig + 1) > '5') {
						snprintf(nbuf,
								 sizeof(nbuf),
								 "&q%s&Q",
								 (!ch->IsNpc() && (IS_IMMORTAL(ch) || GET_INVIS_LEV(ch))) ? GET_NAME(ch) : APERS(ch,
																												to,
																												0,
																												arena));
						i = nbuf;
					} else {
						padis = *(++orig) - '0';
						snprintf(nbuf,
								 sizeof(nbuf),
								 "&q%s&Q",
								 (!ch->IsNpc() && (IS_IMMORTAL(ch) || GET_INVIS_LEV(ch))) ? GET_PAD(ch, padis)
																						  : APERS(ch, to, padis, arena));
						i = nbuf;
					}
					break;
				case 'N':
					if (*(orig + 1) < '0' || *(orig + 1) > '5') {
						snprintf(nbuf,
								 sizeof(nbuf),
								 "&q%s&Q",
								 CHK_NULL(vict_obj, APERS((const CharData *) vict_obj, to, 0, arena)));
						i = nbuf;
					} else {
						padis = *(++orig) - '0';
						snprintf(nbuf,
								 sizeof(nbuf),
								 "&q%s&Q",
								 CHK_NULL(vict_obj, APERS((const CharData *) vict_obj, to, padis, arena)));
						i = nbuf;
					}
					dg_victim = (CharData *) vict_obj;
					break;

				case 'm': i = HMHR(ch);
					break;
				case 'M':
					if (vict_obj)
						i = HMHR((const CharData *) vict_obj);
					else CHECK_NULL(obj, OMHR(obj));
					dg_victim = (CharData *) vict_obj;
					break;

				case 's': i = HSHR(ch);
					break;
				case 'S':
					if (vict_obj)
						i = CHK_NULL(vict_obj, HSHR((const CharData *) vict_obj));
					else CHECK_NULL(obj, OSHR(obj));
					dg_victim = (CharData *) vict_obj;
					break;

				case 'e': i = HSSH(ch);
					break;
				case 'E':
					if (vict_obj)
						i = CHK_NULL(vict_obj, HSSH((const CharData *) vict_obj));
					else CHECK_NULL(obj, OSSH(obj));
					dg_victim = (CharData *) vict_obj;
					break;

				case 'o':
					if (*(orig + 1) < '0' || *(orig + 1) > '5') {
						snprintf(nbuf, sizeof(nbuf), "&q%s&Q", CHK_NULL(obj, AOBJN(obj, to, ECase::kNom, arena)));
						i = nbuf;
					} else {
						padis = *(++orig) - '0';
						snprintf(nbuf,
								 sizeof(nbuf),
								 "&q%s&Q",
								 CHK_NULL(obj, AOBJN(obj, to, padis > ECase::kLastCase ? ECase::kNom : static_cast<ECase>(padis), arena)));
						i = nbuf;
					}
					break;
				case 'O':
					if (*(orig + 1) < '0' || *(orig + 1) > '5') {
						snprintf(nbuf,
								 sizeof(nbuf),
								 "&q%s&Q",
								 CHK_NULL(vict_obj, AOBJN((const ObjData *) vict_obj, to, ECase::kNom, arena)));
						i = nbuf;
					} else {
						padis = *(++orig) - '0';
						snprintf(nbuf, sizeof(nbuf), "&q%s&Q", CHK_NULL(vict_obj,
																		AOBJN((const ObjData *) vict_obj,
																			  to,
																			  padis > ECase::kLastCase ? ECase::kNom : static_cast<ECase>(padis),
																			  arena)));
						i = nbuf;
					}
					dg_victim = (CharData *) vict_obj;
					break;

				case 'p': CHECK_NULL(obj, AOBJS(obj, to, arena));
					break;
				case 'P': CHECK_NULL(vict_obj, AOBJS((const ObjData *) vict_obj, to, arena));
					dg_victim = (CharData *) vict_obj;
					break;

				case 't': i = kick_type.c_str();
					break;

				case 'T': CHECK_NULL(vict_obj, (const char *) vict_obj);
					break;

				case 'F': CHECK_NULL(vict_obj, (const char *) vict_obj);
					break;

				case '$': i = "$";
					break;

				case 'a': i = IS_IMMORTAL(ch) || (arena) ? GET_CH_SUF_6(ch) : GET_CH_VIS_SUF_6(ch, to);
					break;
				case 'A':
					if (vict_obj)
						i = arena ? GET_CH_SUF_6((const CharData *) vict_obj)
								  : GET_CH_VIS_SUF_6((const CharData *) vict_obj, to);
					else CHECK_NULL(obj, arena ? GET_OBJ_SUF_6(obj) : GET_OBJ_VIS_SUF_6(obj, to));
					dg_victim = (CharData *) vict_obj;
					break;

				case 'g': i = IS_IMMORTAL(ch) || (arena) ? GET_CH_SUF_1(ch) : GET_CH_VIS_SUF_1(ch, to);
					break;
				case 'G':
					if (vict_obj)
						i = arena ? GET_CH_SUF_1((const CharData *) vict_obj)
								  : GET_CH_VIS_SUF_1((const CharData *) vict_obj, to);
					else CHECK_NULL(obj, arena ? GET_OBJ_SUF_1(obj) : GET_OBJ_VIS_SUF_1(obj, to));
					dg_victim = (CharData *) vict_obj;
					break;

				case 'y': i = IS_IMMORTAL(ch) || (arena) ? GET_CH_SUF_5(ch) : GET_CH_VIS_SUF_5(ch, to);
					break;
				case 'Y':
					if (vict_obj)
						i = arena ? GET_CH_SUF_5((const CharData *) vict_obj)
								  : GET_CH_VIS_SUF_5((const CharData *) vict_obj, to);
					else CHECK_NULL(obj, arena ? GET_OBJ_SUF_5(obj) : GET_OBJ_VIS_SUF_5(obj, to));
					dg_victim = (CharData *) vict_obj;
					break;

				case 'u': i = IS_IMMORTAL(ch) || (arena) ? GET_CH_SUF_2(ch) : GET_CH_VIS_SUF_2(ch, to);
					break;
				case 'U':
					if (vict_obj)
						i = arena ? GET_CH_SUF_2((const CharData *) vict_obj)
								  : GET_CH_VIS_SUF_2((const CharData *) vict_obj, to);
					else CHECK_NULL(obj, arena ? GET_OBJ_SUF_2(obj) : GET_OBJ_VIS_SUF_2(obj, to));
					dg_victim = (CharData *) vict_obj;
					break;

				case 'w': i = IS_IMMORTAL(ch) || (arena) ? GET_CH_SUF_3(ch) : GET_CH_VIS_SUF_3(ch, to);
					break;
				case 'W':
					if (vict_obj)
						i = arena ? GET_CH_SUF_3((const CharData *) vict_obj)
								  : GET_CH_VIS_SUF_3((const CharData *) vict_obj, to);
					else CHECK_NULL(obj, arena ? GET_OBJ_SUF_3(obj) : GET_OBJ_VIS_SUF_3(obj, to));
					dg_victim = (CharData *) vict_obj;
					break;

				case 'q': i = IS_IMMORTAL(ch) || (arena) ? GET_CH_SUF_4(ch) : GET_CH_VIS_SUF_4(ch, to);
					break;
				case 'Q':
					if (vict_obj)
						i = arena ? GET_CH_SUF_4((const CharData *) vict_obj)
								  : GET_CH_VIS_SUF_4((const CharData *) vict_obj, to);
					else CHECK_NULL(obj, arena ? GET_OBJ_SUF_4(obj) : GET_OBJ_VIS_SUF_4(obj, to));
					dg_victim = (CharData *) vict_obj;
					break;
//я│я┐я└я└п╦п╨я│ пЁп╩я┐п©(я▀п╪,п╬п╧,я▀п╪п╦)
				case 'r': i = IS_IMMORTAL(ch) || (arena) ? GET_CH_SUF_7(ch) : GET_CH_VIS_SUF_7(ch, to);
					break;
				case 'R':
					if (vict_obj)
						i = arena ? GET_CH_SUF_7((const CharData *) vict_obj)
								  : GET_CH_VIS_SUF_7((const CharData *) vict_obj, to);
					else CHECK_NULL(obj, arena ? GET_OBJ_SUF_7(obj) : GET_OBJ_VIS_SUF_7(obj, to));
					dg_victim = (CharData *) vict_obj;
					break;
//я│я┐я└я└п╦п╨я│ п╨п╟п╨(п╬п╣,п╬п╧,п╟я▐,п╦п╣)
				case 'x': i = IS_IMMORTAL(ch) || (arena) ? GET_CH_SUF_8(ch) : GET_CH_VIS_SUF_8(ch, to);
					break;
				case 'X':
					if (vict_obj)
						i = arena ? GET_CH_SUF_8((const CharData *) vict_obj)
								  : GET_CH_VIS_SUF_8((const CharData *) vict_obj, to);
					else CHECK_NULL(obj, arena ? GET_OBJ_SUF_8(obj) : GET_OBJ_VIS_SUF_8(obj, to));
					dg_victim = (CharData *) vict_obj;
					break;
//я│п╨п╩п╬п╫п╣п╫п╦п╣ п╪п╣я│я┌п╬п╦п╪п╣п╫п╦я▐ п▓п╟я┬(п╣,п╟,п╦)
				case 'z':
					if (obj)
						i = OYOU(obj);
					else CHECK_NULL(obj, OYOU(obj));
					break;
				case 'Z':
					if (vict_obj)
						i = HYOU((const CharData *) vict_obj);
					else CHECK_NULL(vict_obj, HYOU((const CharData *) vict_obj))
					break;
				default: log("SYSERR: Illegal $-code to act(): %c", *orig);
					log("SYSERR: %s", orig);
					i = "";
					break;
			}
			if (cap) {
				if (*i == '&') {
					*buf = *(i++);
					buf++;
					*buf = *(i++);
					buf++;
				}
				*buf = a_ucc(*i);
				i++;
				buf++;
				cap = 0;
			}
			while ((*buf = *(i++)))
				buf++;
			orig++;
		} else if (*orig == '\\') {
			if (*(orig + 1) == 'r') {
				*(buf++) = '\r';
				orig += 2;
			} else if (*(orig + 1) == 'n') {
				*(buf++) = '\n';
				orig += 2;
			} else if (*(orig + 1) == 'u')//п║п╩п╣п╢я┐я▌я┴п╟я▐ п©п╬п╢я│я┌п╟п╫п╬п╡п╨п╟ $... п╠я┐п╢п╣я┌ я│ п╠п╬п╩я▄я┬п╬п╧ п╠я┐п╨п╡я▀
			{
				cap = 1;
				orig += 2;
			} else
				*(buf++) = *(orig++);
		} else if (!(*(buf++) = *(orig++)))
			break;
	}

	*(--buf) = '\r';
	*(++buf) = '\n';
	*(++buf) = '\0';

	if (to->desc) {
		// п■п╣п╩п╟п╣п╪ п©п╣я─п╡я▀п╧ я│п╦п╪п╡п╬п╩ п╠п╬п╩я▄я┬п╦п╪, я┐я┤п╦я┌я▀п╡п╟я▐ &X
		// п╡ я│п╡я▐п╥п╦ я│ п╫п╬п╡п╬п╡п╡п╣п╢п╣п╫п╦я▐п╪п╦ я┌п╟п╨п╦я┘ п╨п╩я▌я┤п╣п╧ п╪п╬п╤п╣я┌ п╠я▀я┌я▄ п╫п╣я│п╨п╬п╩я▄п╨п╬ п©я─п╬п©я┐я│я┌п╦п╪ п╦я┘ п╡я│п╣
		if (lbuf[0] == '&') {
			char *tmp;
			tmp = lbuf;
			while ((tmp - lbuf < (int) strlen(lbuf) - 2) && (*tmp == '&'))
				tmp += 2;
			utils::CAP(tmp);
		}
		iosystem::write_to_output(utils::CAP(lbuf), to->desc);
	}

	if ((to->IsNpc() && dg_act_check) && (to != ch))
		act_mtrigger(to, lbuf, ch, dg_victim, obj, dg_target, dg_arg);
}

// moved this to utils.h --- mah
#ifndef SENDOK
#define SENDOK(ch)	((ch)->desc && (to_sleeping || AWAKE(ch)) && \
			(ch->IsNpc() || !EPlrFlag::FLAGGED((ch), EPlrFlag::WRITING)))
#endif

void act(const char *str,
		 int hide_invisible,
		 CharData *ch,
		 const ObjData *obj,
		 const void *vict_obj,
		 int type,
		 const std::string &kick_type) {
	int to_sleeping, check_deaf, check_nodeaf, to_arena = 0, arena_room_rnum;
	int to_brief_shields = 0, to_no_brief_shields = 0;

	if (!str || !*str) {
		return;
	}

	if (!(dg_act_check = !(type & DG_NO_TRIG))) {
		type &= ~DG_NO_TRIG;
	}

	/*
	 * Warning: the following TO_SLEEP code is a hack.
	 *
	 * I wanted to be able to tell act to deliver a message regardless of sleep
	 * without adding an additional argument.  TO_SLEEP is 128 (a single bit
	 * high up).  It's ONLY legal to combine TO_SLEEP with one other TO_x
	 * command.  It's not legal to combine TO_x's with each other otherwise.
	 * TO_SLEEP only works because its value "happens to be" a single bit;
	 * do not change it to something else.  In short, it is a hack.
	 */

	if ((to_no_brief_shields = (type & kToNoBriefShields)))
		type &= ~kToNoBriefShields;
	if ((to_brief_shields = (type & kToBriefShields)))
		type &= ~kToBriefShields;
	if ((to_arena = (type & kToArenaListen)))
		type &= ~kToArenaListen;
	// check if TO_SLEEP is there, and remove it if it is.
	if ((to_sleeping = (type & kToSleep)))
		type &= ~kToSleep;
	if ((check_deaf = (type & kToNotDeaf)))
		type &= ~kToNotDeaf;
	if ((check_nodeaf = (type & kToDeaf)))
		type &= ~kToDeaf;

	if (type == kToChar) {
		if (ch
			&& SENDOK(ch)
			&& ch->in_room != kNowhere
			&& (!check_deaf || !AFF_FLAGGED(ch, EAffect::kDeafness))
			&& (!check_nodeaf || AFF_FLAGGED(ch, EAffect::kDeafness))
			&& (!to_brief_shields || ch->IsFlagged(EPrf::kBriefShields))
			&& (!to_no_brief_shields || !ch->IsFlagged(EPrf::kBriefShields))) {
			perform_act(str, ch, obj, vict_obj, ch, kick_type);
		}
		return;
	}

	if (type == kToVict) {
		CharData *to = (CharData *) vict_obj;
		if (to != nullptr
			&& SENDOK(to)
			&& to->in_room != kNowhere
			&& (!check_deaf || !AFF_FLAGGED(to, EAffect::kDeafness))
			&& (!check_nodeaf || AFF_FLAGGED(to, EAffect::kDeafness))
			&& (!to_brief_shields || to->IsFlagged(EPrf::kBriefShields))
			&& (!to_no_brief_shields || !to->IsFlagged(EPrf::kBriefShields))) {
			perform_act(str, ch, obj, vict_obj, to, kick_type);
		}

		return;
	}
	// ASSUMPTION: at this point we know type must be TO_NOTVICT or TO_ROOM
	// or TO_ROOM_HIDE

	size_t room_number = ~0;
	if (ch && ch->in_room != kNowhere) {
		room_number = ch->in_room;
	} else if (obj && obj->get_in_room() != kNowhere) {
		room_number = obj->get_in_room();
	} else {
		log("No valid target to act('%s')!", str);
		return;
	}

	// п╫я┐п╤п╫п╬ я┤я┌п╬п╠ п╫п╣ п╡я▀п╡п╬п╢п╦п╩п╦я│я▄ я│п╬п╬п╠я┴п╣п╫п╦я▐ я┌п╬п╩я▄п╨п╬ п╢п╩я▐ п╟я─п╣п╫я▀ п╩п╦я┬п╫п╦п╧ я─п╟п╥
	if (type == kToNotVict || type == kToRoom || type == kToRoomSensors) {
		int stop_counter = 0;
		for (const auto to : world[room_number]->people) {
			if (stop_counter >= 1000) {
				break;
			}
			++stop_counter;

			if (!SENDOK(to) || (to == ch)) {
				continue;
			}
			if (hide_invisible && ch && !CAN_SEE(to, ch))
				continue;
			if ((type != kToRoom && type != kToRoomSensors) && to == vict_obj)
				continue;
			if (check_deaf && AFF_FLAGGED(to, EAffect::kDeafness))
				continue;
			if (check_nodeaf && !AFF_FLAGGED(to, EAffect::kDeafness))
				continue;
			if (to_brief_shields && !to->IsFlagged(EPrf::kBriefShields))
				continue;
			if (to_no_brief_shields && to->IsFlagged(EPrf::kBriefShields))
				continue;
			if (type == kToRoomSensors && !AFF_FLAGGED(to, EAffect::kDetectLife)
				&& (to->IsNpc() || !to->IsFlagged(EPrf::kHolylight)))
				continue;
			if (type == kToRoomSensors && to->IsFlagged(EPrf::kHolylight)) {
				std::string buffer = str;
				if (!IS_MALE(ch)) {
					utils::ReplaceFirst(buffer, "я│я▐", GET_CH_SUF_2(ch));
				}
				utils::ReplaceFirst(buffer, "п я┌п╬-я┌п╬", ch->get_name());
				perform_act(buffer.c_str(), ch, obj, vict_obj, to, kick_type);
			} else {
				perform_act(str, ch, obj, vict_obj, to, kick_type);
			}
		}
	}
	//п═п╣п╟п╩п╦п╥п╟я├п╦я▐ я└п╩п╟пЁп╟ я│п╩я▀я┬п╫п╬ п╟я─п╣п╫я┐
	if ((to_arena) && (ch) && !IS_IMMORTAL(ch) && (ch->in_room != kNowhere) && ROOM_FLAGGED(ch->in_room, ERoomFlag::kArena)
		&& ROOM_FLAGGED(ch->in_room, ERoomFlag::kArenaSend) && !ROOM_FLAGGED(ch->in_room, ERoomFlag::kTribune)) {
		arena_room_rnum = ch->in_room;
		// п╫п╟я┘п╬п╢п╦п╪ п©п╣я─п╡я┐я▌ п╨п╩п╣я┌п╨я┐ п╡ п╥п╬п╫п╣
		while ((int) world[arena_room_rnum - 1]->vnum / 100 == (int) world[arena_room_rnum]->vnum / 100)
			arena_room_rnum--;
		//п©я─п╬п╠п╣пЁп╟п╣п╪я│я▐ п©п╬ п╡я│п╣п╪ п╨п╩п╣я┌п╨п╟п╪ п╡ п╥п╬п╫п╣
		while ((int) world[arena_room_rnum + 1]->vnum / 100 == (int) world[arena_room_rnum]->vnum / 100) {
			// п╫п╟я┘п╬п╢п╦п╪ п╨п╩п╣я┌п╨я┐ п╡ п╨п╬я┌п╬я─п╬п╧ я│п╩я▀я┬п╫п╬ п╟я─п╣п╫я┐ п╦ п╡я│п╣п╪ п╦пЁя─п╬п╨п╟п╪ п╡ п╫п╣п╧ п©п╣я─п╣п╢п╟п╣п╪ я│п╬п╬п╠я┴п╣п╫п╦п╣ я│ п╟я─п╣п╫я▀
			if (ch->in_room != arena_room_rnum && ROOM_FLAGGED(arena_room_rnum, ERoomFlag::kTribune)) {
				int stop_count = 0;
				for (const auto to : world[arena_room_rnum]->people) {
					if (stop_count >= 200) {
						break;
					}
					++stop_count;

					if (!to->IsNpc()) {
						perform_act(str, ch, obj, vict_obj, to, to_arena, kick_type);
					}
				}
			}
			arena_room_rnum++;
		}
	}
}

// * This may not be pretty but it keeps game_loop() neater than if it was inline.
#if defined(CIRCLE_WINDOWS)

inline void circle_sleep(struct timeval *timeout)
{
	Sleep(timeout->tv_sec * 1000 + timeout->tv_usec / 1000);
}

#else

inline void circle_sleep(struct timeval *timeout) {
	if (select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, timeout) < 0) {
		if (errno != EINTR) {
			perror("SYSERR: Select sleep");
			exit(1);
		}
	}
}

#endif                // CIRCLE_WINDOWS

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
