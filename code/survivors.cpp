/*******************************************************************************
 *                                O P E N T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "survivors.h"

#include "ccini.h"
#include "ccrand.h"
#include "dbgprint.h"
#include "dialog.h"
#include "display.h"
#include "draw.h"
#include "foot.h"
#include "globals.h"
#include "house.h"
#include "house.hh"
#include "houstype.h"
#include "infantry.h"
#include "infatype.h"
#include "map.h"
#include "rgb.h"
#include "rules.h"
#include "scenario.h"
#include "scheme.h"
#include "surface.h"
#include "_tactica.h"
#include "tactical.h"
#include "unit.h"
#include "unittype.h"

#include <cstdio>
#include <cstring>


SurvivorsClass Survivors;


/// <summary>
/// How far from the hero a wave is stood down, and how much ground the squad may give.
/// </summary>
static int const WAVE_RADIUS_MIN = 8;
static int const WAVE_RADIUS_MAX = 14;
static int const SQUAD_LEASH = 5;
static int const WAVE_GRACE = 5;			///< seconds before the first wave
static int const WAVE_PERIOD = 40;			///< seconds between waves before it tightens


SurvivorsClass::SurvivorsClass(void) :
	Enabled(false),
	Phase(PHASE_OFF),
	Wave(0),
	WaveTimer(0),
	WaveInterval(WAVE_PERIOD * TICKS_PER_SECOND),
	SquadTimer(0),
	CameraTimer(0),
	SquadCount(5),
	Enemy(NULL),
	Hero(NULL)
{
	HeroName[0] = '\0';
	SquadName[0] = '\0';
}


/// <summary>
/// Reads the map's own [Survivors] section. A map that carries one is played as the mode.
/// </summary>
void SurvivorsClass::Read_INI(CCINIClass const & ini)
{
	Enabled = ini.Is_Present("Survivors");
	Reset();

	if (!Enabled) {
		DebugString("Survivors: no [Survivors] section in this mission\n");
		return;
	}

	ini.Get_String("Survivors", "Hero", "", HeroName, sizeof(HeroName));
	ini.Get_String("Survivors", "Squad", "", SquadName, sizeof(SquadName));
	SquadCount = ini.Get_Int("Survivors", "SquadCount", 5);
	if (SquadCount < 1) SquadCount = 1;
	if (SquadCount > 12) SquadCount = 12;

	int period = ini.Get_Int("Survivors", "WavePeriod", WAVE_PERIOD);
	if (period < 10) period = 10;
	WaveInterval = period * TICKS_PER_SECOND;

	DebugString("Survivors: the map asks for the mode (hero '%s', squad '%s' x%d)\n",
		HeroName, SquadName, SquadCount);
}


/// <summary>Clears whatever run was in progress, leaving the mode enabled as read.</summary>
void SurvivorsClass::Reset(void)
{
	Phase = PHASE_OFF;
	Wave = 0;
	WaveTimer = 0;
	SquadTimer = 0;
	CameraTimer = 0;
	GraceTimer = 0;
	Enemy = NULL;
	Hero = NULL;
	Squad.Clear();
}


/// <summary>
/// Finds the infantry type a name refers to, or the strongest one the house may field when the
/// map names nothing.
/// </summary>
static InfantryTypeClass const * Pick_Infantry(HouseClass * house, char const * name, bool strongest)
{
	if (house == NULL || house->Class == NULL) {
		return(NULL);
	}

	if (name != NULL && name[0] != '\0') {
		InfantryType type = InfantryTypeClass::From_Name(name);
		if (type != INFANTRY_NONE) {
			return(InfantryTypes[type]);
		}
	}

	unsigned int mask = 1U << house->Class->HeapID;
	InfantryTypeClass const * best = NULL;

	for (int index = 0; index < InfantryTypes.Count(); index++) {
		InfantryTypeClass const * candidate = InfantryTypes[index];
		if (candidate == NULL || !candidate->IsAllowedToStartInMultiplayer) {
			continue;
		}
		if ((candidate->Ownable & mask) == 0 || candidate->Level > house->Control.TechLevel) {
			continue;
		}
		if (best == NULL
			|| (strongest && candidate->MaxStrength > best->MaxStrength)
			|| (!strongest && candidate->MaxStrength < best->MaxStrength)) {
			best = candidate;
		}
	}

	return(best);
}


/// <summary>Stands one infantryman up near a cell, letting the game find him room.</summary>
static InfantryClass * Stand_Infantry(HouseClass * house, InfantryTypeClass const * type, Cell const & from)
{
	if (house == NULL || type == NULL) {
		return(NULL);
	}

	InfantryClass * man = new InfantryClass(type, house);
	if (man == NULL) {
		return(NULL);
	}

	/*
	 * **	The game's own scan looks for room around the cell first: standing a man on a cell an
	 *	object already fills leaves him to be crushed by it.
	 */
	bool placed = Scan_Place_Object(man, from) != 0;
	if (!placed) {
		placed = man->Unlimbo(Coord(from), DIR_N);
	}

	if (!placed) {
		delete man;
		return(NULL);
	}

	/*
	 * **	An object with no mission is one the game is free to clean up, so every man is given
	 *	one the moment he stands up.
	 */
	man->Assign_Mission(MISSION_GUARD);
	if (man->Ready_To_Commence()) {
		man->Commence();
	}

	return(man);
}


