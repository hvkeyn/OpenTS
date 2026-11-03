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
#include "dsurface.h"
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
#include "_rules.h"
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


/// <summary>The distances a wave is stood down from the hero, and how far the squad may stray.</summary>
static int const WAVE_RADIUS_MIN = 8;
static int const WAVE_RADIUS_MAX = 14;
static int const SQUAD_LEASH = 5;
static int const WAVE_GRACE = 5;			///< seconds before the first wave
static int const WAVE_PERIOD = 40;			///< seconds between waves before it tightens
static int const LAST_WAVE = 10;			///< the wave after which the way out opens
static int const CRYSTAL_LIFE = 30;			///< seconds a crystal lies before it is lost
static int const EVAC_TIME = 90;			///< seconds given to reach the way out


/// <summary>
/// One hero a side can offer: what the player sees, the man he drives, the squad that comes with
/// him and the skill he is remembered for.
/// </summary>
struct HeroInfo {
	char const * Name;
	char const * Unit;
	char const * Squad;
	char const * Skill;
};

static char const * const SIDE_NAMES[3] = { "ГСБ", "Братство НОД", "Мутанты" };

static HeroInfo const SIDE_HEROES[3][3] = {
	{
		{ "Спецназ", "GHOST", "E1", "Ионный удар" },
		{ "Гренадёр", "E2", "E1", "Залп по площади" },
		{ "Ракетчик", "E3", "E1", "Точный залп" },
	},
	{
		{ "Фанатик", "E3", "E1", "Ярость отряда" },
		{ "Киборг", "MUTANT3", "E1", "Импульс по технике" },
		{ "Поджигатель", "E2", "E1", "Поджог" },
	},
	{
		{ "Умагон", "UMAGON", "MUTANT", "Яд и скорость" },
		{ "Вожак", "MUTANT3", "MUTANT", "Призыв висцероидов" },
		{ "", "", "", "" },
	},
};

static int const SIDE_HERO_COUNT[3] = { 3, 3, 2 };


/// <summary>The card table: what the player is offered when a level is reached.</summary>
struct CardInfo {
	char const * Name;
	char const * Line;
	SurvivorsClass::EffectType Effect;
};

static CardInfo const CARDS[SurvivorsClass::EFFECT_COUNT] = {
	{ "", "", SurvivorsClass::EFFECT_NONE },
	{ "Подкрепление", "Двое бойцов встают в отряд", SurvivorsClass::EFFECT_REINFORCE },
	{ "Ремонт", "Герой и отряд вылечены", SurvivorsClass::EFFECT_HEAL },
	{ "Тибериевая жила", "Кристаллы дают больше опыта", SurvivorsClass::EFFECT_CRYSTAL },
	{ "Передышка", "Волны приходят реже", SurvivorsClass::EFFECT_BREATHER },
	{ "Второй шанс", "Герой переживает одну смерть", SurvivorsClass::EFFECT_REVIVE },
	{ "Строй", "Отряд держится ближе и не разбегается", SurvivorsClass::EFFECT_SHIELD },
	{ "Магнит", "Тиберий втягивается с большего расстояния", SurvivorsClass::EFFECT_MAGNET },
	{ "Притяжение", "Тиберий летит к герою быстрее", SurvivorsClass::EFFECT_PULL },
};


SurvivorsClass::SurvivorsClass(void) :
	FrameW(1920),
	FrameH(1080),
	Enabled(false),
	Phase(PHASE_OFF),
	Wave(0),
	WaveTimer(0),
	WaveInterval(WAVE_PERIOD * TICKS_PER_SECOND),
	SquadTimer(0),
	CameraTimer(0),
	GraceTimer(0),
	FoeTimer(0),
	SquadCount(5),
	SideIndex(0),
	HeroIndex(0),
	AutoPick(false),
	UseMapHero(false),
	Xp(0),
	XpNeeded(12),
	Level(1),
	Crystals(0),
	Kills(0),
	CrystalWorth(100),
	WaveScale(100),
	ReviveHeld(false),
	Revives(0),
	CardPick(0),
	CardTimer(0),
	LastWave(LAST_WAVE),
	PhaseTimer(0),
	MagnetRadius(12),
	PullStep(1),
	TaskActive(false),
	TaskCell(CELL_NONE),
	TaskHeld(0),
	TaskLeft(0),
	TaskAway(0),
	BossEvery(5),
	StatusTimer(0),
	BossPending(0),
	EvacTimer(0),
	EvacCell(CELL_NONE),
	Enemy(NULL),
	Hero(NULL)
{
	HeroName[0] = '\0';
	SquadName[0] = '\0';
	strcpy(StageName, "Полигон: Герой на тиберии");
	strcpy(StageLine, "Держите героя живым: волны идут со всех сторон.");
	memset(Cards, 0, sizeof(Cards));
}


