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

class CCINIClass;
class Surface;
class SaveStreamClass;
class CRCEngine;
class TechnoClass;

/*
 * A super power panel: a strip of six cells that sits at the top of the battlefield, next to
 * the sidebar. Each cell carries one ability, its own charge timer and the number of times it
 * may still be used, and the set of abilities is read from SUPERPOWERS.INI, which a mission
 * may override.
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
			ABILITY_ARMOR_BOOST
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

		bool Is_Available(void) const;      // has charges left
		bool Is_Ready(void) const;          // charged and usable
		int  Seconds_Left(void) const;

		void Charge_Up(void);               // called each logic frame
		bool Fire(Cell const & cell);       // apply the effect

		static AbilityType Type_From_Name(char const * name);
		static DeliveryType Delivery_From_Name(char const * name);

	public:
		char Name[64];
		char Description[256];
		char Hint[128];
		char Role[32];
		AbilityType Type;
		DeliveryType Delivery;
		int Side;                           // -1 = any, otherwise the side index
		int Charge;                         // seconds of charge
		int Count;                          // uses per mission
		bool Tiberium;                      // leave tiberium behind
		char Units[256];                    // comma separated type names

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
		void AI(KeyNumType & input, Point2D const & xy);
		void Draw(Surface & surface, Rect const & view);
		void Serialize(SaveStreamClass & stream);
		void Compute_CRC(CRCEngine & crc) const;

		int Count(void) const;
		SuperPanelAbilityClass & operator[](int index) { return(Slots[index]); }
		SuperPanelAbilityClass const & operator[](int index) const { return(Slots[index]); }

		Rect Cell_Rect(int index, Rect const & view) const;

	protected:
		bool Fire_Slot(int index, Cell const & cell);
		bool Place_Squad(SuperPanelAbilityClass const & ability, Cell const & cell, bool airborne);
		bool Place_Seed(Cell const & cell, int radius, char const * overlay_name);
		bool Damage_Area(Cell const & cell, int radius, int strength, char const * warhead_name, bool tiberium);

	public:
		SuperPanelAbilityClass Slots[MAX_SLOTS];
		int SlotCount;
		bool TestAll;
};

extern SuperPanelClass SuperPanel;