/// <summary>The house the waves belong to: the first one that is not on the player's side.</summary>
HouseClass * SurvivorsClass::Enemy_House(void) const
{
	if (PlayerPtr == NULL) {
		return(NULL);
	}

	for (int index = 0; index < Houses.Count(); index++) {
		HouseClass * house = Houses[index];
		if (house == NULL || house == PlayerPtr || house->Class == NULL) {
			continue;
		}
		if (house->ActLike != PlayerPtr->ActLike) {
			return(house);
		}
	}

	return(NULL);
}


/// <summary>Where the hero stands, or the middle of the map when he is not standing anywhere.</summary>
Cell SurvivorsClass::Hero_Cell(void) const
{
	if (Hero != NULL && Hero->IsActive) {
		return(Hero->Center_Coord().As_Cell());
	}

	return(Cell(Map.PlayRect.X + Map.PlayRect.Width / 2, Map.PlayRect.Y + Map.PlayRect.Height / 2));
}


/// <summary>
/// Starts a run: the hero and his squad are put down where the player would begin a mission.
/// </summary>
bool SurvivorsClass::Begin(void)
{
	if (PlayerPtr == NULL) {
		return(false);
	}

	Cell origin = Cell(Map.PlayRect.X + Map.PlayRect.Width / 2, Map.PlayRect.Y + Map.PlayRect.Height / 2);
	if (PlayerPtr->SpawnWaypoint >= 0) {
		origin = Scen->Get_Waypoint_Cell(PlayerPtr->SpawnWaypoint);
	}

	InfantryTypeClass const * hero_type = Pick_Infantry(PlayerPtr, HeroName, true);
	Hero = Stand_Infantry(PlayerPtr, hero_type, origin);

	if (Hero == NULL) {
		DebugString("Survivors: the hero could not be placed\n");
		Finish("hero");
		return(false);
	}

	Enemy = Enemy_House();
	Wave = 0;
	WaveInterval = WAVE_PERIOD * TICKS_PER_SECOND;
	WaveTimer = WAVE_GRACE * TICKS_PER_SECOND;
	Phase = PHASE_WAVE;
	GraceTimer = 3 * TICKS_PER_SECOND;

	Spawn_Squad();

	DebugString("Survivors: the run begins at %d,%d under %s with %d in the squad (%s), the enemy is %s\n",
		origin.X, origin.Y,
		hero_type != NULL ? (char const *)hero_type->IniName : "nothing",
		Squad.Count(),
		SquadName[0] != 0 ? SquadName : "the side's weakest",
		Enemy != NULL ? (char const *)Enemy->Class->IniName : "none");

	return(true);
}


