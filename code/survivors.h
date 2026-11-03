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
/// The survival mode. The player picks a side and a hero, reads what the stage asks of him, then
/// keeps the hero alive: waves of the other side come at him from around the map, the men they
/// lose leave tiberium behind, and what the hero picks up buys him improvements. After the last
/// wave a super boss comes, and beating it opens the way out under fire.
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
			PHASE_CARD,			///< a level was gained, one of three cards is being chosen
			PHASE_EVAC,			///< the way out is open and the hero has to reach it
			PHASE_OVER			///< the hero is gone and the run has ended
		};

		/// <summary>Tiberium left where a man fell, waiting to be walked over.</summary>
		struct PickupType {
			Cell Where;
			int Life;			///< ticks before it is lost
			int Value;			///< experience it carries
		};

		/// <summary>What a card does when it is taken.</summary>
		enum EffectType {
			EFFECT_NONE,
			EFFECT_REINFORCE,	///< two more men join the squad
			EFFECT_HEAL,		///< the hero and the squad are mended
			EFFECT_CRYSTAL,		///< tiberium is worth more from now on
			EFFECT_BREATHER,	///< the waves come more slowly
			EFFECT_REVIVE,		///< the hero survives one death
			EFFECT_SHIELD,		///< the squad is kept closer and steadier
			EFFECT_MAGNET,		///< tiberium is pulled in from further off
			EFFECT_PULL,		///< tiberium flies to the hero faster
			EFFECT_AMMO,		///< the hero and his squad hit harder
			EFFECT_ARMOR,		///< the hero and his squad take less
			EFFECT_COUNT
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
		void Spawn_Boss(void);
		void Spawn_Squad(void);
		void Add_To_Squad(int how_many);
		void Keep_Squad_Together(void);
		void Follow_With_Camera(void);
		TechnoClass * Spawn_Enemy(HouseClass * house, Cell const & around, int radius);
		TechnoClass * Spawn_Boss_Unit(HouseClass * house, Cell const & around, int radius);
		HouseClass * Enemy_House(void) const;
		Cell Hero_Cell(void) const;
		int Enemies_On_Field(void) const;
		int Nearest_Crystal(void) const;

		void Watch_Foes(void);
		void Drop_Crystal(Cell const & where, int value);
		void Collect_Crystals(void);
		void Open_Cards(void);
		void Apply_Card(int index);
		void Mend(void);
		void Apply_Bias(void);
		void Start_Evacuation(void);
		void Start_Task(void);
		void Watch_Task(void);
		void Draw_Task(Surface & surface);
		void Draw_Crystals(Surface & surface);

		void Draw_Choices(Surface & surface);
		void Draw_Stage(Surface & surface);
		void Draw_Cards(Surface & surface);
		void Draw_Evacuation(Surface & surface);
		void Draw_Run_Line(Surface & surface);
		Rect Card_Rect(int index, int count, int top, int height) const;
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
		int FoeTimer;			///< ticks left until the fallen are looked for
		int SquadCount;
		int SideIndex;			///< which of the three sides was picked
		int HeroIndex;			///< which of that side's heroes was picked
		bool AutoPick;			///< start at once with the first side and hero, for testing
		bool UseMapHero;		///< the map named the hero itself, so the choice is skipped
		int Xp;					///< experience carried towards the next level
		int XpNeeded;			///< where that level is reached
		int Level;
		int Crystals;			///< tiberium gathered over the run
		int Kills;
		int CrystalWorth;		///< what one crystal is worth, raised by a card
		int WaveScale;			///< percent of the usual wave pace, lowered by a card
		bool ReviveHeld;		///< the hero has one death forgiven
		int Revives;
		int Cards[3];			///< the effects offered on the card table
		int CardPick;
		int CardTimer;
		int StatusTimer;
		int PhaseTimer;
		double DamageBias;			///< how hard the player's men hit
		double ArmorBias;			///< how well they stand up
		int MagnetRadius;			///< how far the hero pulls tiberium in
		int PullStep;				///< how fast it comes
		int LastWave;
		bool TaskActive;			///< a bonus task is on the map
		Cell TaskCell;				///< where it has to be held
		int TaskHeld;				///< ticks spent inside the zone
		int TaskLeft;				///< ticks the zone has to be held for
		int TaskAway;				///< ticks spent outside it since the last look
		int BossEvery;			///< which card the player took, for the record
		int BossPending;		///< a boss is due on this wave
		int EvacTimer;			///< ticks left to reach the way out
		Cell EvacCell;
		char HeroName[16];
		char SquadName[16];
		char StageName[64];
		char StageLine[192];
		HouseClass * Enemy;
		TechnoClass * Hero;
		DynamicVectorClass<TechnoClass *> Squad;
		DynamicVectorClass<TechnoClass *> Foes;		///< the enemy seen last tick
		DynamicVectorClass<Cell> FoeCells;			///< where each of them was
		DynamicVectorClass<PickupType> Pickups;
};

extern SurvivorsClass Survivors;