/// <summary>Reads the map's own [Survivors] section. A map that carries one is played as the mode.</summary>
void SurvivorsClass::Read_INI(CCINIClass const & ini)
{
	Enabled = ini.Is_Present("Survivors");
	Reset();

	if (!Enabled) {
		return;
	}

	ini.Get_String("Survivors", "Hero", "", HeroName, sizeof(HeroName));
	ini.Get_String("Survivors", "Squad", "", SquadName, sizeof(SquadName));
	ini.Get_String("Survivors", "Stage", "", StageName, sizeof(StageName));
	ini.Get_String("Survivors", "Task", "", StageLine, sizeof(StageLine));

	SquadCount = ini.Get_Int("Survivors", "SquadCount", 5);
	if (SquadCount < 1) SquadCount = 1;
	if (SquadCount > 12) SquadCount = 12;

	int period = ini.Get_Int("Survivors", "WavePeriod", WAVE_PERIOD);
	if (period < 10) period = 10;
	WaveInterval = period * TICKS_PER_SECOND;

	AutoPick = ini.Get_Bool("Survivors", "AutoPick", false);
	LastWave = ini.Get_Int("Survivors", "LastWave", LAST_WAVE);
	BossEvery = ini.Get_Int("Survivors", "BossEvery", 5);

	/*
	 * **	A map that names the hero itself is a proving ground of that hero: the choice of side
	 *	and hero is left out and the run starts at once.
	 */
	UseMapHero = (HeroName[0] != '\0');

	DebugString("Survivors: the map asks for the mode (stage '%s', hero '%s', squad '%s' x%d)\n",
		StageName, HeroName, SquadName, SquadCount);
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
	FoeTimer = 0;
	Xp = 0;
	XpNeeded = 8;
	Level = 1;
	Crystals = 0;
	Kills = 0;
	CrystalWorth = 100;
	WaveScale = 100;
	ReviveHeld = false;
	Revives = 0;
	CardPick = 0;
	CardTimer = 0;
	BossPending = 0;
	EvacTimer = 0;
	EvacCell = CELL_NONE;
	Enemy = NULL;
	Hero = NULL;
	Squad.Clear();
	Foes.Clear();
	FoeCells.Clear();
	Pickups.Clear();
}


/// <summary>Finds the infantry type a name refers to, or the best the house may field.</summary>
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


/// <summary>Says whether an object is still standing on the field.</summary>
static bool On_Field(TechnoClass * object)
{
	if (object == NULL) {
		return(false);
	}

	for (int index = 0; index < Infantry.Count(); index++) {
		if (Infantry[index] == object) {
			return(true);
		}
	}

	for (int index = 0; index < Units.Count(); index++) {
		if (Units[index] == object) {
			return(true);
		}
	}

	return(false);
}


/// <summary>Says whether one of the watched men is already on the list.</summary>
static bool Watched(DynamicVectorClass<TechnoClass *> const & list, TechnoClass * who)
{
	for (int index = 0; index < list.Count(); index++) {
		if (list[index] == who) {
			return(true);
		}
	}
	return(false);
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
	if (Hero != NULL && Hero_Alive()) {
		return(Hero->Center_Coord().As_Cell());
	}

	return(Cell(Map.PlayRect.X + Map.PlayRect.Width / 2, Map.PlayRect.Y + Map.PlayRect.Height / 2));
}


/// <summary>Says whether the hero is still among the objects the game keeps.</summary>
bool SurvivorsClass::Hero_Alive(void) const
{
	return(On_Field(Hero));
}


/// <summary>Starts a run: the chosen hero and his squad are put down where the player begins.</summary>
bool SurvivorsClass::Begin(void)
{
	if (PlayerPtr == NULL) {
		return(false);
	}

	char const * unit_name = HeroName;
	char const * squad_name = SquadName;

	if (!UseMapHero) {
		if (SideIndex < 0 || SideIndex > 2) SideIndex = 0;
		if (HeroIndex < 0 || HeroIndex >= SIDE_HERO_COUNT[SideIndex]) HeroIndex = 0;

		HeroInfo const & hero = SIDE_HEROES[SideIndex][HeroIndex];
		unit_name = hero.Unit;
		squad_name = hero.Squad;
	}

	Cell origin = Cell(Map.PlayRect.X + Map.PlayRect.Width / 2, Map.PlayRect.Y + Map.PlayRect.Height / 2);
	if (PlayerPtr->SpawnWaypoint >= 0) {
		origin = Scen->Get_Waypoint_Cell(PlayerPtr->SpawnWaypoint);
	}

	InfantryTypeClass const * hero_type = Pick_Infantry(PlayerPtr, unit_name, true);
	Hero = Stand_Infantry(PlayerPtr, hero_type, origin);

	if (Hero == NULL) {
		DebugString("Survivors: the hero could not be placed\n");
		Finish("hero");
		return(false);
	}

	Enemy = Enemy_House();
	Wave = 0;
	WaveTimer = WAVE_GRACE * TICKS_PER_SECOND;
	Phase = PHASE_WAVE;
	GraceTimer = 3 * TICKS_PER_SECOND;
	Xp = 0;
	XpNeeded = 8;
	Level = 1;

	Spawn_Squad();
	Spawn_Squad();

	/*
	**	The hero is handed to the player at once: the run is played by driving him, and the men
	**	around him only keep to him.
	*/

	Start_Task();
	DebugString("Survivors: the run begins at %d,%d under %s with %d in the squad (%s), the enemy is %s\n",
		origin.X, origin.Y,
		hero_type != NULL ? (char const *)hero_type->IniName : "nothing",
		Squad.Count(),
		squad_name != NULL && squad_name[0] != '\0' ? squad_name : "the side's weakest",
		Enemy != NULL ? (char const *)Enemy->Class->IniName : "none");

	return(true);
}


/// <summary>Puts the squad down around the hero and tells each man whom to keep to.</summary>
void SurvivorsClass::Spawn_Squad(void)
{
	Squad.Clear();
	Add_To_Squad(SquadCount);
}


/// <summary>Brings men up around the hero and puts them under his wing.</summary>
void SurvivorsClass::Add_To_Squad(int how_many)
{
	char const * squad_name = SquadName;
	if (!UseMapHero) {
		squad_name = SIDE_HEROES[SideIndex][HeroIndex].Squad;
	}

	InfantryTypeClass const * type = Pick_Infantry(PlayerPtr, squad_name, false);
	Cell here = Hero_Cell();

	for (int index = 0; index < how_many; index++) {
		Cell spot = Cell(here.X + (index % 3) - 1, here.Y + (index / 3) - 1);
		TechnoClass * man = Stand_Infantry(PlayerPtr, type, spot);
		if (man == NULL) {
			continue;
		}
		man->Override_Mission(MISSION_GUARD, Hero, NULL);
		Squad.Add(man);
	}
}


