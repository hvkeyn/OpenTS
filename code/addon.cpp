/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "addon.h"

#include "_deploymentconfig.h"
#include "gamedirs.h"
#include "ccfile.h"
#include "data.h"
#include "deploymentconfig.h"
#include "init.h"
#include "language/language.h"
#include "ownrdraw.h"

#include <string>

/*
 * The mutexes the startup code creates to keep a second copy of the game from running.
 */
extern HANDLE AppMutex;
extern HANDLE AutoPlayMutex;

INT_PTR CALLBACK Select_Game_Type_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

int AvailableAddOns = 1 << ADDON_BASE_GAME;
int ActiveAddOns = 1 << ADDON_BASE_GAME;
AddonType RequiredAddon = ADDON_BASE_GAME;

/*
 * An installation may hold more than one game, each in a folder of its own, and the launcher
 * that starts the game names the one it wants. A game named this way is settled here rather
 * than by the dialog, whose question nobody would be left to answer.
 */
static bool GameTypePresetGiven = false;
static AddonType GameTypePreset = ADDON_BASE_GAME;

/// <summary>
/// Steps an addon type on to the value after it.
/// </summary>
/// <returns>Returns with the incremented value.</returns>
AddonType operator++(AddonType & val)
{
	val = AddonType(int(val) + 1);
	return(val);
}


/// <summary>
/// Steps an addon type back to the value before it.
/// </summary>
/// <returns>Returns with the decremented value.</returns>
AddonType operator--(AddonType & val)
{
	val = AddonType(int(val) - 1);
	return(val);
}


/// <summary>
/// Asks the player which game they wish to play.
/// This routine puts up the game type dialog whenever an expansion has been detected, and
/// enables whichever addon the player settles on. With nothing but the base game present
/// there is no choice to make, so the dialog never appears.
/// </summary>
/// <param name="type">The addon that the player chose to play.</param>
/// <returns>bool; Should the game carry on? Returns false if the player backed out.</returns>
bool Select_Game_Type_Dialog(AddonType &type)
{
	int retval;

	type = ADDON_BASE_GAME;

	/*
	**	The launcher has already answered the question, so the game it named is set up here
	**	exactly as the dialog below would have set it up.
	*/
	if (Get_Game_Type_Preset(type)) {
		if (type == ADDON_FIRESTORM) {
			Enable_Addon(ADDON_FIRESTORM);
		} else {
			Disable_Addon(ADDON_ANY);
		}

		Set_Required_Addon(type);
		return(true);
	}

	if (Addon_Installed(ADDON_ANY)) {
		HWND dialog = OwnerDraw::Begin_Dialog(IDD_SELECT_GAME_TYPE, Select_Game_Type_Dialog_Proc);
		if (dialog != 0) {

			SetWindowLongPtr(dialog, DWLP_USER, (LONG_PTR)&retval);
			OwnerDraw::Display_Dialog(dialog);

			retval = -1;
			while (retval == -1) {
				if (OwnerDraw::Dialog_Message_Handler() == true) {
					break;
				}

				Title_Screen_Restore(false);
			}

			ShowWindow(dialog, SW_HIDE);
			UpdateWindow(MainWindow);
			OwnerDraw::End_Dialog(dialog);
			ActiveAddOns = 1 << ADDON_BASE_GAME;

			switch (retval) {
				default:
					type = ADDON_BASE_GAME;
					break;

				case IDC_GAMETYPE_FIRESTORM:
					Enable_Addon(ADDON_FIRESTORM);
					type = ADDON_FIRESTORM;
					break;

				case IDCANCEL:
					return(false);
			}
		}

		Set_Required_Addon(type);
		return(true);
	}

	return(true);
}


/// <summary>
/// Handles the messages for the game type selection dialog.
/// This routine stashes the control that the player pressed into the caller's result
/// variable, which is what lets the dialog loop know it can stop.
/// </summary>
INT_PTR CALLBACK Select_Game_Type_Dialog_Proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	int * retval;

	INT_PTR rc = OwnerDraw::Default_Dialog_Proc(window, message, wparam, lparam);

	if (rc == 0) {
		switch (message) {
			case WM_COMMAND:
				retval = (int *)GetWindowLongPtr(window, DWLP_USER);
				*retval = LOWORD(wparam);
				break;
		}
		rc = 0;
	}

	return(rc);
}


