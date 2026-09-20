/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

class Surface;
class ConvertClass;
class ShapeSet;
template<class T> class TRect;
typedef TRect<int> Rect;

class MSFont
{
	public:
		MSFont(bool use_side_palette = true);
		MSFont(char const * file_name);
		MSFont(char const * file_name, char const * palette_name);
		virtual ~MSFont(void);

		///
		/// Enlarges the font by a whole number of times. A display larger than the one the
		/// screens were written for asks for bigger text, and text drawn a whole pixel at a
		/// time stays sharp where a stretched picture of it would not.
		///
		void Set_Scale(int numerator, int denominator = 1);

		int Get_Font_Width(void) const;
		int Get_Font_Height(void) const;

		unsigned char Get_Red(void) {return(Red);}
		unsigned char Get_Green(void) {return(Green);}
		unsigned char Get_Blue(void) {return(Blue);}
		int Get_Color(void) {return(Color);}

		bool Init(char const * file_name, char const * palette_name);

		void Get_String_Rect(char const * string, Rect & rect);

		virtual int Get_Character_Width(char32_t code);
		int Get_Character_Width(char) = delete;
		int Get_Character_Width(unsigned char) = delete;
		virtual int Get_String_Width(char const * string);

		virtual void Draw_String(Surface * surface, char const * string, int x, int y, int frame);
		virtual void Draw_Character(Surface * surface, char32_t code, int x, int y, int frame, bool do_sound);
		void Draw_Character(Surface *, char, int, int, int, bool) = delete;
		void Draw_Character(Surface *, unsigned char, int, int, int, bool) = delete;

	private:
		int Glyph_Frame(char32_t code) const;

		///
		/// Draws one glyph enlarged, a pixel of the glyph at a time. Only used when the font
		/// has been scaled up.
		///
		void Draw_Glyph_Enlarged(Surface * surface, int shape_frame, int x, int y);

		/*
		 * These are the width and height of the font's glyph cell, expressed in pixels and
		 * taken from the shape file as it is loaded. The height is what a newline advances
		 * by, and the width serves as a rough per character measure.
		 */
		int FontWidth;
		int FontHeight;

		/*
		 * How much the glyphs and the metrics are enlarged by. The numerator is the size the
		 * font is drawn at and the denominator the size it was made at, so a font of eight
		 * pixels drawn at twice the size has a numerator of two.
		 */
		int ScaleNum;
		int ScaleDen;

		/*
		 * These are the red, green and blue components of the color the font prints in, read
		 * out of the palette that colors the glyphs. They let a caller shade a panel or a
		 * rule to match the text that will be drawn over it.
		 */
		unsigned char Red;
		unsigned char Green;
		unsigned char Blue;

		/*
		 * This is that same font color built into a pixel of the display's own format, ready
		 * to be handed to the line and rectangle drawing routines.
		 */
		int Color;

		/*
		 * This points to the glyph shapes for the font. Every printable character owns three
		 * consecutive frames, which the menu print animations step through to fade a
		 * character in, and the last of the three is the one measured for the glyph's width.
		 */
		ShapeSet * FontFile;

		/*
		 * This points to the conversion table the glyphs are drawn through, built from the
		 * font's own palette. It is what remaps the glyph shapes into the colors that the
		 * visible surface is running in.
		 */
		ConvertClass * Drawer;

		/*
		 * If the glyph shapes had to be loaded from disk rather than found already resident
		 * in a mixfile, then this flag will be true. Only shapes loaded that way belong to
		 * the font, so only those are freed when the last font is destroyed.
		 */
		bool AllocLoaded;
};
