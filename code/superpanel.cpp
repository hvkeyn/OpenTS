/*******************************************************************************
 *                                O P E N T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "superpanel.h"

#include "ccfile.h"
#include "dbgprint.h"
#include "ccini.h"
#include "cell.h"
#include "ccrand.h"
#include "combat.h"
#include "empulse.h"
#include "conquer.h"
#include "crc.h"
#include "dialog.h"
#include "display.h"
#include "draw.h"
#include "dsurface.h"
#include "foot.h"
#include "globals.h"
#include "goptions.h"
#include "house.h"
#include "house.hh"
#include "infantry.h"
#include "infatype.h"
#include "building.h"
#include "builtype.h"
#include "aircraft.h"
#include "anim.h"
#include "animtype.h"
#include "airctype.h"
#include "houstype.h"
#include "rgb.h"
#include "rules.h"
#include "scheme.h"
#include "side.h"
#include "_convert.h"
#include "_mixfile.h"
#include "mixfile.h"
#include "_rect.h"
#include "map.h"
#include "overlay.h"
#include "_rules.h"
#include "overtype.h"
#include "scenario.h"
#include "savestream.h"
#include "sidebar.h"
#include "stimer.h"
#include "struct.hh"
#include "super.h"
#include "suprtype.h"
#include "surface.h"
#include "techno.h"
#include "unit.h"
#include "unittype.h"
#include "vox.h"
#include "warhead.h"

#include <cctype>
#include <cstdio>
#include <cstring>


SuperPanelClass SuperPanel;

static char const * const SUPERPOWERS_INI = "SUPERPOWERS.INI";
static char const * const PANEL_SECTION = "Panel";
static int const POOL_LIMIT = 48;			// how many abilities the file may offer in all

/*
** The squares are the size of a sidebar cameo, so that the icons of the game's own super
** weapons and the icons of units and buildings can be shown in them unchanged.
*/
static int const PANEL_CELL_WIDTH = SidebarClass::StripClass::OBJECT_WIDTH;
static int const PANEL_CELL_HEIGHT = SidebarClass::StripClass::OBJECT_HEIGHT;
static int const PANEL_GAP = 2;				// the space between two squares
static int const PANEL_COLUMNS = 2;			// squares across; the rest stack below
static int const PANEL_MARGIN = 6;			// the gap between the strip and the sidebar
static int const PANEL_CAPTION_HEIGHT = 11;	// the strip at the foot of a square holding its name

/*
** The shades the strip is painted in. It lands on the battlefield, which is a hicolor
** surface, so the colors have to be built as pixels of that surface and not taken from the
** palette the artwork uses.
*/
static int const PANEL_PLATE = DSurface::Build_Hicolor_Pixel(6, 6, 6);
static int const PANEL_CELL_BACK = DSurface::Build_Hicolor_Pixel(18, 18, 18);
static int const PANEL_FRAME_READY = DSurface::Build_Hicolor_Pixel(0, 190, 0);
static int const PANEL_FRAME_WAITING = DSurface::Build_Hicolor_Pixel(96, 96, 96);
static int const PANEL_FRAME_AIMING = DSurface::Build_Hicolor_Pixel(255, 220, 0);

/// The wash laid over the part of an icon that has not charged up yet.
static RGBClass const PANEL_SHADE(0, 0, 0);


/// <summary>
/// Turns what an INI says about an icon into the shape itself. The description names where
/// the art comes from: a super weapon, a building, a unit, an infantry type or a shape in
/// the mix files. Anything that cannot be found falls back on the icon of the weapon the
/// square borrows, and failing that on the generic icon the game uses for the unknown.
/// </summary>
static ShapeSet const * Resolve_Cameo(char const * description, SuperWeaponType fallback)
{
	ShapeSet const * cameo = NULL;

	if (description != NULL && description[0] != '\0') {
		char buffer[64];
		strncpy(buffer, description, sizeof(buffer) - 1);
		buffer[sizeof(buffer) - 1] = '\0';

		char * name = strchr(buffer, ':');
		if (name != NULL) {
			*name++ = '\0';
			while (*name == ' ') name++;

			if (stricmp(buffer, "SUPER") == 0) {
				SuperWeaponType type = SuperPanelClass::Find_Weapon(SuperPanelAbilityClass::Weapon_From_Name(name));
				if (type != SUPER_NONE) {
					cameo = SuperWeaponTypes[type]->CameoData;
				}
			} else if (stricmp(buffer, "SHP") == 0) {
				cameo = (ShapeSet const *)MFCD::Retrieve(name);
			} else if (stricmp(buffer, "BUILDING") == 0) {
				StructType type = BuildingTypeClass::From_Name(name);
				if (type != STRUCT_NONE) cameo = (ShapeSet const *)BuildingTypes[type]->Get_Cameo_Data();
			} else if (stricmp(buffer, "UNIT") == 0) {
				UnitType type = UnitTypeClass::From_Name(name);
				if (type != UNIT_NONE) cameo = (ShapeSet const *)UnitTypes[type]->Get_Cameo_Data();
			} else if (stricmp(buffer, "INFANTRY") == 0) {
				InfantryType type = InfantryTypeClass::From_Name(name);
				if (type != INFANTRY_NONE) cameo = (ShapeSet const *)InfantryTypes[type]->Get_Cameo_Data();
			} else if (stricmp(buffer, "AIRCRAFT") == 0) {
				AircraftType type = AircraftTypeClass::From_Name(name);
				if (type != AIRCRAFT_NONE) cameo = (ShapeSet const *)AircraftTypes[type]->Get_Cameo_Data();
			}
		} else {
			// a bare name is the file name of a shape in the mix files
			cameo = (ShapeSet const *)MFCD::Retrieve(buffer);
		}
	}

	if (cameo == NULL && fallback != SUPER_NONE && fallback < SuperWeaponTypes.Count()) {
		cameo = SuperWeaponTypes[fallback]->CameoData;
	}

	if (cameo == NULL) {
		cameo = (ShapeSet const *)MFCD::Retrieve("XXICON.SHP");
	}

	DebugString("SuperPanel: cameo [%s] -> %s\n", description != NULL ? description : "",
		cameo != NULL ? "found" : "missing");

	return(cameo);
}


/// <summary>
/// Turns the name of a side into its value. Any house that belongs to a side may be named
/// in its place, so that rules names such as Nod or GDI work as well.
/// </summary>
static SideType Side_From_Name(char const * name)
{
	if (name == NULL || name[0] == '\0' || stricmp(name, "any") == 0) return(SIDE_NONE);

	if (stricmp(name, "GDI") == 0) return(SIDE_GDI);
	if (stricmp(name, "NOD") == 0) return(SIDE_NOD);
	if (stricmp(name, "Civilian") == 0) return(SIDE_CIVILIAN);
	if (stricmp(name, "Mutant") == 0) return(SIDE_MUTANT);

	HousesType type = HouseTypeClass::From_Name(name);
	if (type != HOUSE_NONE && HouseTypes[type] != NULL) {
		return(HouseTypes[type]->Side);
	}

	return(SIDE_NONE);
}


