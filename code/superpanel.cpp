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
#include "combat.h"
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
#include "infantry.h"
#include "infatype.h"
#include "building.h"
#include "builtype.h"
#include "aircraft.h"
#include "airctype.h"
#include "houstype.h"
#include "rgb.h"
#include "scheme.h"
#include "_convert.h"
#include "_mixfile.h"
#include "mixfile.h"
#include "_rect.h"
#include "map.h"
#include "overlay.h"
#include "overtype.h"
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

#include <cstdio>
#include <cstring>


SuperPanelClass SuperPanel;

static char const * const SUPERPOWERS_INI = "SUPERPOWERS.INI";
static char const * const PANEL_SECTION = "Panel";

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
				SuperWeaponType type = SuperPanelAbilityClass::Weapon_From_Name(name);
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
/// Creates an ability with everything switched off.
/// </summary>
SuperPanelAbilityClass::SuperPanelAbilityClass(void) :
	Type(ABILITY_NONE),
	Delivery(DELIVERY_AIR),
	Side(-1),
	Charge(240),
	Count(1),
	Tiberium(false),
	Weapon(SUPER_NONE),
	Cursor(SUPER_NONE),
	Cameo(NULL),
	Cooldown(0),
	Charges(0)
{
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
	Tiberium = ini.Get_Bool(section, "Tiberium", Tiberium);

	// the weapon that carries out the effect and lends its target cursor
	ini.Get_String(section, "Weapon", "", buffer, sizeof(buffer));
	Weapon = Weapon_From_Name(buffer);

	// a weapon borrowed only for its target cursor, for abilities the panel carries out itself
	ini.Get_String(section, "Cursor", "", buffer, sizeof(buffer));
	Cursor = Weapon_From_Name(buffer);
	if (Cursor == SUPER_NONE) {
		Cursor = Weapon;
	}

	// the icon: a super weapon, a unit or building type, or a shape in the mix files
	ini.Get_String(section, "Cameo", "", buffer, sizeof(buffer));
	Cameo = Resolve_Cameo(buffer, Cursor);

	ini.Get_String(section, "Side", "", buffer, sizeof(buffer));
	if (buffer[0] == '\0' || stricmp(buffer, "any") == 0) {
		Side = -1;
	} else {
		Side = HouseTypeClass::From_Name(buffer);
	}

	return(true);
}


/// <summary>
/// Puts the ability back to the start of a mission.
/// </summary>
void SuperPanelAbilityClass::Reset(void)
{
	Cooldown = Charge * TICKS_PER_SECOND;
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
	Pending(-1)
{
}


void SuperPanelClass::One_Time(void)
{
}


/// <summary>
/// Reads the ability set out of SUPERPOWERS.INI, then lets the mission override it.
/// The file is optional: with no file the panel simply stays hidden.
/// </summary>
void SuperPanelClass::Read_INI(CCINIClass const & mission_ini)
{
	char section[64];
	CCINIClass ini;

	SlotCount = 0;
	Pending = -1;

	CCFileClass file(SUPERPOWERS_INI);
	if (!file.Is_Available()) {
		return;
	}

	ini.Load(file, false);

	TestAll = ini.Get_Bool(PANEL_SECTION, "TestAll", false);

	// every entry of [Panel] names a section holding one ability
	int entries = ini.Entry_Count(PANEL_SECTION);
	for (int index = 0; index < entries; index++) {
		char const * entry = ini.Get_Entry(PANEL_SECTION, index);
		if (entry == NULL) continue;
		if (stricmp(entry, "Slots") == 0 || stricmp(entry, "TestAll") == 0) continue;

		ini.Get_String(PANEL_SECTION, entry, "", section, sizeof(section));
		if (section[0] == '\0') continue;
		if (SlotCount >= MAX_SLOTS) break;

		if (Slots[SlotCount].Read_INI(ini, section)) {
			SlotCount++;
		}
	}

	DebugString("SuperPanel: %d abilities read (%s)\n", SlotCount, TestAll ? "test set" : "mission set");

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

		if (Slots[index].Is_Available()) {
			Aim_At(index);
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
				done = Damage_Area(cell, 4, 150, "AP", false);
				break;

			case SuperPanelAbilityClass::ABILITY_HUNTER_SEEKER:
				done = Damage_Area(cell, 1, 600, "AP", false);
				break;

			case SuperPanelAbilityClass::ABILITY_TIBERIUM_SEED:
				done = Place_Seed(cell, 3, "TIB01");
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

	if (done) {
		ability.Charges--;
		ability.Cooldown = ability.Charge * TICKS_PER_SECOND;
	}

	return(done);
}


/// <summary>
/// Lands a squad of the listed units around the cell given.
/// Infantry arrive by drop pod when the delivery is airborne; anything else is placed on the
/// ground next to the spot.
/// </summary>
bool SuperPanelClass::Place_Squad(SuperPanelAbilityClass const & ability, Cell const & cell, bool airborne)
{
	int placed = 0;
	char list[256];

	strncpy(list, ability.Units, sizeof(list) - 1);
	list[sizeof(list) - 1] = '\0';

	if (list[0] == '\0') {
		strcpy(list, "E1,E1,E2");
	}

	char * token = strtok(list, ",");
	while (token != NULL) {
		while (*token == ' ' || *token == '\t') token++;
		char * end = token + strlen(token);
		while (end > token && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';

		if (*token == '\0') {
			token = strtok(NULL, ",");
			continue;
		}

		Cell nearby = Map.Nearby_Location(cell, SPEED_FOOT);
		if (!Map.In_Radar(nearby)) {
			nearby = cell;
		}

		InfantryType itype = InfantryTypeClass::From_Name(token);
		if (itype != INFANTRY_NONE) {
			InfantryClass * inf = (InfantryClass *)InfantryTypes[itype]->Create_One_Of(PlayerPtr);
			if (inf != NULL) {
				inf->Veterancy.Set_Elite(true);
				if (airborne) {
					inf->Link_DropPod();
				}
				inf->PositionCoord = nearby;
				inf->Look();
				inf->Assign_Mission(MISSION_GUARD_AREA);
				inf->Commence();
				placed++;
			}
		} else {
			UnitType utype = UnitTypeClass::From_Name(token);
			if (utype != UNIT_NONE) {
				UnitClass * unit = new UnitClass(UnitTypes[utype], PlayerPtr);
				if (unit != NULL && unit->Unlimbo(Coord(nearby), DIR_N)) {
					unit->Assign_Mission(MISSION_GUARD_AREA);
					unit->Commence();
					placed++;
				} else {
					delete unit;
				}
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