/*
 * An installation may hold a second game beside this one, in a folder of its own. The
 * launcher may name it outright; without that, the folder is looked for beside the data
 * folder under the name a player is most likely to have given it.
 */
static std::string OtherGameDirectory;


/// <summary>
/// Records where the second game's data folder is.
/// </summary>
/// <param name="path">The folder named on the command line.</param>
void Set_Other_Game_Directory(char const * path)
{
	std::string directory = path != NULL ? path : "";

	if (!directory.empty() && directory.back() != '\\' && directory.back() != '/') {
		directory += '\\';
	}

	OtherGameDirectory = directory;
}


/// <summary>
/// Fetches the second game's data folder, working it out from this one when nobody named it.
/// </summary>
/// <returns>Returns with the folder, ending in a separator, or an empty string when there is
/// no second game to reach.</returns>
char const * Other_Game_Directory(void)
{
	static std::string resolved;
	static bool resolved_once = false;

	if (!resolved_once) {
		resolved_once = true;

		if (!OtherGameDirectory.empty()) {
			resolved = OtherGameDirectory;
		} else {
			std::string const & data = Data_Directory();

			if (!data.empty()) {
				resolved = data + "..\\TwistedInsurrection\\";
			}
		}
	}

	return(resolved.c_str());
}


/// <summary>
/// Is there a second game installed beside this one that can be started?
/// </summary>
/// <returns>bool; Is the other game's folder there with an executable in it?</returns>
bool Other_Game_Available(void)
{
	char const * dir = Other_Game_Directory();

	if (dir == NULL || dir[0] == '\0') {
		return(false);
	}

	std::string exe = std::string(dir) + "Game.exe";

	return(RawFileClass(exe.c_str()).Is_Available());
}