/// <summary>
/// The name of the map being played, without folder or extension and in capitals. That is
/// how a mission names its own ability set in SUPERPOWERS.INI.
/// </summary>
static void Map_Stem(char * buffer, int size)
{
	buffer[0] = '\0';
	if (size < 2 || Scen == NULL || Scen->ScenarioName[0] == '\0') return;

	char const * start = Scen->ScenarioName;
	for (char const * at = Scen->ScenarioName; *at != '\0'; at++) {
		if (*at == '\\' || *at == '/') start = at + 1;
	}

	int at = 0;
	while (start[at] != '\0' && start[at] != '.' && at < size - 1) {
		buffer[at] = (char)toupper((unsigned char)start[at]);
		at++;
	}
	buffer[at] = '\0';
}


/// <summary>
/// Creates an ability with everything switched off.
/// </summary>
SuperPanelAbilityClass::SuperPanelAbilityClass(void) :
	Type(ABILITY_NONE),
	Delivery(DELIVERY_AIR),
	Side(SIDE_NONE),
	Tier(2),
	Charge(240),
	Count(1),
	Tiberium(false),
	Weapon(SUPER_NONE),
	Cursor(SUPER_NONE),
	Cameo(NULL),
	Cooldown(0),
	Charges(0)
{
	Section[0] = '\0';
	Name[0] = '\0';
	Description[0] = '\0';
	Hint[0] = '\0';
	Role[0] = '\0';
	Units[0] = '\0';
}


/// <summary>
/// Turns the name of an ability into its type.
/// </summary>
SuperPanelAbilityClass::AbilityType SuperPanelAbilityClass::Type_From_Name(char const * name)
{
	if (name == NULL) return(ABILITY_NONE);

	if (stricmp(name, "DROP_PODS") == 0) return(ABILITY_DROP_PODS);
	if (stricmp(name, "ION_CANNON") == 0) return(ABILITY_ION_CANNON);
	if (stricmp(name, "MISSILE") == 0) return(ABILITY_MISSILE);
	if (stricmp(name, "CHEM_MISSILE") == 0) return(ABILITY_CHEM_MISSILE);
	if (stricmp(name, "HUNTER_SEEKER") == 0) return(ABILITY_HUNTER_SEEKER);
	if (stricmp(name, "EM_PULSE") == 0) return(ABILITY_EM_PULSE);
	if (stricmp(name, "FIRESTORM") == 0) return(ABILITY_FIRESTORM);
	if (stricmp(name, "AIR_REINFORCE") == 0) return(ABILITY_AIR_REINFORCE);
	if (stricmp(name, "UNIT_REINFORCE") == 0) return(ABILITY_UNIT_REINFORCE);
	if (stricmp(name, "TIBERIUM_SEED") == 0) return(ABILITY_TIBERIUM_SEED);
	if (stricmp(name, "ARMOR_BOOST") == 0) return(ABILITY_ARMOR_BOOST);
	if (stricmp(name, "RECON") == 0) return(ABILITY_RECON);
	if (stricmp(name, "BARRAGE") == 0) return(ABILITY_BARRAGE);
	return(ABILITY_NONE);
}


/// <summary>
/// Turns the name of a delivery method into its value.
/// </summary>
SuperPanelAbilityClass::DeliveryType SuperPanelAbilityClass::Delivery_From_Name(char const * name)
{
	if (name == NULL) return(DELIVERY_AIR);
	if (stricmp(name, "Underground") == 0) return(DELIVERY_UNDERGROUND);
	if (stricmp(name, "Orbital") == 0) return(DELIVERY_ORBITAL);
	return(DELIVERY_AIR);
}


/// <summary>
/// Turns the name of one of the game's super weapons into its value. The friendly names of
/// the panel are taken first, so that an INI can say ION_CANNON as well as IonCannon.
/// </summary>
SuperWeaponType SuperPanelAbilityClass::Weapon_From_Name(char const * name)
{
	if (name == NULL || name[0] == '\0' || stricmp(name, "none") == 0) {
		return(SUPER_NONE);
	}

	if (stricmp(name, "ION_CANNON") == 0) return(SUPER_ION_CANNON);
	if (stricmp(name, "MULTI_MISSILE") == 0) return(SUPER_MULTI_MISSILE);
	if (stricmp(name, "MISSILE") == 0) return(SUPER_MULTI_MISSILE);
	if (stricmp(name, "CHEM_MISSILE") == 0) return(SUPER_CHEM_MISSILE);
	if (stricmp(name, "EM_PULSE") == 0) return(SUPER_EM_PULSE);
	if (stricmp(name, "EMPULSE") == 0) return(SUPER_EM_PULSE);
	if (stricmp(name, "FIRESTORM") == 0) return(SUPER_FIRESTORM);
	if (stricmp(name, "HUNTER_SEEKER") == 0) return(SUPER_HUNTER_SEEKER);
	if (stricmp(name, "DROP_PODS") == 0) return(SUPER_DROP_PODS);
	if (stricmp(name, "DROP_POD") == 0) return(SUPER_DROP_PODS);

	return(Special_From_Name(name));
}


/// <summary>
/// Reads one ability out of an INI section.
/// </summary>
bool SuperPanelAbilityClass::Read_INI(CCINIClass const & ini, char const * section)
{
	char buffer[256];

	strncpy(Section, section, sizeof(Section) - 1);
	Section[sizeof(Section) - 1] = '\0';

	if (ini.Get_String(section, "Name", "", buffer, sizeof(buffer)) <= 0) {
		return(false);
	}
	strncpy(Name, buffer, sizeof(Name) - 1);
	Name[sizeof(Name) - 1] = '\0';

	ini.Get_String(section, "Type", "", buffer, sizeof(buffer));
	Type = Type_From_Name(buffer);
	if (Type == ABILITY_NONE) {
		return(false);
	}

	ini.Get_String(section, "Description", "", Description, sizeof(Description));
	ini.Get_String(section, "Hint", "", Hint, sizeof(Hint));
	ini.Get_String(section, "Role", "", Role, sizeof(Role));
	ini.Get_String(section, "Units", "", Units, sizeof(Units));

	ini.Get_String(section, "Delivery", "", buffer, sizeof(buffer));
	Delivery = Delivery_From_Name(buffer);

	Charge = ini.Get_Int(section, "Charge", Charge);
	Count = ini.Get_Int(section, "Count", Count);
	Tier = ini.Get_Int(section, "Tier", Tier);
	Tiberium = ini.Get_Bool(section, "Tiberium", Tiberium);

	// the weapon that carries out the effect, and the one whose target cursor is borrowed.
	// The rules list their weapons in an order of their own, so a behaviour named here is
	// looked up rather than taken for an index.
	ini.Get_String(section, "Weapon", "", buffer, sizeof(buffer));
	Weapon = SuperPanelClass::Find_Weapon(Weapon_From_Name(buffer));

	ini.Get_String(section, "Cursor", "", buffer, sizeof(buffer));
	Cursor = SuperPanelClass::Find_Weapon(Weapon_From_Name(buffer));
	if (Cursor == SUPER_NONE) {
		Cursor = Weapon;
	}

	// the icon: a super weapon, a unit or building type, or a shape in the mix files
	ini.Get_String(section, "Cameo", "", buffer, sizeof(buffer));
	Cameo = Resolve_Cameo(buffer, Cursor);

	ini.Get_String(section, "Side", "", buffer, sizeof(buffer));
	Side = Side_From_Name(buffer);

	return(true);
}


