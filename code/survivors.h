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
#include "vector.h"

class CCINIClass;
class ObjectClass;
class TechnoClass;
class HouseClass;
class Surface;


/// <summary>
/// The survival mode: one hero the player drives, a squad that keeps to him, and waves of
/// enemies that come at him from every side. A map turns the mode on by carrying a
/// [Survivors] section of its own, which is also where the hero and the squad are named.
/// </summary>
class SurvivorsClass
{
	public:
		enum PhaseType {
			PHASE_OFF,		///< the mode is not running
			PHASE_WAVE,		///< a wave is being fought, the next one is on its timer
			PHASE_OVER		///< the hero is gone and the run has ended
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

		bool Enabled;			///< the map asked for the mode
		PhaseType Phase;
		int Wave;				///< the wave being fought, counting from one
		int WaveTimer;			///< ticks left until the next wave
		int WaveInterval;		///< ticks between waves
		int SquadTimer;			///< ticks left until the squad is looked at again
		int GraceTimer;
		int CameraTimer;		///< ticks left until the camera is pulled to the hero
		int SquadCount;
		char HeroName[16];
		char SquadName[16];
		HouseClass * Enemy;
		TechnoClass * Hero;
		DynamicVectorClass<TechnoClass *> Squad;
};

extern SurvivorsClass Survivors;
