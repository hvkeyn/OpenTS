/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "vector.h"

class MSAnim;
class MSSfx;
template<class T> class TRect;
typedef TRect<int> Rect;
class Surface;


class MSEngine
{
	public:
		MSEngine(void);
		virtual ~MSEngine(void);
		virtual void Do_Custom_Draw(Surface * surface) {}
		virtual void Process_Idle(void) {}

		int Add_Animation(MSAnim * anim);
		void Replace_Anim(MSAnim * new_anim, MSAnim * old_anim);
		void Remove_Anim(MSAnim * anim);
		void Wait_For_Anim(MSAnim * anim, unsigned delay=300000, bool finish_on_input=false);
		DynamicVectorClass<MSAnim *> * Get_Anims(void) { return(&Anims); }

		void Restore_And_Advance(void);
		void Advance(Surface * surface);
		void Restore_Anims(Rect const & rect);

		void Add_Update_Rect(Rect const & rect);
		void Blit_All(Surface * surface);
		void Blit_Rect(Surface * surface, Rect const & rect);

		/*
		 * A screen that was laid out for the 640 by 400 display draws onto a page of its
		 * own and has that page stretched onto the frame when it presents itself. A screen
		 * that sets no page draws straight onto the frame, as it always has.
		 */
		void Set_Page(Surface * page) { StagedPage = page; }
		Surface * Get_Page(void) const;
		void Present_Page(void);

		void Wait_Delay(int delay);
		void Wait_For_Focus(void);

		bool Add_Sound_Effect(char const * name, char const * file_name);
		void Play_Sound_Effect(char const * name, int volume);

	protected:
		/*
		 * This is the number of update regions currently pending. The list they sit in is
		 * a fixed size, so this count is what separates a live region from stale storage.
		 */
		int RectCount;

		/*
		 * These are the regions of the screen that have been drawn over and are still due
		 * to be sent to the visible surface. Overlapping regions are combined as they are
		 * added, and once the list is full a new one is folded into the cheapest to grow.
		 */
		VectorClass<Rect> Rects;

		/*
		 * These are the animations the engine is running, in the order they are drawn. The
		 * engine owns them, and disposes of each one as it reports itself finished.
		 */
		DynamicVectorClass<MSAnim *> Anims;

		/*
		 * These are the sound effects the screen has registered, searched by name whenever
		 * one is asked to be played. The engine owns them and destroys them with itself.
		 */
		DynamicVectorClass<MSSfx *> Sounds;

		/*
		 * This is the page a staged screen draws on, or NULL when the screen draws on the
		 * frame itself.
		 */
		Surface * StagedPage;
};
