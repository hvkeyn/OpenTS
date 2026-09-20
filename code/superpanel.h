/*******************************************************************************
 *                                O P E N T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "always.h"

#include "cell.h"
#include "keyboard.h"
#include "rect.h"
#include "coord.h"
#include "side.hh"
#include "super.hh"

class CCINIClass;
class Surface;
class SaveStreamClass;
class CRCEngine;
class TechnoClass;
class ShapeSet;

/*
 * A super power panel: a block of squares that sits beside the radar. Each square carries one
 * ability, its own charge timer and the number of times it may still be used, and the set of
 * abilities is read from SUPERPOWERS.INI, which a mission may override.
 *
 * An ability is either one of the game's own super weapons - the square then borrows that
 * weapon's cameo, its target cursor and its effect - or one of the panel's own, in which case
 * only the cursor is borrowed and the panel carries out the effect itself.
 */

class SuperPanelAbilityClass
{
	public:
		enum AbilityType {
			ABILITY_NONE = 0,
			ABILITY_DROP_PODS,
			ABILITY_ION_CANNON,
			ABILITY_MISSILE,
			ABILITY_CHEM_MISSILE,
			ABILITY_HUNTER_SEEKER,
			ABILITY_EM_PULSE,
			ABILITY_FIRESTORM,
			ABILITY_AIR_REINFORCE,
			ABILITY_UNIT_REINFORCE,
			ABILITY_TIBERIUM_SEED,
			ABILITY_ARMOR_BOOST,
			ABILITY_RECON,
			ABILITY_BARRAGE
		};

		enum DeliveryType {
			DELIVERY_AIR = 0,
			DELIVERY_UNDERGROUND,
			DELIVERY_ORBITAL
		};

	public:
		SuperPanelAbilityClass(void);

		bool Read_INI(CCINIClass const & ini, char const * section);
		void Reset(void);

		bool Is_Available(void) const;      // has uses left
		bool Is_Ready(void) const;          // charged and usable
		int  Seconds_Left(void) const;
		int  Charge_Percent(void) const;    // how much of the charge has built up, 0..100

		void Charge_Up(void);               // called once per logic frame
		bool Fire(Cell const & cell);       // apply the effect

		static AbilityType Type_From_Name(char const * name);
		static DeliveryType Delivery_From_Name(char const * name);
		static SuperWeaponType Weapon_From_Name(char const * name);

	public:
		char Section[32];                   /// the INI section the ability was read from
		char Name[64];
		char Description[256];
		char Hint[128];
		char Role[32];
		AbilityType Type;
		DeliveryType Delivery;
		SideType Side;                      // SIDE_NONE = the ability suits any side
		int Tier;                           // how far into a campaign it becomes available
		int Charge;                         // seconds of charge
		int Count;                          // uses per mission
		bool Tiberium;                      // leave tiberium behind
		char Units[256];                    // comma separated type names

		SuperWeaponType Weapon;             /// the game's weapon that carries out the effect
		SuperWeaponType Cursor;             /// the game's weapon whose target cursor is borrowed
		ShapeSet const * Cameo;             /// the icon shown in the square

		// runtime state
		int Cooldown;                       // ticks left until ready
		int Charges;                        // uses left
};

class SuperPanelClass
{
	public:
		enum { MAX_SLOTS = 6 };

		SuperPanelClass(void);

		void One_Time(void);
		void Read_INI(CCINIClass const & ini);
		void Reset(void);
		void Logic(void);                   // one pass of the charge timers
		bool Click(Point2D const & screen); /// a click on the strip: aim, or say it is not ready
		bool Target(Point2D const & screen);/// a click on the field while aiming
		bool Cancel(void);                  /// give up aiming

		bool Is_Aiming(void) const { return(Pending >= 0); }

		void Draw(Surface & surface, Rect const & strip);
		void Draw_On_Field(Surface & surface);
		void Serialize(SaveStreamClass & stream);
		void Compute_CRC(CRCEngine & crc) const;

		int Count(void) const;
		SuperPanelAbilityClass & operator[](int index) { return(Slots[index]); }
		SuperPanelAbilityClass const & operator[](int index) const { return(Slots[index]); }

		/// The game's weapon that performs the behaviour named, or SUPER_NONE.
		static SuperWeaponType Find_Weapon(SuperWeaponType behaviour);

		/// The screen rectangle of the whole strip and of one square inside it.
		static Rect Strip_Rect(void);
		Rect Cell_Rect(int index, Rect const & strip) const;

	protected:
		static SideType Panel_Side(void);
		static int Mission_Tier(char const * map_name);
		bool Fire_Slot(int index, Cell const & cell);
		void Aim_At(int index);
		bool Place_Squad(SuperPanelAbilityClass const & ability, Cell const & cell, bool airborne);
		bool Place_Seed(Cell const & cell, int radius, char const * overlay_name);
		bool Reveal_Area(Cell const & cell, int radius);
		bool Barrage(Cell const & cell, int shells, int radius, int strength, char const * warhead_name);
		bool Damage_Area(Cell const & cell, int radius, int strength, char const * warhead_name, bool tiberium);

	public:
		SuperPanelAbilityClass Slots[MAX_SLOTS];
		int SlotCount;
		bool TestAll;
		int Pending;                        /// the square that is waiting for a target, or -1
};

extern SuperPanelClass SuperPanel;