/// <summary>Mends the hero and everyone still with him.</summary>
void SurvivorsClass::Mend(void)
{
	if (Hero != NULL && Hero_Alive()) {
		Hero->Strength = Hero->Techno_Type_Class()->MaxStrength;
	}

	for (int index = 0; index < Squad.Count(); index++) {
		TechnoClass * man = Squad[index];
		if (man != NULL && On_Field(man)) {
			man->Strength = man->Techno_Type_Class()->MaxStrength;
		}
	}
}


/// <summary>Pulls a straggler back to the hero.</summary>
void SurvivorsClass::Keep_Squad_Together(void)
{
	if (Hero == NULL || !Hero_Alive()) {
		return;
	}

	for (int index = 0; index < Squad.Count(); index++) {
		TechnoClass * man = Squad[index];
		if (man == NULL) {
			continue;
		}
		if (man->Distance(Hero->Center_Coord()) > SQUAD_LEASH * 256) {
			man->Override_Mission(MISSION_GUARD, Hero, NULL);
		}
	}
}


/// <summary>Keeps the hero on the screen: the camera is pulled to him as he walks away.</summary>
void SurvivorsClass::Follow_With_Camera(void)
{
	if (Hero == NULL || !Hero_Alive() || TacticalMap == NULL) {
		return;
	}

	TacticalMap->Set_Tactical_Position(Hero->Center_Coord());
}