/// <summary>
/// Starts the second game and leaves this instance to bow out.
/// The game refuses to run twice on one machine, so the guard mutexes are let go of first.
/// </summary>
/// <returns>bool; Was the other game started?</returns>
bool Launch_Other_Game(void)
{
	char const * dir = Other_Game_Directory();

	if (dir == NULL || dir[0] == '\0') {
		return(false);
	}

	std::string exe = std::string(dir) + "Game.exe";
	std::string command = "\"" + exe + "\" -DATADIR=\"" + std::string(dir) + "\"";

	// The mutexes that keep a second copy from running are released so that the copy
	// started here is not turned away at the door.
	if (AppMutex != NULL) {
		CloseHandle(AppMutex);
		AppMutex = NULL;
	}

	if (AutoPlayMutex != NULL) {
		CloseHandle(AutoPlayMutex);
		AutoPlayMutex = NULL;
	}

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	memset(&si, 0, sizeof(si));
	memset(&pi, 0, sizeof(pi));
	si.cb = sizeof(si);

	if (!CreateProcessA(NULL, const_cast<char *>(command.c_str()), NULL, NULL, FALSE, 0, NULL,
		dir, &si, &pi)) {
		return(false);
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return(true);
}


/// <summary>
/// Records the game a launcher asked for, named by the option it was passed.
/// An unrecognized name leaves the choice to the dialog, which is what happens when the game
/// is started directly.
/// </summary>
/// <param name="name">The name from the command line, such as "TS" or "FIRESTORM".</param>
void Set_Game_Type_Preset(char const * name)
{
	if (name == NULL) return;

	if (stricmp(name, "TS") == 0 || stricmp(name, "TIBSUN") == 0
		|| stricmp(name, "TIBERIAN SUN") == 0 || stricmp(name, "BASE") == 0) {
		GameTypePreset = ADDON_BASE_GAME;
		GameTypePresetGiven = true;
	} else if (stricmp(name, "FS") == 0 || stricmp(name, "FIRESTORM") == 0) {
		GameTypePreset = ADDON_FIRESTORM;
		GameTypePresetGiven = true;
	}
}


/// <summary>
/// Fetches the game a launcher asked for.
/// </summary>
/// <param name="type">Receives the name of the game.</param>
/// <returns>bool; Did the command line name a game?</returns>
bool Get_Game_Type_Preset(AddonType & type)
{
	if (!GameTypePresetGiven) return(false);

	type = GameTypePreset;
	return(true);
}


/// <summary>
/// Rebuilds the installed and active addon sets from the expansion rules files present.
/// </summary>
void Detect_Addons(void)
{
	AvailableAddOns = (1 << ADDON_BASE_GAME);
	ActiveAddOns = (1 << ADDON_BASE_GAME);

	if (CCFileClass(DeploymentConfig.RulesExpansionFile.c_str()).Is_Available() == true) {
		AvailableAddOns |= (1 << ADDON_FIRESTORM);
	}
}


/// <summary>
/// Is the specified addon installed on this machine?
/// Pass ADDON_ANY to ask whether any expansion at all was found.
/// </summary>
/// <returns>bool; Is the addon present?</returns>
bool Addon_Installed(AddonType addon)
{
	if (addon == ADDON_ANY) {
		return((AvailableAddOns & ~(1 << ADDON_BASE_GAME)) != false);
	}

	return((AvailableAddOns & (1 << addon)) != false);
}


/// <summary>
/// Is the specified addon switched on?
/// An addon must be both installed and enabled to count. Pass ADDON_ANY to ask whether any
/// expansion at all is currently in force.
/// </summary>
/// <returns>bool; Is the addon active?</returns>
bool Addon_Enabled(AddonType addon)
{
	if (addon == ADDON_ANY) {
		return((ActiveAddOns & AvailableAddOns & ~(1 << ADDON_BASE_GAME)) != false);
	}
	return((ActiveAddOns & AvailableAddOns & (1 << addon)) != false);
}


/// <summary>
/// Switches on an installed addon.
/// Only an addon that was actually detected can be enabled, so asking for one that is not
/// present is harmless. Pass ADDON_ANY to switch on everything that is installed.
/// </summary>
void Enable_Addon(AddonType addon)
{
	if (addon == ADDON_ANY) {
		ActiveAddOns = AvailableAddOns;
	} else if (addon > ADDON_BASE_GAME) {
		ActiveAddOns |= AvailableAddOns & (1 << addon);
	}
}


/// <summary>
/// Switches an addon back off.
/// Pass ADDON_ANY to drop all the way back to the base game.
/// </summary>
void Disable_Addon(AddonType addon)
{
	if (addon == ADDON_ANY) {
		ActiveAddOns = (1 << ADDON_BASE_GAME);
	} else if (addon > ADDON_BASE_GAME) {
		ActiveAddOns &= ~(1 << addon);
	}
}


/// <summary>
/// Is the specified addon the one the current game demands?
/// </summary>
/// <returns>bool; Is this the required addon?</returns>
bool Is_Required_Addon(AddonType addon)
{
	return(addon == RequiredAddon);
}


/// <summary>
/// Fetches the addon that the current game demands.
/// </summary>
/// <returns>Returns with the required addon.</returns>
AddonType Get_Required_Addon(void)
{
	return(RequiredAddon);
}


/// <summary>
/// Sets the addon that the current game demands.
/// This routine is called when the player picks a game type from the menus, and again when
/// a scenario or a saved game states which addon its rules were built for.
/// </summary>
void Set_Required_Addon(AddonType addon)
{
	RequiredAddon = addon;
}


/// <summary>
/// Fetches the display name of an addon.
/// </summary>
/// <returns>Returns with the localized title text for the addon specified.</returns>
const char * Get_Addon_Title(AddonType addon)
{
	static int _id[ADDON_COUNT] = {
		TXT_SHORT_TITLE,
		TXT_EXPANSION_TITLE,
	};
	return(Fetch_String(_id[addon]));
}