/// <summary>Puts the squad down around the hero and tells each man whom to keep to.</summary>
void SurvivorsClass::Spawn_Squad(void)
{
	Squad.Clear();

	InfantryTypeClass const * type = Pick_Infantry(PlayerPtr, SquadName, false);
	Cell here = Hero_Cell();

	for (int index = 0; index < SquadCount; index++) {
		Cell spot = Cell(here.X + (index % 3) - 1, here.Y + (index / 3) - 1);
		TechnoClass * man = Stand_Infantry(PlayerPtr, type, spot);
		if (man == NULL) {
			continue;
		}
		man->Override_Mission(MISSION_GUARD, Hero, NULL);
		Squad.Add(man);
	}
}


/// <summary>
/// Pulls a straggler back to the hero. The squad is expected to drift while it fights, and the
/// mission brings back the men who drifted too far.
/// </summary>
void SurvivorsClass::Keep_Squad_Together(void)
{
	if (Hero == NULL || !Hero->IsActive) {
		return;
	}

	for (int index = 0; index < Squad.Count(); index++) {
		TechnoClass * man = Squad[index];
		if (man == NULL || !man->IsActive || man->In_Which_Layer() == LAYER_NONE) {
			continue;
		}
		if (man->Distance(Hero->Center_Coord()) > SQUAD_LEASH * 256) {
			man->Override_Mission(MISSION_GUARD, Hero, NULL);
		}
	}
}


/// <summary>
/// Keeps the hero on the screen: the camera is pulled to him when he has walked away from it.
/// </summary>
void SurvivorsClass::Follow_With_Camera(void)
{
	if (Hero == NULL || !Hero->IsActive || TacticalMap == NULL) {
		return;
	}

	TacticalMap->Set_Tactical_Position(Hero->Center_Coord());
}


/// <summary>
/// Stands one enemy down on a ring around the hero and sends him at him.
/// </summary>
TechnoClass * SurvivorsClass::Spawn_Enemy(HouseClass * house, Cell const & around, int radius)
{
	if (house == NULL) {
		return(NULL);
	}

	InfantryTypeClass const * type = Pick_Infantry(house, NULL, false);
	if (type == NULL) {
		return(NULL);
	}

	/*
	 * **	The man is put down somewhere on the ring, not on one fixed side of it: the wave is
	 *	meant to come from every direction.
	 */
	for (int tries = 0; tries < 40; tries++) {
		int angle = Random_Pick(0, 255);
		int dx = ((angle & 15) - 8) * radius / 8;
		int dy = (((angle >> 4) & 15) - 8) * radius / 8;

		/*
		 * **	The ring closes in as the tries run out, so a man is still found a place when the
		 *	ground further off is blocked.
		 */
		if (tries > 20) {
			radius = radius > 4 ? radius - 1 : radius;
			dx = ((angle & 15) - 8) * radius / 8;
			dy = (((angle >> 4) & 15) - 8) * radius / 8;
		}

		Cell spot = Cell(around.X + dx, around.Y + dy);

		if (!Map.In_Local_Radar(spot)) {
			continue;
		}

		TechnoClass * man = Stand_Infantry(house, type, spot);
		if (man == NULL) {
			continue;
		}

		man->Override_Mission(MISSION_HUNT, Hero, NULL);
		return(man);
	}

	return(NULL);
}


/// <summary>
/// Sends the next wave. The count grows with the wave number, so each one is a little harder to
/// stand against than the last.
/// </summary>
void SurvivorsClass::Spawn_Wave(void)
{
	if (Enemy == NULL) {
		Enemy = Enemy_House();
	}

	Wave++;

	int count = 3 + Wave;
	if (count > 20) count = 20;

	Cell here = Hero_Cell();
	int made = 0;

	for (int index = 0; index < count; index++) {
		int radius = Random_Pick(WAVE_RADIUS_MIN, WAVE_RADIUS_MAX);
		if (Spawn_Enemy(Enemy, here, radius) != NULL) {
			made++;
		}
	}

	/*
	 * **	The waves come faster as the run goes on, but never faster than half the opening pace.
	 */
	int interval = (WAVE_PERIOD - Wave) * TICKS_PER_SECOND;
	if (interval < (WAVE_PERIOD / 2) * TICKS_PER_SECOND) {
		interval = (WAVE_PERIOD / 2) * TICKS_PER_SECOND;
	}
	WaveInterval = interval;
	WaveTimer = interval;

	DebugString("Survivors: wave %d stands %d of %d enemies on the field\n", Wave, made, count);
}


