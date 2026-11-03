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
#include "rect.h"
#include "vector.h"

class CCINIClass;
class ObjectClass;
class Surface;
class TechnoClass;


/// <summary>
/// The survival mode: the player picks a side and a hero, reads what the stage asks of him, and
/// then keeps the hero alive while waves of the other side come at him from the edge of the map.
/// A map turns the mode on by carrying a [Survivors] section of its own.
/// </summary>
class SurvivorsClass
{
	public:
		enum PhaseType {
			PHASE_OFF,			///< the mode is not running
			PHASE_SIDE,			///< choosing the side to play
			PHASE_HERO,			///< choosing one of that side's heroes
			PHASE_STAGE,		///< reading what this stage asks for
			PHASE_WAVE,			///< fighting, with the next wave on its timer
			PHASE_OVER			///< the hero is gone and the run has ended
		};

		SurvivorsClass(void);

		bool Is_Wanted(void) const { return(Enabled); }
		bool Is_Active(void) const { return(Phase != PHASE_OFF); }
		int Current_Wave(void) const { return(Wave); }

		void Read_INI(CCINIClass const & ini);
		void Reset(void);
		void Logic(void);
		void Draw_On_Field(Surface & surface);

		bool Click(Point2D const & screen);

	private:
		bool Begin(void);
		bool Hero_Alive(void) const;
		void Finish(char const * reason);
		void Spawn_Wave(void);
		void Spawn_Squad(void);
		void Keep_Squad_Together(void);
		void Follow_With_Camera(void);
		TechnoClass * Spawn_Enemy(HouseClass * house, Cell const & around, int radius);
		HouseClass * Enemy_House(void) const;
		Cell Hero_Cell(void) const;
		int Enemies_On_Field(void) const;

		void Draw_Choices(Surface & surface);
		void Draw_Stage(Surface & surface);
		Rect Card_Rect(int index, int count) const;
		bool Choose(int index);

		int FrameW;				///< width of the frame the cards were last placed on
		int FrameH;
		bool Enabled;			///< the map asked for the mode
		PhaseType Phase;
		int Wave;				///< the wave being fought, counting from one
		int WaveTimer;			///< ticks left until the next wave
		int WaveInterval;		///< ticks between waves
		int SquadTimer;			///< ticks left until the squad is looked at again
		int CameraTimer;		///< ticks left until the camera is pulled to the hero
		int GraceTimer;			///< ticks the hero is given to stand up
		int SquadCount;
		int SideIndex;			///< which of the three sides was picked
		int HeroIndex;			///< which of that side's heroes was picked
		bool AutoPick;			///< start at once with the first side and hero, for testing
		bool UseMapHero;		///< the map named the hero itself, so the choice is skipped
		char HeroName[16];
		char SquadName[16];
		char StageName[64];
		char StageLine[192];
		HouseClass * Enemy;
		TechnoClass * Hero;
		DynamicVectorClass<TechnoClass *> Squad;
};

extern SurvivorsClass Survivors;