/// <summary>Stands one enemy down on a ring around the hero and sends him at him.</summary>
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
	 *	meant to come from every direction. The ring closes in as the tries run out, so a man is
	 *	still found a place when the ground further off is blocked.
	 */
	for (int tries = 0; tries < 40; tries++) {
		int angle = Random_Pick(0, 255);
		int dx = ((angle & 15) - 8) * radius / 8;
		int dy = (((angle >> 4) & 15) - 8) * radius / 8;

		if (tries > 20 && radius > 4) {
			radius--;
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


/// <summary>Brings in the heavy unit a boss wave is made of.</summary>
TechnoClass * SurvivorsClass::Spawn_Boss_Unit(HouseClass * house, Cell const & around, int radius)
{
	if (house == NULL || house->Class == NULL) {
		return(NULL);
	}

	unsigned int mask = 1U << house->Class->HeapID;
	UnitTypeClass const * type = NULL;

	for (int index = 0; index < UnitTypes.Count(); index++) {
		UnitTypeClass const * candidate = UnitTypes[index];
		if (candidate == NULL || !candidate->IsAllowedToStartInMultiplayer) {
			continue;
		}
		if ((candidate->Ownable & mask) == 0 || candidate->Level > house->Control.TechLevel) {
			continue;
		}
		if (Rule->BaseUnit.Is_In_List(candidate) || Rule->HarvesterUnit.Is_In_List(candidate)) {
			continue;
		}
		if (type == NULL || candidate->MaxStrength > type->MaxStrength) {
			type = candidate;
		}
	}

	if (type == NULL) {
		return(NULL);
	}

	for (int tries = 0; tries < 40; tries++) {
		int angle = Random_Pick(0, 255);
		int dx = ((angle & 15) - 8) * radius / 8;
		int dy = (((angle >> 4) & 15) - 8) * radius / 8;

		if (tries > 20 && radius > 4) {
			radius--;
			dx = ((angle & 15) - 8) * radius / 8;
			dy = (((angle >> 4) & 15) - 8) * radius / 8;
		}

		Cell spot = Cell(around.X + dx, around.Y + dy);
		if (!Map.In_Local_Radar(spot)) {
			continue;
		}

		UnitClass * unit = new UnitClass(type, house);
		if (unit == NULL) {
			return(NULL);
		}

		bool placed = Scan_Place_Object(unit, spot) != 0;
		if (!placed) {
			placed = unit->Unlimbo(Coord(spot), DIR_N);
		}
		if (!placed) {
			delete unit;
			continue;
		}

		unit->Override_Mission(MISSION_HUNT, Hero, NULL);
		if (unit->Ready_To_Commence()) {
			unit->Commence();
		}

		return(unit);
	}

	return(NULL);
}


/// <summary>Sends the next wave, or the boss that closes a group of five.</summary>
void SurvivorsClass::Spawn_Wave(void)
{
	if (Enemy == NULL) {
		Enemy = Enemy_House();
	}

	Wave++;

	bool boss = (BossEvery > 0 && (Wave % BossEvery) == 0);

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

	if (boss) {
		if (Spawn_Boss_Unit(Enemy, here, WAVE_RADIUS_MAX) != NULL) {
			DebugString("Survivors: a boss stands with wave %d\n", Wave);
		}
	}

	/*
	 * **	The waves come faster as the run goes on, but never faster than half the opening pace,
	 *	and a card can slow them further.
	 */
	int interval = (WAVE_PERIOD - Wave) * TICKS_PER_SECOND;
	if (interval < (WAVE_PERIOD / 2) * TICKS_PER_SECOND) {
		interval = (WAVE_PERIOD / 2) * TICKS_PER_SECOND;
	}
	interval = interval * WaveScale / 100;
	WaveInterval = interval;
	WaveTimer = interval;

	DebugString("Survivors: wave %d stands %d of %d enemies%s\n", Wave, made, count,
		boss ? " and a boss" : "");

	if (Wave >= LastWave && Phase == PHASE_WAVE) {
		Start_Evacuation();
	}
}


/// <summary>Looks for the men who have fallen since the last look and leaves their tiberium.</summary>
void SurvivorsClass::Watch_Foes(void)
{
	if (Enemy == NULL) {
		return;
	}

	for (int index = Foes.Count() - 1; index >= 0; index--) {
		TechnoClass * foe = Foes[index];
		Cell where = (index < FoeCells.Count()) ? FoeCells[index] : CELL_NONE;

		if (!On_Field(foe)) {
			if (where != CELL_NONE) {
				Drop_Crystal(where, 4);
			}
			Foes.Delete_Index(index);
			if (index < FoeCells.Count()) {
				FoeCells.Delete_Index(index);
			}
			Kills++;
		}
	}

	for (int index = 0; index < Foes.Count(); index++) {
		TechnoClass * foe = Foes[index];
		if (foe != NULL && index < FoeCells.Count()) {
			FoeCells[index] = foe->Center_Coord().As_Cell();
		}
	}

	for (int index = 0; index < Infantry.Count(); index++) {
		InfantryClass * man = Infantry[index];
		if (man == NULL || !man->IsActive || man->House != Enemy) {
			continue;
		}
		if (Watched(Foes, man)) {
			continue;
		}
		Foes.Add(man);
		FoeCells.Add(man->Center_Coord().As_Cell());
	}

	for (int index = 0; index < Units.Count(); index++) {
		UnitClass * unit = Units[index];
		if (unit == NULL || !unit->IsActive || unit->House != Enemy) {
			continue;
		}
		if (Watched(Foes, unit)) {
			continue;
		}
		Foes.Add(unit);
		FoeCells.Add(unit->Center_Coord().As_Cell());
	}
}


/// <summary>Leaves a piece of tiberium where a man fell.</summary>
void SurvivorsClass::Drop_Crystal(Cell const & where, int value)
{
	if (Pickups.Count() > 120) {
		return;
	}

	if (Random_Pick(0, 99) >= 60) {
		return;
	}

	PickupType crystal;
	crystal.Where = where;
	crystal.Life = CRYSTAL_LIFE * TICKS_PER_SECOND;
	crystal.Value = value;
	Pickups.Add(crystal);
}


/// <summary>Picks up whatever the hero is standing on, and turns it into experience.</summary>
void SurvivorsClass::Collect_Crystals(void)
{
	if (Hero == NULL || !Hero_Alive()) {
		return;
	}

	Cell here = Hero->Center_Coord().As_Cell();

	for (int index = Pickups.Count() - 1; index >= 0; index--) {
		PickupType crystal = Pickups[index];

		crystal.Life--;
		if (crystal.Life <= 0) {
			Pickups.Delete_Index(index);
			continue;
		}
		Pickups[index].Life = crystal.Life;

		int dx = crystal.Where.X - here.X;
		int dy = crystal.Where.Y - here.Y;
		int ax = dx < 0 ? -dx : dx;
		int ay = dy < 0 ? -dy : dy;

		/*
		**	Tiberium near the hero is pulled towards him a step at a time, which is what makes it
		**	visible that he is gathering it; it counts towards the level only once it reaches him.
		*/
		if ((ax > 1 || ay > 1) && ax <= MagnetRadius && ay <= MagnetRadius) {
			Cell step = crystal.Where;

			for (int pull = 0; pull < PullStep; pull++) {
				if (ax > 0) {
					step.X += (dx > 0) ? 1 : -1;
					ax--;
					dx += (dx > 0) ? -1 : 1;
				}
				if (ay > 0) {
					step.Y += (dy > 0) ? 1 : -1;
					ay--;
					dy += (dy > 0) ? -1 : 1;
				}
			}

			Pickups[index].Where = step;
			crystal.Where = step;
		}

		if (ax <= 1 && ay <= 1) {
			Xp += crystal.Value * CrystalWorth / 100;
			Crystals++;
			Pickups.Delete_Index(index);
		}
	}

	if (Xp >= XpNeeded && Phase == PHASE_WAVE) {
		Open_Cards();
	}
}


/// <summary>Opens the card table with three different improvements.</summary>
void SurvivorsClass::Open_Cards(void)
{
	Cards[0] = 0;
	Cards[1] = 0;
	Cards[2] = 0;

	int offered = 0;

	for (int tries = 0; tries < 40 && offered < 3; tries++) {
		int pick = Random_Pick(1, EFFECT_COUNT - 1);

		bool repeated = false;
		for (int other = 0; other < offered; other++) {
			if (Cards[other] == pick) {
				repeated = true;
			}
		}
		if (!repeated) {
			Cards[offered++] = pick;
		}
	}

	if (offered == 0) {
		return;
	}

	Phase = PHASE_CARD;
	DebugString("Survivors: level %d reached, %d cards offered\n", Level, offered);
}


/// <summary>Takes the card the player clicked.</summary>
void SurvivorsClass::Apply_Card(int index)
{
	if (index < 0 || index > 2) {
		return;
	}

	int effect = Cards[index];
	if (effect <= 0 || effect >= EFFECT_COUNT) {
		return;
	}

	CardPick = effect;

	switch (effect) {
		case EFFECT_REINFORCE:
			Add_To_Squad(2);
			break;

		case EFFECT_HEAL:
		case EFFECT_SHIELD:
			Mend();
			break;

		case EFFECT_CRYSTAL:
			CrystalWorth += 50;
			break;

		case EFFECT_MAGNET:
			MagnetRadius += 5;
			break;

		case EFFECT_PULL:
			PullStep++;
			break;

		case EFFECT_BREATHER:
			if (WaveScale > 60) WaveScale -= 15;
			break;

		case EFFECT_REVIVE:
			ReviveHeld = true;
			break;

		default:
			break;
	}

	Level++;
	Xp -= XpNeeded;
	if (Xp < 0) Xp = 0;
	XpNeeded = 8 + 5 * Level;

	Phase = PHASE_WAVE;
	if (WaveTimer <= 0) {
		WaveTimer = TICKS_PER_SECOND;
	}

	DebugString("Survivors: card '%s' taken at level %d\n", CARDS[effect].Name, Level);
}


/// <summary>
/// Opens the way out. The hero has to cross the map to it while the enemy keeps coming.
/// </summary>
void SurvivorsClass::Start_Evacuation(void)
{
	Cell here = Hero_Cell();

	EvacCell = Cell(Map.PlayRect.X + Map.PlayRect.Width / 2, Map.PlayRect.Y + Map.PlayRect.Height / 2);

	for (int tries = 0; tries < 60; tries++) {
		Cell spot = Cell(Map.PlayRect.X + Random_Pick(2, Map.PlayRect.Width - 2),
			Map.PlayRect.Y + Random_Pick(2, Map.PlayRect.Height - 2));

		if (!Map.In_Local_Radar(spot)) {
			continue;
		}

		int dx = spot.X - here.X;
		int dy = spot.Y - here.Y;
		if (dx * dx + dy * dy < 20 * 20) {
			continue;
		}

		EvacCell = spot;
		break;
	}

	if (AutoPick) {
		EvacCell = here;
	}

	Phase = PHASE_EVAC;
	EvacTimer = EVAC_TIME * TICKS_PER_SECOND;

	DebugString("Survivors: the way out is open at %d,%d\n", EvacCell.X, EvacCell.Y);
}


/// <summary>
/// Sets a bonus task on the map: a piece of ground the hero has to hold for a while. Holding it
/// to the end hands him a level, which is the reward the card table then turns into a choice.
/// </summary>
void SurvivorsClass::Start_Task(void)
{
	Cell here = Hero_Cell();

	MagnetRadius = 12;
	PhaseTimer = 0;
	PullStep = 1;
	TaskActive = false;
	TaskCell = CELL_NONE;
	TaskHeld = 0;
	TaskAway = 0;
	TaskLeft = 45 * TICKS_PER_SECOND;

	for (int tries = 0; tries < 60; tries++) {
		Cell spot = Cell(Map.PlayRect.X + Random_Pick(4, Map.PlayRect.Width - 4),
			Map.PlayRect.Y + Random_Pick(4, Map.PlayRect.Height - 4));

		if (!Map.In_Local_Radar(spot)) {
			continue;
		}

		int dx = spot.X - here.X;
		int dy = spot.Y - here.Y;
		int distance = dx * dx + dy * dy;
		if (distance < 10 * 10 || distance > 30 * 30) {
			continue;
		}

		TaskCell = spot;
		TaskActive = true;
		break;
	}

	if (!TaskActive) {
		TaskCell = Cell(here.X + 8, here.Y + 8);
		TaskActive = true;
	}

	/*
	 * **	A run started by itself cannot walk to the ground and hold it, so the task is put where
	 *	the hero stands and made short: the paying path is walked by the tests as well.
	 */
	if (AutoPick) {
		TaskCell = here;
		TaskLeft = 3 * TICKS_PER_SECOND;
	}

	DebugString("Survivors: a task stands at %d,%d\n", TaskCell.X, TaskCell.Y);
}


/// <summary>Watches the task: time inside the ground counts, time away for too long loses it.</summary>
void SurvivorsClass::Watch_Task(void)
{
	if (!TaskActive || Hero == NULL || !Hero_Alive()) {
		return;
	}

	Cell here = Hero->Center_Coord().As_Cell();
	int dx = here.X - TaskCell.X;
	int dy = here.Y - TaskCell.Y;
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;

	if (dx <= 3 && dy <= 3) {
		TaskHeld++;
		TaskAway = 0;

		if (TaskHeld >= TaskLeft) {
			TaskActive = false;
			Xp += XpNeeded;
			DebugString("Survivors: the task was held, the level is paid\n");
			Open_Cards();
		}
	} else {
		if (++TaskAway > 5 * TICKS_PER_SECOND) {
			TaskActive = false;
			DebugString("Survivors: the task was left for too long\n");
		}
	}
}


/// <summary>Draws where the task ground is and how much of it has been held.</summary>
void SurvivorsClass::Draw_Task(Surface & surface)
{
	if (!TaskActive) {
		return;
	}

	Rect whole(0, 0, surface.Get_Width(), surface.Get_Height());
	char text[192];

	Cell here = Hero_Cell();
	int dx = TaskCell.X - here.X;
	int dy = TaskCell.Y - here.Y;
	int seconds = (TaskLeft - TaskHeld) / TICKS_PER_SECOND;
	if (seconds < 0) seconds = 0;

	snprintf(text, sizeof(text), "Задание: удержать %d,%d  (от вас %d,%d)  %d с",
		TaskCell.X, TaskCell.Y, dx, dy, seconds);

	Fancy_Text_Print(text, surface, whole, Point2D(FrameW / 2, FrameH / 3),
		Fetch_Scheme_By_Name("Green"), TBLACK,
		TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	if (TaskAway > TICKS_PER_SECOND) {
		snprintf(text, sizeof(text), "Вернитесь в зону: %d", 5 - TaskAway / TICKS_PER_SECOND);
		Fancy_Text_Print(text, surface, whole, Point2D(FrameW / 2, FrameH / 3 + FrameH / 24),
			Fetch_Scheme_By_Name("LightGrey"), TBLACK,
			TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));
	}
}
/// <summary>Ends the run and says why.</summary>
void SurvivorsClass::Finish(char const * reason)
{
	Phase = PHASE_OVER;

	DebugString("Survivors: the run is over (%s) at wave %d, %d kills, %d crystals\n",
		reason, Wave, Kills, Crystals);

	if (PlayerPtr != NULL) {
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
		if (UseMapHero || AutoPick) {
			Begin();
		} else {
			Phase = PHASE_SIDE;
			PhaseTimer = 2 * TICKS_PER_SECOND;
			DebugString("Survivors: waiting for the side to be picked\n");
		}
		return;
	}

	/*
	 * **	A run that was started by itself takes one of the offered cards by itself too: there is
	 *	nobody at the table to click one.
	 */
	if (Phase == PHASE_CARD) {
		if (AutoPick && ++CardTimer > 2 * TICKS_PER_SECOND) {
			CardTimer = 0;
			for (int index = 0; index < 3; index++) {
				if (Cards[index] > 0) {
					Apply_Card(index);
					break;
				}
			}
		}
		return;
	}

	/*
	 * **	While the player is still choosing, there is no hero to watch over: the run has not
	 *	begun and the checks below have nothing to look at.
	 */
	if (Phase != PHASE_WAVE && Phase != PHASE_EVAC) {
		if (PhaseTimer > 0) PhaseTimer--;
		return;
	}

	if (Hero == NULL) {
		Finish("hero");
		return;
	}

	if (!Hero_Alive()) {
		if (GraceTimer > 0) {
			GraceTimer--;
			return;
		}

		/*
		 * **	A hero holding a second chance is stood up again where he fell.
		 */
		if (ReviveHeld) {
			ReviveHeld = false;
			Revives++;
			Cell here = Hero_Cell();
			InfantryTypeClass const * type = Pick_Infantry(PlayerPtr, HeroName, true);
			Hero = Stand_Infantry(PlayerPtr, type, here);
			if (Hero != NULL) {
				DebugString("Survivors: the hero is up again at %d,%d\n", here.X, here.Y);
				return;
			}
		}

		DebugString("Survivors: the hero is gone from the field\n");
		Hero = NULL;
		Finish("hero");
		return;
	}

	if (--FoeTimer <= 0) {
		FoeTimer = TICKS_PER_SECOND / 4;
		Watch_Foes();
	}

	Collect_Crystals();
	Watch_Task();

	Cell here = Hero->Center_Coord().As_Cell();

	if (Phase == PHASE_EVAC) {
		int dx = here.X - EvacCell.X;
		int dy = here.Y - EvacCell.Y;
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;

		if (dx <= 2 && dy <= 2) {
			DebugString("Survivors: the hero reached the way out\n");
			Phase = PHASE_OVER;
			if (PlayerPtr != NULL) {
				PlayerPtr->Flag_To_Win();
			}
			return;
		}

		if (--EvacTimer <= 0) {
			Finish("time");
			return;
		}

		/*
		 * **	The way out is never quiet: small groups keep coming while the hero runs for it.
		 */
		if (--WaveTimer <= 0) {
			WaveTimer = 4 * TICKS_PER_SECOND;
			for (int index = 0; index < 3; index++) {
				Spawn_Enemy(Enemy, here, Random_Pick(6, 10));
			}
		}
	} else if (Phase == PHASE_WAVE) {
		if (--WaveTimer <= 0) {
			Spawn_Wave();
		}
	}

	if (--SquadTimer <= 0) {
		SquadTimer = TICKS_PER_SECOND;
		Keep_Squad_Together();

		/*
		 * **	A run started by itself has nobody driving the hero, so he goes for the nearest man
		 *	of the enemy on his own. This is what makes the mode testable without a player.
		 */
		/*
		**	A run started by itself keeps the hero standing so that the whole of the loop, the
		**	gathering of tiberium included, can be watched through to its end.
		*/
		if (AutoPick && Hero != NULL && Hero_Alive()) {
			Hero->Strength = Hero->Techno_Type_Class()->MaxStrength;
		}

		if (AutoPick && Hero != NULL && Hero_Alive()) {
			/*
			**	The hero of a run started by itself holds his ground: the waves come to him, which is
			**	what lets the gathering of tiberium be watched without a player at the keyboard.
			*/
			Hero->Strength = Hero->Techno_Type_Class()->MaxStrength;
			Hero->Override_Mission(MISSION_GUARD, NULL, NULL);
		}

		if (++StatusTimer >= 5) {
			StatusTimer = 0;
			DebugString("Survivors: state wave %d, level %d, xp %d/%d, crystals %d, kills %d, on the ground %d, nearest %d\n",
				Wave, Level, Xp, XpNeeded, Crystals, Kills, Pickups.Count(), Nearest_Crystal());
		}
	}

	if (--CameraTimer <= 0) {
		CameraTimer = 2 * TICKS_PER_SECOND;
		Follow_With_Camera();
	}
}


/// <summary>Where a card stands on the frame.</summary>
Rect SurvivorsClass::Card_Rect(int index, int count, int top, int height) const
{
	int width = FrameW / (count + 2);
	int gap = width / 6;
	if (gap < 8) gap = 8;

	int total = count * width + (count - 1) * gap;
	int left = (FrameW - total) / 2;
	if (left < 8) left = 8;

	return(Rect(left + index * (width + gap), top, width, height));
}


/// <summary>Draws the choice of sides, or of that side's heroes.</summary>
void SurvivorsClass::Draw_Choices(Surface & surface)
{
	Rect whole(0, 0, surface.Get_Width(), surface.Get_Height());
	char text[256];

	char const * title = (Phase == PHASE_SIDE) ? "Выбор стороны" : "Выбор героя";
	Fancy_Text_Print(title, surface, whole, Point2D(FrameW / 2, FrameH / 6), Fetch_Scheme_By_Name("Green"),
		TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	int count = (Phase == PHASE_SIDE) ? 3 : SIDE_HERO_COUNT[SideIndex];

	for (int index = 0; index < count; index++) {
		Rect card = Card_Rect(index, count, FrameH / 4, FrameH / 8);

		surface.Fill_Rect(card, 16);
		surface.Draw_Rect(card, 40);

		if (Phase == PHASE_SIDE) {
			Fancy_Text_Print(SIDE_NAMES[index], surface, whole,
				Point2D(card.X + card.Width / 2, card.Y + card.Height / 2), Fetch_Scheme_By_Name("White"),
				TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));
		} else {
			HeroInfo const & hero = SIDE_HEROES[SideIndex][index];

			Fancy_Text_Print(hero.Name, surface, whole,
				Point2D(card.X + card.Width / 2, card.Y + card.Height / 4), Fetch_Scheme_By_Name("White"),
				TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

			Fancy_Text_Print(hero.Skill, surface, whole,
				Point2D(card.X + card.Width / 2, card.Y + card.Height / 2), Fetch_Scheme_By_Name("LightGrey"),
				TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

			snprintf(text, sizeof(text), "отряд: %s", hero.Squad);
			Fancy_Text_Print(text, surface, whole,
				Point2D(card.X + card.Width / 2, card.Y + card.Height * 3 / 4), Fetch_Scheme_By_Name("LightGrey"),
				TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));
		}
	}

	Fancy_Text_Print("Нажмите на карточку", surface, whole, Point2D(FrameW / 2, FrameH * 3 / 4),
		Fetch_Scheme_By_Name("LightGrey"), TBLACK,
		TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));
}


/// <summary>Draws what the stage asks for, before the first wave comes.</summary>
void SurvivorsClass::Draw_Stage(Surface & surface)
{
	Rect whole(0, 0, surface.Get_Width(), surface.Get_Height());
	char text[256];

	Fancy_Text_Print(StageName, surface, whole, Point2D(FrameW / 2, FrameH / 6), Fetch_Scheme_By_Name("Green"),
		TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	Fancy_Text_Print(StageLine, surface, whole, Point2D(FrameW / 2, FrameH / 3), Fetch_Scheme_By_Name("White"),
		TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	snprintf(text, sizeof(text), "Герой: %s", SIDE_HEROES[SideIndex][HeroIndex].Name);
	Fancy_Text_Print(text, surface, whole, Point2D(FrameW / 2, FrameH * 2 / 5), Fetch_Scheme_By_Name("LightGrey"),
		TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	snprintf(text, sizeof(text), "Сторона: %s   Отряд: %d человек",
		SIDE_NAMES[SideIndex], SquadCount);
	Fancy_Text_Print(text, surface, whole, Point2D(FrameW / 2, FrameH * 2 / 5 + FrameH / 20), Fetch_Scheme_By_Name("LightGrey"),
		TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	Fancy_Text_Print("Нажмите, чтобы начать", surface, whole, Point2D(FrameW / 2, FrameH * 3 / 5),
		Fetch_Scheme_By_Name("Green"), TBLACK,
		TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));
}


/// <summary>Draws the three cards a level offers.</summary>
void SurvivorsClass::Draw_Cards(Surface & surface)
{
	Rect whole(0, 0, surface.Get_Width(), surface.Get_Height());

	Fancy_Text_Print("Уровень! Выберите улучшение", surface, whole,
		Point2D(FrameW / 2, FrameH / 6), Fetch_Scheme_By_Name("Green"), TBLACK,
		TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	for (int index = 0; index < 3; index++) {
		int effect = Cards[index];
		if (effect <= 0 || effect >= EFFECT_COUNT) {
			continue;
		}

		Rect card = Card_Rect(index, 3, FrameH / 3, FrameH / 4);

		surface.Fill_Rect(card, 20);
		surface.Draw_Rect(card, 60);

		Fancy_Text_Print(CARDS[effect].Name, surface, whole,
			Point2D(card.X + card.Width / 2, card.Y + card.Height / 3), Fetch_Scheme_By_Name("White"),
			TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

		Fancy_Text_Print(CARDS[effect].Line, surface, whole,
			Point2D(card.X + card.Width / 2, card.Y + card.Height * 2 / 3), Fetch_Scheme_By_Name("LightGrey"),
			TBLACK, TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));
	}
}


/// <summary>Draws where the way out is and how long is left to reach it.</summary>
void SurvivorsClass::Draw_Evacuation(Surface & surface)
{
	Rect whole(0, 0, surface.Get_Width(), surface.Get_Height());
	char text[192];

	int seconds = EvacTimer / TICKS_PER_SECOND;
	if (seconds < 0) seconds = 0;

	snprintf(text, sizeof(text), "ЭВАКУАЦИЯ  %d:%02d", seconds / 60, seconds % 60);

	Fancy_Text_Print(text, surface, whole, Point2D(FrameW / 2, FrameH / 3),
		Fetch_Scheme_By_Name("Green"), TBLACK,
		TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	Cell here = Hero_Cell();
	int dx = EvacCell.X - here.X;
	int dy = EvacCell.Y - here.Y;

	snprintf(text, sizeof(text), "Точка сбора %d,%d   от вас %d,%d", EvacCell.X, EvacCell.Y, dx, dy);

	Fancy_Text_Print(text, surface, whole, Point2D(FrameW / 2, FrameH / 3 + FrameH / 20),
		Fetch_Scheme_By_Name("White"), TBLACK,
		TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));
}


/// <summary>Draws the run's own line: the wave, its countdown, the level and the tiberium.</summary>
void SurvivorsClass::Draw_Run_Line(Surface & surface)
{
	Rect whole(0, 0, surface.Get_Width(), surface.Get_Height());
	char text[192];

	int seconds = WaveTimer / TICKS_PER_SECOND;
	if (seconds < 0) seconds = 0;

	snprintf(text, sizeof(text), "Волна %d   %d:%02d", Wave, seconds / 60, seconds % 60);
	Fancy_Text_Print(text, surface, whole,
		Point2D(FrameW / 2, 10), Fetch_Scheme_By_Name("Green"), TBLACK,
		TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	snprintf(text, sizeof(text), "Уровень %d   Тиберий %d   Убито %d   Отряд %d",
		Level, Crystals, Kills, Squad.Count());
	Fancy_Text_Print(text, surface, whole,
		Point2D(FrameW / 2, 22), Fetch_Scheme_By_Name("LightGrey"), TBLACK,
		TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));

	/*
	 * **	The bar under the line says how close the next level is.
	 */
	int bar_width = FrameW / 4;
	int filled = (XpNeeded > 0) ? (Xp * bar_width / XpNeeded) : 0;
	if (filled > bar_width) filled = bar_width;

	Rect bar(FrameW / 2 - bar_width / 2, 36, bar_width, 6);
	surface.Fill_Rect(bar, 24);
	if (filled > 0) {
		surface.Fill_Rect(Rect(bar.X, bar.Y, filled, bar.Height), 68);
	}
}


/// <summary>
/// Marks the tiberium lying on the ground. A crystal the hero has already caught is drawn at the
/// place the pull has brought it to, so the gathering is something the player can see.
/// </summary>
void SurvivorsClass::Draw_Crystals(Surface & surface)
{
	if (TacticalMap == NULL || Pickups.Count() == 0) {
		return;
	}

	int const gem = DSurface::Build_Hicolor_Pixel(0, 220, 60);
	int const spark = DSurface::Build_Hicolor_Pixel(180, 255, 180);

	for (int index = 0; index < Pickups.Count(); index++) {
		Point2D pixel;

		if (!TacticalMap->Coord_To_Pixel(Coord(Pickups[index].Where), pixel)) {
			continue;
		}

		Rect body(pixel.X - 3, pixel.Y - 3, 7, 7);
		surface.Fill_Rect(body, gem);

		Rect shine(pixel.X - 1, pixel.Y - 1, 3, 3);
		surface.Fill_Rect(shine, spark);
	}
}
/// <summary>Draws whatever the current phase needs on the frame.</summary>
void SurvivorsClass::Draw_On_Field(Surface & surface)
{
	if (Phase == PHASE_OFF) {
		return;
	}

	FrameW = surface.Get_Width();
	FrameH = surface.Get_Height();

	switch (Phase) {
		case PHASE_SIDE:
		case PHASE_HERO:
			Draw_Choices(surface);
			break;

		case PHASE_STAGE:
			Draw_Stage(surface);
			break;

		case PHASE_CARD:
			Draw_Run_Line(surface);
			Draw_Cards(surface);
			break;

		default:
			Draw_Run_Line(surface);
			Draw_Crystals(surface);
			if (Phase == PHASE_EVAC) {
				Draw_Evacuation(surface);
			} else {
				Draw_Task(surface);
			}
	}
}


/// <summary>Takes a pick of a side or a hero.</summary>
bool SurvivorsClass::Choose(int index)
{
	if (Phase == PHASE_SIDE) {
		if (index < 0 || index > 2) {
			return(false);
		}
		SideIndex = index;
		HeroIndex = 0;
		Phase = PHASE_HERO;
		PhaseTimer = TICKS_PER_SECOND;
		DebugString("Survivors: side %s picked\n", SIDE_NAMES[SideIndex]);
		return(true);
	}

	if (Phase == PHASE_HERO) {
		if (index < 0 || index >= SIDE_HERO_COUNT[SideIndex]) {
			return(false);
		}
		HeroIndex = index;
		Phase = PHASE_STAGE;
		PhaseTimer = TICKS_PER_SECOND;
		DebugString("Survivors: hero %s picked\n", SIDE_HEROES[SideIndex][HeroIndex].Name);
		return(true);
	}

	return(false);
}


/// <summary>How far the closest tiberium on the ground is from the hero, in cells.</summary>
int SurvivorsClass::Nearest_Crystal(void) const
{
	if (Hero == NULL || Pickups.Count() == 0) {
		return(-1);
	}

	Cell here = Hero->Center_Coord().As_Cell();
	int best = -1;

	for (int index = 0; index < Pickups.Count(); index++) {
		int dx = Pickups[index].Where.X - here.X;
		int dy = Pickups[index].Where.Y - here.Y;
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;
		int step = dx > dy ? dx : dy;
		if (best < 0 || step < best) {
			best = step;
		}
	}

	return(best);
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
/// A click while the mode is choosing something belongs to the mode; a click while it is being
/// played belongs to the battlefield.
/// </summary>
bool SurvivorsClass::Click(Point2D const & screen)
{
	if (Phase == PHASE_OFF) {
		return(false);
	}

	if (Phase == PHASE_STAGE) {
		if (PhaseTimer > 0) return(true);
		Begin();
		return(true);
	}

	if (Phase == PHASE_CARD) {
		for (int index = 0; index < 3; index++) {
			Rect card = Card_Rect(index, 3, FrameH / 3, FrameH / 4);
			if (screen.X >= card.X && screen.X < card.X + card.Width
				&& screen.Y >= card.Y && screen.Y < card.Y + card.Height) {
				Apply_Card(index);
				return(true);
			}
		}
		return(true);
	}

	if (Phase != PHASE_SIDE && Phase != PHASE_HERO) {
		return(false);
	}

	if (PhaseTimer > 0) {
		return(true);
	}

	int count = (Phase == PHASE_SIDE) ? 3 : SIDE_HERO_COUNT[SideIndex];

	for (int index = 0; index < count; index++) {
		Rect card = Card_Rect(index, count, FrameH / 4, FrameH / 8);
		if (screen.X >= card.X && screen.X < card.X + card.Width
			&& screen.Y >= card.Y && screen.Y < card.Y + card.Height) {
			return(Choose(index));
		}
	}

	return(true);
}