/// <summary>Ends the run and says why.</summary>
void SurvivorsClass::Finish(char const * reason)
{
	Phase = PHASE_OVER;

	DebugString("Survivors: the run is over (%s) at wave %d\n", reason, Wave);

	if (PlayerPtr != NULL && reason != NULL && strcmp(reason, "hero") != 0) {
		PlayerPtr->Flag_To_Lose();
	}
}


/// <summary>One pass of the mode, driven from the game's own logic tick.</summary>
void SurvivorsClass::Logic(void)
{

	if (!Enabled) {
		return;
	}

	if (Phase == PHASE_OFF) {
		Begin();
		return;
	}

	if (Phase == PHASE_OVER) {
		return;
	}

	/*
	 * **	The hero falling is the end of the run. A hero who has only just been stood up is
	 *	not yet counted as fallen: the object is not in a layer until the game has placed it.
	 */
	if (Hero == NULL) {
		Finish("hero");
		return;
	}

	if (!Hero_Alive()) {
		if (GraceTimer > 0) {
			GraceTimer--;
			return;
		}
		DebugString("Survivors: the hero is gone from the field\n");
		Hero = NULL;
		Finish("hero");
		return;
	}

	if (--WaveTimer <= 0) {

		Spawn_Wave();
	}

	if (--SquadTimer <= 0) {
		SquadTimer = TICKS_PER_SECOND;

		Keep_Squad_Together();
	}

	if (--CameraTimer <= 0) {
		CameraTimer = 2 * TICKS_PER_SECOND;

		Follow_With_Camera();
	}
}


/// <summary>Draws the run's own line at the top of the field: the wave and its countdown.</summary>
void SurvivorsClass::Draw_On_Field(Surface & surface)
{
	if (Phase == PHASE_OFF) {
		return;
	}

	Rect whole(0, 0, surface.Get_Width(), surface.Get_Height());
	char text[128];

	int seconds = WaveTimer / TICKS_PER_SECOND;
	if (seconds < 0) seconds = 0;

	sprintf(text, "Волна %d   %d:%02d", Wave, seconds / 60, seconds % 60);

	Fancy_Text_Print(text, surface, whole,
		Point2D(surface.Get_Width() / 2, 10), Fetch_Scheme_By_Name("Green"), TBLACK,
		TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	sprintf(text, "Отряд %d   Врагов %d", Squad.Count(), Enemies_On_Field());

	Fancy_Text_Print(text, surface, whole,
		Point2D(surface.Get_Width() / 2, 22), Fetch_Scheme_By_Name("LightGrey"), TBLACK,
		TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));
}

/// <summary>
/// Says whether the hero is still among the objects the game keeps. A pointer alone is not
/// enough: an object the game has taken away leaves the pointer to freed memory behind.
/// </summary>
bool SurvivorsClass::Hero_Alive(void) const
{
	if (Hero == NULL) {
		return(false);
	}

	for (int index = 0; index < Infantry.Count(); index++) {
		if (Infantry[index] == Hero) {
			return(true);
		}
	}

	for (int index = 0; index < Units.Count(); index++) {
		if (Units[index] == Hero) {
			return(true);
		}
	}

	return(false);
}

/// <summary>How many of the enemy are on the field, which is what the run is measured in.</summary>
int SurvivorsClass::Enemies_On_Field(void) const
{
	if (Enemy == NULL) {
		return(0);
	}

	int count = 0;

	for (int index = 0; index < Infantry.Count(); index++) {
		InfantryClass * man = Infantry[index];
		if (man != NULL && man->IsActive && man->House == Enemy) {
			count++;
		}
	}

	for (int index = 0; index < Units.Count(); index++) {
		UnitClass * unit = Units[index];
		if (unit != NULL && unit->IsActive && unit->House == Enemy) {
			count++;
		}
	}

	return(count);
}


/// <summary>
/// The mode has no buttons of its own yet; the card table will be handled here once there are
/// cards to choose from.
/// </summary>
bool SurvivorsClass::Click(Point2D const &)
{
	return(false);
}