/// <summary>
/// Puts the ability back to the start of a mission.
/// </summary>
/// <summary>
/// Puts the ability back to the start of a mission.
/// An ability that has not been spent yet is ready to be called straight away; it is the
/// use itself that starts the charge over, not the mission.
/// </summary>
void SuperPanelAbilityClass::Reset(void)
{
	Cooldown = 0;
	Charges = Count;
}


bool SuperPanelAbilityClass::Is_Available(void) const
{
	return(Charges > 0);
}


bool SuperPanelAbilityClass::Is_Ready(void) const
{
	return(Is_Available() && Cooldown <= 0);
}


int SuperPanelAbilityClass::Seconds_Left(void) const
{
	if (Cooldown <= 0) return(0);
	return((Cooldown + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND);
}


int SuperPanelAbilityClass::Charge_Percent(void) const
{
	if (Is_Ready()) return(100);

	int const total = Charge * TICKS_PER_SECOND;
	if (total <= 0) return(100);

	int const done = total - Cooldown;
	if (done <= 0) return(0);
	if (done >= total) return(100);

	return((done * 100) / total);
}


void SuperPanelAbilityClass::Charge_Up(void)
{
	if (Cooldown > 0) {
		Cooldown--;
	}
}


/// <summary>
/// Creates an ability panel that holds nothing yet.
/// </summary>
SuperPanelClass::SuperPanelClass(void) :
	SlotCount(0),
	TestAll(false),
	Pending(-1),
	PoolCount(0),
	SlotLimit(MAX_SLOTS),
	Named(false),
	NamedCount(0),
	PresentMask(0),
	RefreshTimer(0)
{
	Stem[0] = '\0';
}


void SuperPanelClass::One_Time(void)
{
}


/// <summary>
/// Fetches the game's weapon that performs the behaviour named, or SUPER_NONE when the
/// rules do not offer it. This game is the one that decides the order of its own weapons,
/// so a behaviour is looked up by name rather than taken for an index.
/// </summary>
SuperWeaponType SuperPanelClass::Find_Weapon(SuperWeaponType behaviour)
{
	if (behaviour == SUPER_NONE) return(SUPER_NONE);

	for (int index = SUPER_FIRST; index < SuperWeaponTypes.Count(); index++) {
		if (SuperWeaponTypes[index] != NULL && SuperWeaponTypes[index]->Type == behaviour) {
			return((SuperWeaponType)index);
		}
	}

	return(SUPER_NONE);
}


/// <summary>
/// The side the player is fighting for. It is what decides which abilities the panel may
/// offer the player.
/// </summary>
SideType SuperPanelClass::Panel_Side(void)
{
	if (PlayerPtr != NULL && PlayerPtr->Class != NULL) {
		return(PlayerPtr->Class->Side);
	}

	return(SIDE_NONE);
}


/// <summary>
/// How far into its campaign the mission being played is, taken from the number in its
/// name: GDI1A, FSNOD04 and the like. A mission without a number counts as a middling one.
/// </summary>
int SuperPanelClass::Mission_Tier(char const * map_name)
{
	int number = -1;

	if (map_name != NULL) {
		for (char const * at = map_name; *at != '\0'; at++) {
			if (*at >= '0' && *at <= '9') {
				number = (number < 0 ? 0 : number * 10) + (*at - '0');
			} else if (number >= 0) {
				break;
			}
		}
	}

	if (number < 0) return(2);
	if (number <= 2) return(1);
	if (number <= 4) return(2);
	if (number <= 7) return(3);

	return(4);
}


/// <summary>
/// Reads the whole pool of abilities out of SUPERPOWERS.INI and picks the set this mission
/// hands to the player.
///
/// The pool is every section [Panel] lists. A mission takes the set its own
/// [Mission.&lt;map&gt;] section names when the file has one, and otherwise the abilities that
/// suit the side the player fights for and are unlocked at the stage the mission has reached.
/// A house that belongs to a side of its own - the campaigns that other games bring along -
/// falls back on the head of the pool, so that its panel is never empty.
///
/// Whatever the panel ends up with, the mission's own [SuperPowers] section may still
/// replace or add to it.
/// </summary>
void SuperPanelClass::Read_INI(CCINIClass const & mission_ini)
{
	char section[64];
	CCINIClass ini;

	SlotCount = 0;
	Pending = -1;
	PoolCount = 0;
	Named = false;
	NamedCount = 0;
	PresentMask = 0;
	RefreshTimer = 0;

	CCFileClass file(SUPERPOWERS_INI);
	if (!file.Is_Available()) {
		return;
	}

	ini.Load(file, false);

	TestAll = ini.Get_Bool(PANEL_SECTION, "TestAll", false);

	SlotLimit = ini.Get_Int(PANEL_SECTION, "Slots", MAX_SLOTS);
	if (SlotLimit < 0) SlotLimit = 0;
	if (SlotLimit > MAX_SLOTS) SlotLimit = MAX_SLOTS;

	// every entry of [Panel] names a section holding one ability of the pool
	int entries = ini.Entry_Count(PANEL_SECTION);
	for (int index = 0; index < entries && PoolCount < POOL_LIMIT; index++) {
		char const * entry = ini.Get_Entry(PANEL_SECTION, index);
		if (entry == NULL) continue;
		if (stricmp(entry, "Slots") == 0 || stricmp(entry, "TestAll") == 0) continue;

		ini.Get_String(PANEL_SECTION, entry, "", section, sizeof(section));
		if (section[0] == '\0') continue;

		if (Pool[PoolCount].Read_INI(ini, section)) {
			PoolCount++;
		}
	}

	Map_Stem(Stem, sizeof(Stem));

	if (TestAll) {

		// the test set: the head of the pool, whatever side the abilities belong to
		for (int index = 0; index < PoolCount && SlotCount < SlotLimit; index++) {
			Slots[SlotCount++] = Pool[index];
		}

	} else {

		char key[96];
		snprintf(key, sizeof(key), "Mission.%s", Stem);

		if (Stem[0] != '\0' && ini.Entry_Count(key) > 0) {

			// the mission names its own set, in the order the player will see it in
			int wanted = ini.Entry_Count(key);
			for (int index = 0; index < wanted && NamedCount < MAX_SLOTS; index++) {
				char const * entry = ini.Get_Entry(key, index);
				if (entry == NULL) continue;

				ini.Get_String(key, entry, "", section, sizeof(section));
				if (section[0] == '\0') continue;

				int found = -1;
				for (int slot = 0; slot < PoolCount; slot++) {
					if (stricmp(Pool[slot].Section, section) == 0) {
						found = slot;
						break;
					}
				}

				if (found >= 0) {
					NamedSlots[NamedCount++] = Pool[found];
					Named = true;
				} else {
					// a section of the mission's own, written for it alone
					SuperPanelAbilityClass ability;
					if (ability.Read_INI(ini, section)) {
						NamedSlots[NamedCount++] = ability;
						Named = true;
					}
				}
			}
		}

		Choose();
	}

	PresentMask = Present_Mask();

	{
		char list[512];
		list[0] = '\0';

		for (int index = 0; index < SlotCount; index++) {
			snprintf(list + strlen(list), sizeof(list) - strlen(list),
				"%s%s", index ? ", " : "", Slots[index].Name);
		}

		DebugString("SuperPanel: %d abilities for side %d at %s: %s\n",
			SlotCount, (int)Panel_Side(), Stem, list);
	}

	// a mission may tune what it was given or add its own abilities
	int mission_entries = mission_ini.Entry_Count("SuperPowers");
	for (int index = 0; index < mission_entries; index++) {
		char const * entry = mission_ini.Get_Entry("SuperPowers", index);
		if (entry == NULL) continue;

		mission_ini.Get_String("SuperPowers", entry, "", section, sizeof(section));
		if (section[0] == '\0') continue;

		SuperPanelAbilityClass ability;
		if (!ability.Read_INI(mission_ini, section)) continue;

		bool replaced = false;
		for (int slot = 0; slot < SlotCount; slot++) {
			if (stricmp(Slots[slot].Name, ability.Name) == 0) {
				int cooldown = Slots[slot].Cooldown;
				int charges = Slots[slot].Charges;
				Slots[slot] = ability;
				Slots[slot].Cooldown = cooldown;
				Slots[slot].Charges = charges;
				replaced = true;
				break;
			}
		}

		if (!replaced && SlotCount < MAX_SLOTS) {
			Slots[SlotCount++] = ability;
		}
	}
}


/// <summary>
/// Is this one of the powers the other side would normally hold, which the player can now
/// call on because the building that grants it has been taken over? The game knows, because
/// it hands such a weapon to whoever owns the building that grants it.
/// </summary>
bool SuperPanelClass::Captured(SuperPanelAbilityClass const & ability) const
{
	return(ability.Weapon != SUPER_NONE
		&& PlayerPtr != NULL
		&& ability.Weapon < PlayerPtr->SuperWeapon.Count()
		&& PlayerPtr->SuperWeapon[ability.Weapon]->Is_Present());
}


/// <summary>
/// Works out which of the pooled abilities the player is offered. Called when the mission is
/// read and again whenever the powers the player holds change.
/// </summary>
void SuperPanelClass::Choose(void)
{
	SlotCount = 0;

	if (Named) {
		for (int index = 0; index < NamedCount && SlotCount < SlotLimit; index++) {
			Slots[SlotCount++] = NamedSlots[index];
		}
		return;
	}

	if (TestAll) {
		for (int index = 0; index < PoolCount && SlotCount < SlotLimit; index++) {
			Slots[SlotCount++] = Pool[index];
		}
		return;
	}

	SideType const side = Panel_Side();
	int const tier = Mission_Tier(Stem);

	// a mission that starts with nothing standing has no use for the powers that look after
	// a base
	bool const has_base = (PlayerPtr != NULL && PlayerPtr->CurBuildings > 0);

	SuperPanelAbilityClass candidate[POOL_LIMIT];
	int count = 0;

	for (int index = 0; index < PoolCount; index++) {
		bool const suits_side = (Pool[index].Side == SIDE_NONE || Pool[index].Side == side);

		// a power of the other side is offered once its building has been taken over
		if (!suits_side && !Captured(Pool[index])) continue;
		if (Pool[index].Tier > tier) continue;
		if (!has_base && Pool[index].Type == SuperPanelAbilityClass::ABILITY_ARMOR_BOOST) continue;

		int at = count++;
		while (at > 0 && candidate[at - 1].Tier < Pool[index].Tier) {
			candidate[at] = candidate[at - 1];
			at--;
		}
		candidate[at] = Pool[index];
	}

	// nothing suits this side: the head of the pool still gives the player something
	if (count == 0) {
		for (int index = 0; index < PoolCount; index++) {
			candidate[count++] = Pool[index];
		}
	}

	// the heaviest weapons win the places, then the set is laid out lightest first
	int const take = (count < SlotLimit) ? count : SlotLimit;
	for (int index = take - 1; index >= 0; index--) {
		Slots[SlotCount++] = candidate[index];
	}
}


/// <summary>
/// Which of the game's super weapons the player holds at the moment.
/// A weapon becomes held when the building that grants it is owned, so this is also a record
/// of which of the enemy's powers the player has taken over.
/// </summary>
int SuperPanelClass::Present_Mask(void)
{
	int mask = 0;

	if (PlayerPtr != NULL) {
		for (int index = 0; index < PlayerPtr->SuperWeapon.Count() && index < 31; index++) {
			if (PlayerPtr->SuperWeapon[index]->Is_Present()) {
				mask |= (1 << index);
			}
		}
	}

	return(mask);
}


/// <summary>
/// Goes over the set again while the mission runs.
/// Taking over a building that grants a super weapon - the enemy's temple, an uplink - gives
/// the player powers of theirs, and a set that was chosen when the mission was read would
/// never notice. The squares the player already has keep their charges.
/// </summary>
void SuperPanelClass::Refresh(void)
{
	if (TestAll || Named || PoolCount == 0) return;

	if (++RefreshTimer < TICKS_PER_SECOND * 5) return;
	RefreshTimer = 0;

	int const mask = Present_Mask();
	if (mask == PresentMask) return;

	PresentMask = mask;

	SuperPanelAbilityClass before[MAX_SLOTS];
	int const before_count = SlotCount;

	for (int index = 0; index < before_count; index++) {
		before[index] = Slots[index];
	}

	Choose();

	for (int index = 0; index < SlotCount; index++) {
		for (int old = 0; old < before_count; old++) {
			if (stricmp(Slots[index].Section, before[old].Section) == 0) {
				Slots[index].Cooldown = before[old].Cooldown;
				Slots[index].Charges = before[old].Charges;
				break;
			}
		}
	}

	{
		char list[512];
		list[0] = '\0';

		for (int index = 0; index < SlotCount; index++) {
			snprintf(list + strlen(list), sizeof(list) - strlen(list),
				"%s%s", index ? ", " : "", Slots[index].Name);
		}

		DebugString("SuperPanel: %d abilities after the player's own super weapons changed: %s\n",
			SlotCount, list);
	}
}


/// <summary>
/// Starts every ability off at the beginning of a mission.
/// </summary>
void SuperPanelClass::Reset(void)
{
	Pending = -1;

	for (int index = 0; index < SlotCount; index++) {
		Slots[index].Reset();

		// the test set hands every ability over at once, so that all of them can be tried
		if (TestAll) {
			Slots[index].Cooldown = 0;
			Slots[index].Charges = 99;
		}
	}
}


/// <summary>
/// Turns the charge of every square on by one frame. The panel is charged from the game's own
/// logic pass, so the timers run at the rate of the game and not at the rate the screen
/// happens to be drawn.
/// </summary>
void SuperPanelClass::Logic(void)
{
	// the set is looked at again now and then, for the powers the player may have taken
	// over from the other side
	Refresh();

	for (int index = 0; index < SlotCount; index++) {
		Slots[index].Charge_Up();
	}

	// the player may have given up the aim with the escape key, which the game handles itself
	if (Pending >= 0 && Map.IsTargettingMode == SUPER_NONE) {
		Pending = -1;
	}
}


int SuperPanelClass::Count(void) const
{
	return(SlotCount);
}


/// <summary>
/// The place the strip takes on the screen: a block of squares hung beside the radar, just
/// clear of the sidebar. Nothing that belongs to the sidebar, to the battlefield or to the
/// briefing screens is moved or covered by it.
/// </summary>
/// <returns>The screen rectangle of the whole strip.</returns>
Rect SuperPanelClass::Strip_Rect(void)
{
	int const rows = (MAX_SLOTS + PANEL_COLUMNS - 1) / PANEL_COLUMNS;
	int const width = PANEL_COLUMNS * PANEL_CELL_WIDTH + (PANEL_COLUMNS - 1) * PANEL_GAP;
	int const height = rows * PANEL_CELL_HEIGHT + (rows - 1) * PANEL_GAP;

	int left;
	if (Options.IsSidebarOnRight) {
		left = SidebarRect.X - PANEL_MARGIN - width;
	} else {
		left = SidebarRect.X + SidebarRect.Width + PANEL_MARGIN;
	}

	// the strip hangs beside the radar, so that the two of them line up
	Rect const radar = SidebarClass::Sidebar_Radar_Rect();
	int top = radar.Y + (radar.Height - height) / 2;
	if (top < TacticalRect.Y) {
		top = TacticalRect.Y;
	}

	return(Rect(left, top, width, height));
}


/// <summary>
/// One square of the strip. The squares fill a column at a time, so that a strip of six
/// abilities reads as two neat columns of three.
/// </summary>
Rect SuperPanelClass::Cell_Rect(int index, Rect const & strip) const
{
	int const rows = (MAX_SLOTS + PANEL_COLUMNS - 1) / PANEL_COLUMNS;
	int const column = index / rows;
	int const row = index % rows;

	return(Rect(strip.X + column * (PANEL_CELL_WIDTH + PANEL_GAP),
		strip.Y + row * (PANEL_CELL_HEIGHT + PANEL_GAP),
		PANEL_CELL_WIDTH,
		PANEL_CELL_HEIGHT));
}


static bool point_in_rect(Point2D const & point, Rect const & rect)
{
	return(point.X >= rect.X && point.X < rect.X + rect.Width
		&& point.Y >= rect.Y && point.Y < rect.Y + rect.Height);
}


/// <summary>
/// Puts one square into the aiming state: the game is told which of its super weapons lends
/// the square its target cursor, and the click that follows on the battlefield picks the
/// place the ability lands on.
/// </summary>
void SuperPanelClass::Aim_At(int index)
{
	Pending = index;

	// a mission set may name a weapon this game does not have, so the cursor is checked
	SuperWeaponType cursor = Slots[index].Cursor;
	if (cursor < SUPER_FIRST || cursor >= SuperWeaponTypes.Count()) {
		cursor = SUPER_FIRST;
	}
	Map.IsTargettingMode = cursor;

	Unselect_All();
	Speak(VOX_SELECT_TARGET);

	DebugString("SuperPanel: %s aiming, pick a target\n", Slots[index].Name);
}


/// <summary>
/// Takes a click on the strip. A ready square starts aiming; one that is still charging
/// says so and keeps the click, so that nothing behind the panel acts on it.
/// </summary>
/// <param name="screen">Where the mouse was, in screen coordinates.</param>
/// <returns>Was the click spent on the panel?</returns>
bool SuperPanelClass::Click(Point2D const & screen)
{
	if (SlotCount == 0) return(false);

	Rect const strip = Strip_Rect();

	for (int index = 0; index < SlotCount; index++) {
		if (!point_in_rect(screen, Cell_Rect(index, strip))) continue;

		if (Slots[index].Is_Ready()) {
			Aim_At(index);
		} else if (Slots[index].Is_Available()) {
			// the square is charging: the click is taken, but nothing happens until it is done
			DebugString("SuperPanel: %s is charging, %d seconds left\n",
				Slots[index].Name, Slots[index].Seconds_Left());
		} else {
			DebugString("SuperPanel: %s has been used up\n", Slots[index].Name);
		}

		return(true);
	}

	return(false);
}


/// <summary>
/// Takes the click that picks the place an aiming square lands on.
/// </summary>
/// <param name="screen">Where the mouse was, in screen coordinates.</param>
/// <returns>Was the click spent on the panel?</returns>
bool SuperPanelClass::Target(Point2D const & screen)
{
	if (Pending < 0) return(false);

	int const index = Pending;
	Pending = -1;
	Map.IsTargettingMode = SUPER_NONE;

	Cell cell = Map.Click_Cell_Calc(screen);
	if (cell != CELL_NONE) {
		Fire_Slot(index, cell);
	}

	return(true);
}


/// <summary>
/// Gives up the aim, which is what the right mouse button is for.
/// </summary>
/// <returns>Was there an aim to give up?</returns>
bool SuperPanelClass::Cancel(void)
{
	if (Pending < 0) return(false);

	Pending = -1;
	Map.IsTargettingMode = SUPER_NONE;

	DebugString("SuperPanel: aiming cancelled\n");

	return(true);
}


/// <summary>
/// Applies one ability at the cell given.
/// </summary>
bool SuperPanelClass::Fire_Slot(int index, Cell const & cell)
{
	SuperPanelAbilityClass & ability = Slots[index];
	bool done = false;

	if (ability.Weapon != SUPER_NONE) {

		/*
		**	The game's own super weapon carries the effect out for us: the ion blast, the
		**	missile rising from the map edge, the drop pods, the EMP pulse. The panel keeps the
		**	charge timer, so the weapon is handed over fully charged and given back afterwards,
		**	which is what lets a mission use it without the building that would grant it.
		*/
		if (PlayerPtr != NULL && ability.Weapon < PlayerPtr->SuperWeapon.Count()) {
			DebugString("SuperPanel: %s fired at %d,%d\n", ability.Name, cell.X, cell.Y);
			done = PlayerPtr->SuperWeapon[ability.Weapon]->Discharge_From_Panel(cell);
		}

	} else {

		DebugString("SuperPanel: %s fired at %d,%d\n", ability.Name, cell.X, cell.Y);

		switch (ability.Type) {
			case SuperPanelAbilityClass::ABILITY_DROP_PODS:
			case SuperPanelAbilityClass::ABILITY_AIR_REINFORCE:
				done = Place_Squad(ability, cell, true);
				break;

			case SuperPanelAbilityClass::ABILITY_UNIT_REINFORCE:
				done = Place_Squad(ability, cell, false);
				break;

			case SuperPanelAbilityClass::ABILITY_ION_CANNON:
				done = Damage_Area(cell, 2, 900, "HE", false);
				break;

			case SuperPanelAbilityClass::ABILITY_MISSILE:
				done = Damage_Area(cell, 3, 400, "HE", ability.Tiberium);
				break;

			case SuperPanelAbilityClass::ABILITY_CHEM_MISSILE:
				done = Damage_Area(cell, 3, 250, "Fire", true);
				break;

			case SuperPanelAbilityClass::ABILITY_FIRESTORM:
				done = Damage_Area(cell, 4, 200, "Fire", false);
				break;

			case SuperPanelAbilityClass::ABILITY_EM_PULSE:
				// a real electromagnetic pulse: everything mechanical in the area is left
				// standing dead for a while, sparks and all
				done = Pulse_Area(cell, 4, 10);
				break;

			case SuperPanelAbilityClass::ABILITY_HUNTER_SEEKER:
				// the drone comes in from the edge of the map and picks its own target, so the
				// place the player aimed at is where it starts looking
				done = Launch_Hunter_Seeker(cell);
				break;

			case SuperPanelAbilityClass::ABILITY_TIBERIUM_SEED:
				done = Place_Seed(cell, 3, "TIB01");
				break;

			case SuperPanelAbilityClass::ABILITY_RECON:
				done = Reveal_Area(cell, 7);
				break;

			case SuperPanelAbilityClass::ABILITY_BARRAGE:
				done = Barrage(cell, 8, 2, 200, "HE");
				break;

			case SuperPanelAbilityClass::ABILITY_ARMOR_BOOST:
				// the GloboTech technology: the owner's structures around the spot are repaired
				for (int y = -4; y <= 4; y++) {
					for (int x = -4; x <= 4; x++) {
						Cell spot(cell.X + x, cell.Y + y);
						if (!Map.In_Radar(spot)) continue;

						BuildingClass * building = Map[spot].Cell_Building();
						if (building != NULL && building->House == PlayerPtr) {
							building->Repair(200);
							done = true;
						}
					}
				}
				break;

			default:
				break;
		}
	}

	// the game's targeting mode is put out either way, so that a shot that did nothing does
	// not leave the player stuck with a target cursor
	Map.IsTargettingMode = SUPER_NONE;

	DebugString("SuperPanel: %s at %d,%d %s\n", ability.Name, cell.X, cell.Y,
		done ? "done" : "found nowhere to go");

	// the use is spent and the charge starts over whatever came of the strike. An ability
	// that put nothing on the ground - ground that was all taken, say - must not leave the
	// player able to call it again and again.
	ability.Charges--;
	ability.Cooldown = ability.Charge * TICKS_PER_SECOND;

	return(done);
}


/// <summary>
/// Lands a squad of the listed units around the cell given.
/// Infantry arrive by drop pod when the delivery is airborne; anything else is placed on the
/// ground next to the spot.
/// </summary>
/// <summary>
/// Puts one arrived unit down on the ground near the cell asked for.
/// The cell itself is tried first and then a ring of cells around it, so a squad that
/// lands on ground that is already taken spreads out instead of being thrown away, which
/// is what used to leave an ability with nothing to show for it.
/// </summary>
/// <returns>Was somewhere found for the unit to stand?</returns>
/// <summary>
/// Throws up the dust of an arrival from underground, so that a squad that tunnels up is
/// seen to come out of the ground rather than simply appearing on it.
/// </summary>
static void Dig_Out_Dust(Cell const & cell)
{
	AnimType type = AnimTypeClass::From_Name("S_TUMU30");
	if (type == ANIM_NONE) return;

	new AnimClass(AnimTypes[type], Coord(cell), Random_Pick(0, 6));
}


/// <summary>
/// Mark where a drop pod came down.
/// </summary>
static void Drop_Pod_Dust(Cell const & cell)
{
	if (Rule == NULL || Rule->DropPod.Count() == 0) return;

	new AnimClass(Rule->DropPod[Random_Pick(0, Rule->DropPod.Count() - 1)], Coord(cell));
}


/// <summary>
/// Sends one aircraft in over the spot asked for.
/// An aircraft does not stand about the way a squad does, so one that arrives is told to
/// hunt and flagged as a loaner, which is how the game sends home an aircraft it lent to a
/// mission.
/// </summary>
/// <returns>Was somewhere found for the aircraft to appear?</returns>
static bool Fly_Here(AircraftClass * craft, Cell const & cell)
{
	if (craft == NULL) return(false);

	craft->IsALoaner = true;

	for (int radius = 0; radius <= 6; radius++) {
		for (int y = -radius; y <= radius; y++) {
			for (int x = -radius; x <= radius; x++) {
				if (radius > 0 && abs(x) != radius && abs(y) != radius) continue;

				Cell spot(cell.X + x, cell.Y + y);
				if (!Map.In_Radar(spot)) continue;

				if (craft->Unlimbo(Coord(spot), DIR_N)) {
					craft->Look();
					craft->Assign_Mission(MISSION_HUNT);
					craft->Commence();
					return(true);
				}
			}
		}
	}

	return(false);
}


static bool Land_Here(FootClass * object, Cell const & cell, MissionType mission = MISSION_GUARD_AREA)
{
	for (int radius = 0; radius <= 5; radius++) {
		for (int y = -radius; y <= radius; y++) {
			for (int x = -radius; x <= radius; x++) {
				if (radius > 0 && abs(x) != radius && abs(y) != radius) continue;

				Cell spot(cell.X + x, cell.Y + y);
				if (!Map.In_Radar(spot)) continue;

				if (object->Unlimbo(Coord(spot), DIR_N)) {
					object->Look();
					object->Assign_Mission(mission);
					object->Commence();
					return(true);
				}
			}
		}
	}

	return(false);
}


/// <summary>
/// Does the player's house own this kind of unit at all?
/// A vehicle the house does not own would arrive as a stray rather than as a
/// reinforcement, so it is passed over in favour of one that belongs to the player.
/// </summary>
static bool House_Owns(TechnoTypeClass const * type)
{
	return(PlayerPtr != NULL && type != NULL && PlayerPtr->Can_Build(type, false, false) != -1);
}


bool SuperPanelClass::Place_Squad(SuperPanelAbilityClass const & ability, Cell const & cell, bool airborne)
{
	int placed = 0;
	char list[256];

	strncpy(list, ability.Units, sizeof(list) - 1);
	list[sizeof(list) - 1] = '\0';

	if (list[0] == '\0') {
		strcpy(list, "E1,E1,E2");
	}

	DebugString("SuperPanel: %s is bringing in %s\n", ability.Name, list);

	char * token = strtok(list, ",");
	while (token != NULL) {
		while (*token == ' ' || *token == '\t') token++;
		char * end = token + strlen(token);
		while (end > token && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';

		if (*token == '\0') {
			token = strtok(NULL, ",");
			continue;
		}

		InfantryType itype = InfantryTypeClass::From_Name(token);
		if (itype != INFANTRY_NONE) {
			InfantryClass * inf = (InfantryClass *)InfantryTypes[itype]->Create_One_Of(PlayerPtr);
			if (inf != NULL) {
				inf->Veterancy.Set_Elite(true);
				Cell landed = cell;
				if (Land_Here(inf, cell)) {
					landed = inf->PositionCoord.As_Cell();
					if (ability.Delivery == SuperPanelAbilityClass::DELIVERY_UNDERGROUND) {
						Dig_Out_Dust(landed);
					} else {
						Drop_Pod_Dust(landed);
					}
					placed++;
				} else {
					delete inf;
				}
			}
		} else {
			AircraftType atype = AircraftTypeClass::From_Name(token);

			if (atype != AIRCRAFT_NONE && House_Owns(AircraftTypes[atype])) {

				// an aircraft is flown in rather than dropped off
				ScenarioInit++;
				AircraftClass * craft = new AircraftClass(AircraftTypes[atype], PlayerPtr);
				ScenarioInit--;

				if (craft != NULL && Fly_Here(craft, cell)) {
					placed++;
				} else {
					delete craft;
				}

				token = strtok(NULL, ",");
				continue;
			}

			UnitType utype = UnitTypeClass::From_Name(token);
			if (utype != UNIT_NONE && House_Owns(UnitTypes[utype])) {
				UnitClass * unit = new UnitClass(UnitTypes[utype], PlayerPtr);
				if (unit != NULL && Land_Here(unit, cell)) {
					Cell landed = unit->PositionCoord.As_Cell();
					if (ability.Delivery == SuperPanelAbilityClass::DELIVERY_UNDERGROUND) {
						Dig_Out_Dust(landed);
					} else {
						Drop_Pod_Dust(landed);
					}
					placed++;
				} else {
					delete unit;
				}
			} else {
				DebugString("SuperPanel: %s names nothing this game knows: %s\n", ability.Name, token);
			}
		}

		token = strtok(NULL, ",");
	}

	return(placed > 0);
}


/// <summary>
/// Leaves a patch of an overlay - tiberium, usually - around the cell given.
/// </summary>
bool SuperPanelClass::Place_Seed(Cell const & cell, int radius, char const * overlay_name)
{
	OverlayType otype = OverlayTypeClass::From_Name(overlay_name);
	if (otype == OVERLAY_NONE) {
		return(false);
	}

	int placed = 0;
	for (int y = -radius; y <= radius; y++) {
		for (int x = -radius; x <= radius; x++) {
			if ((x * x + y * y) > radius * radius) continue;

			Cell spot(cell.X + x, cell.Y + y);
			if (!Map.In_Radar(spot)) continue;
			if (Map[spot].Overlay != OVERLAY_NONE) continue;

			new OverlayClass(OverlayTypes[otype], spot, HOUSE_NONE);
			placed++;
		}
	}

	return(placed > 0);
}


/// <summary>
/// Sets off an electromagnetic pulse.
/// Everything mechanical inside the circle is paralysed for the length of time asked for,
/// which is what the pulse cannon of the game does when it fires.
/// </summary>
/// <param name="cell">The middle of the pulse.</param>
/// <param name="spread">The radius of the pulse, in cells.</param>
/// <param name="seconds">How long the machinery stays dead.</param>
/// <returns>Was a pulse set off?</returns>
bool SuperPanelClass::Pulse_Area(Cell const & cell, int spread, int seconds)
{
	if (spread <= 0 || seconds <= 0) return(false);

	new EMPulseClass(cell, spread, seconds * TICKS_PER_SECOND, NULL);

	return(true);
}


/// <summary>
/// Sends Nod's homing drone in from the edge of the map.
/// The drone finds a target of its own once it is up, which is what it is for; the
/// building that normally launches it is not needed.
/// </summary>
/// <returns>Was a drone launched?</returns>
bool SuperPanelClass::Launch_Hunter_Seeker(Cell const & target)
{
	if (PlayerPtr == NULL) return(false);

	SideClass const * side = PlayerPtr->Acted_Side();
	if (side == NULL || side->HunterSeeker == NULL) {
		DebugString("SuperPanel: this side has no hunter seeker\n");
		return(false);
	}

	// the drone is sent in over the edge of the map, and failing that from where the
	// player aimed, so that the power is never spent on an arrival that has no room
	Cell entries[3];
	entries[0] = Map.Calculated_Cell(PlayerPtr->Control.Edge, CELL_NONE, CELL_NONE, SPEED_WINGED);
	entries[1] = Map.Calculated_Cell(SOURCE_NORTH, CELL_NONE, CELL_NONE, SPEED_WINGED);
	entries[2] = target;

	UnitClass * drone = new UnitClass(side->HunterSeeker, PlayerPtr);
	if (drone == NULL) return(false);

	for (int which = 0; which < 3; which++) {
		if (entries[which] == CELL_NONE) continue;

		if (Land_Here(drone, entries[which], MISSION_ATTACK)) {
			drone->Locomotion->Acquire_Hunter_Seeker_Target();
			drone->Commence();
			DebugString("SuperPanel: hunter seeker sent in at %d,%d\n",
				entries[which].X, entries[which].Y);
			return(true);
		}
	}

	delete drone;
	DebugString("SuperPanel: nowhere to send the hunter seeker in from\n");

	return(false);
}


/// <summary>
/// Lifts the shroud from a circle of the map, which is what a scout buys the player.
/// </summary>
/// <returns>Was any ground uncovered?</returns>
bool SuperPanelClass::Reveal_Area(Cell const & cell, int radius)
{
	int revealed = 0;

	for (int y = -radius; y <= radius; y++) {
		for (int x = -radius; x <= radius; x++) {
			if ((x * x + y * y) > radius * radius) continue;

			Cell spot(cell.X + x, cell.Y + y);
			if (!Map.In_Radar(spot)) continue;

			Map.Map_Cell(spot, PlayerPtr);
			revealed++;
		}
	}

	return(revealed > 0);
}


/// <summary>
/// Brings a battery of guns down on an area.
/// Several shells land around the spot the player picked rather than one blast, which is
/// what a barrage looks and feels like from above.
/// </summary>
/// <returns>Was the barrage fired?</returns>
bool SuperPanelClass::Barrage(Cell const & cell, int shells, int radius, int strength, char const * warhead_name)
{
	WarheadTypeClass const * warhead = WarheadTypeClass::From_Name(warhead_name);

	if (shells <= 0) return(false);

	for (int shell = 0; shell < shells; shell++) {
		Cell spot(cell.X + Random_Pick(-radius, radius), cell.Y + Random_Pick(-radius, radius));
		if (!Map.In_Radar(spot)) {
			spot = cell;
		}

		Explosion_Damage(Map[spot].Cell_Coord(), strength, NULL, warhead, true);
	}

	return(true);
}


/// <summary>
/// Wrecks an area around the cell given, optionally seeding it with tiberium afterwards.
/// </summary>
bool SuperPanelClass::Damage_Area(Cell const & cell, int radius, int strength, char const * warhead_name, bool tiberium)
{
	WarheadTypeClass const * warhead = WarheadTypeClass::From_Name(warhead_name);

	Explosion_Damage(Map[cell].Cell_Coord(), strength, NULL, warhead, true);

	if (tiberium) {
		Place_Seed(cell, radius, "TIB01");
	}

	return(true);
}


/// <summary>
/// Draws the strip onto the battlefield itself, which is the surface the finished frame is
/// put together on. The strip only shows while a mission is being played, so the menus and
/// the briefing dialogs are left alone, and the buttons are drawn after it, which hides it
/// for as long as a dialog is open.
/// </summary>
void SuperPanelClass::Draw_On_Field(Surface & surface)
{
	if (SlotCount == 0 || !ScenarioActive || !Map.IsSidebarActive) return;

	Rect strip = Strip_Rect();

	// the battlefield surface has its own origin, which is not the corner of the screen
	strip.X -= TacticalRect.X;

	Draw(surface, strip);
}


/// <summary>
/// Draws the strip: one square per ability, carrying its icon, its name, how far it has
/// charged and how many uses it has left. The icons are the game's own cameos, drawn in the
/// palette the sidebar draws its cameos with, so an ion cannon looks like the ion cannon the
/// player already knows.
/// </summary>
void SuperPanelClass::Draw(Surface & surface, Rect const & strip)
{
	if (SlotCount == 0) return;

	// a plate behind the squares, so that the strip stands apart from the battlefield
	Rect plate(strip.X - 3, strip.Y - 3, strip.Width + 6, strip.Height + 6);
	surface.Fill_Rect(plate, PANEL_PLATE);
	surface.Draw_Rect(plate, PANEL_FRAME_WAITING);

	for (int index = 0; index < SlotCount; index++) {
		SuperPanelAbilityClass const & ability = Slots[index];
		Rect cell = Cell_Rect(index, strip);
		bool const ready = ability.Is_Ready();
		bool const aiming = (Pending == index);

		surface.Fill_Rect(cell, PANEL_CELL_BACK);

		// the icon
		if (ability.Cameo != NULL) {
			// the draw point is taken relative to the clip window, so the whole surface is
			// handed over as the window and the square is named in surface coordinates
			Draw_Shape(surface, *CameoDrawer, ability.Cameo, 0, Point2D(cell.X, cell.Y),
				surface.Get_Rect(), ShapeFlags_Type(SHAPE_WIN_REL));
		}

		// whatever has not charged up yet is dimmed, so the charge is read at a glance
		int const icon_height = cell.Height - PANEL_CAPTION_HEIGHT;
		int const charged = (icon_height * ability.Charge_Percent()) / 100;
		if (charged < icon_height) {
			surface.Fill_Rect_Trans(Rect(cell.X, cell.Y, cell.Width, icon_height - charged), PANEL_SHADE, 70);
		}

		surface.Draw_Rect(cell, aiming ? PANEL_FRAME_AIMING : (ready ? PANEL_FRAME_READY : PANEL_FRAME_WAITING));

		// how long until it is ready, or how many uses are left
		char info[16];
		if (ready) {
			sprintf(info, "%d", ability.Charges);
		} else {
			sprintf(info, "%d", ability.Seconds_Left());
		}

		Fancy_Text_Print(info, surface, cell, Point2D(cell.X + cell.Width - 3, cell.Y + 2),
			Fetch_Scheme_By_Name(ready ? "Green" : "LightGrey"), TBLACK,
			TextPrintType(TPF_RIGHT|TPF_8POINT|TPF_FULLSHADOW));

		// the name, in the place and the style the sidebar captions its cameos with
		Fancy_Text_Print(ability.Name, surface, cell,
			Point2D(cell.X + cell.Width / 2, cell.Y + SidebarClass::StripClass::CAMEO_TEXT_Y_OFFSET),
			Fetch_Scheme_By_Name(ready ? "Green" : "Grey"), TBLACK,
			TextPrintType(TPF_CENTER|TPF_8POINT|TPF_FULLSHADOW));
	}
}


/// <summary>
/// Records what the panel is doing, so that a saved game carries the timers on.
/// </summary>
void SuperPanelClass::Serialize(SaveStreamClass & stream)
{
	for (int index = 0; index < MAX_SLOTS; index++) {
		stream.Serialize(Slots[index].Cooldown);
		stream.Serialize(Slots[index].Charges);
	}

	stream.Serialize(Pending);

	// a saved game cannot be aiming at anything
	Pending = -1;
	Map.IsTargettingMode = SUPER_NONE;
}


void SuperPanelClass::Compute_CRC(CRCEngine & crc) const
{
	for (int index = 0; index < MAX_SLOTS; index++) {
		crc(Slots[index].Cooldown);
		crc(Slots[index].Charges);
	}
}
