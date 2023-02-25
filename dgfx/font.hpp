// MIT License
//
// Copyright (c) 2023 Anton Schreiner
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#if !defined(FONT_HPP)
#    define FONT_HPP

static const u32 simplefont_bitmap_width          = u32(128);
static const u32 simplefont_bitmap_height         = u32(128);
static const u32 simplefont_bitmap_glyphs_per_row = u32(16);
static const u32 simplefont_bitmap_glyphs_width   = u32(8);
static const u32 simplefont_bitmap_glyphs_pad_x   = u32(0);
static const u32 simplefont_bitmap_glyphs_pad_y   = u32(0);
static const u32 simplefont_bitmap_glyphs_height  = u32(16);

// origin: Tamsyn http://www.fial.com/~scott/tamsyn-font/
// Tamsyn font is free.  You are hereby granted permission to use, copy, modify,
// and distribute it as you see fit.
//
// Tamsyn font is provided "as is" without any express or implied warranty.
//
// The author makes no representations about the suitability of this font for
// a particular purpose.
//
// In no event will the author be held liable for damages arising from the use
// of this font.
static char const *simplefont_bitmap[] = {
    "                                                                                                                                ",
    "                  *  *                                     *                                                                    ",
    "           *      *  *              *             **       *         *    *                                                   * ",
    "           *      *  *    *  *      *     **     *  *      *        *      *                                                  * ",
    "           *      *  *    *  *     ****  *  *  * *  *      *        *      *                *                                *  ",
    "           *             ******   *      *  * *  *  *              *        *     *  *      *                                *  ",
    "           *              *  *    *       ** *    **               *        *      **       *                               *   ",
    "           *              *  *     ***      *     **   *           *        *    ******  *******         ******             *   ",
    "                          *  *        *    * **  *  *  *           *        *      **       *                              *    ",
    "                         ******       *   * *  * *   **            *        *     *  *      *                              *    ",
    "           *              *  *    ****   *  *  * *   **            *        *               *      **              **     *     ",
    "           *              *  *      *        **   ***  *            *      *                       **              **     *     ",
    "                                    *                               *      *                        *                    *      ",
    "                                                                     *    *                         *                    *      ",
    "                                                                                                   *                            ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                          ****  ",
    "  ****      *     ****   ******      *   ******    ***   ******   ****    ****                       *            *      *    * ",
    " *    *    **    *    *      *      **   *        *           *  *    *  *    *                     *              *          * ",
    " *   **   * *         *     *      * *   *       *           *   *    *  *    *    **      **      *                *        *  ",
    " *  * *     *         *    ***    *  *   *****   *           *   *    *  *    *    **      **     *      ******      *      *   ",
    " * *  *     *        *        *  *   *        *  *****      *     ****    *****                  *                    *    *    ",
    " **   *     *       *         *  ******       *  *    *     *    *    *       *                   *                  *          ",
    " *    *     *      *          *      *        *  *    *    *     *    *       *                    *     ******     *           ",
    " *    *     *     *      *    *      *   *    *  *    *    *     *    *      *     **      **       *              *       *    ",
    "  ****    *****  ******   ****       *    ****    ****     *      ****    ***      **      **        *            *        *    ",
    "                                                                                            *                                   ",
    "                                                                                            *                                   ",
    "                                                                                           *                                    ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "   ***                                                                                                                          ",
    "  *   *    **    *****     ****  ****    ******  ******    ****  *    *   *****       *  *    *  *       *     * *    *   ****  ",
    " *     *  *  *   *    *   *      *   *   *       *        *      *    *     *         *  *   *   *       **   ** **   *  *    * ",
    " *  **** *    *  *    *  *       *    *  *       *       *       *    *     *         *  *  *    *       * * * * * *  *  *    * ",
    " * *   * *    *  *    *  *       *    *  *       *       *       *    *     *         *  * *     *       *  *  * *  * *  *    * ",
    " * *   * *    *  *****   *       *    *  *****   *****   *   **  ******     *         *  **      *       *  *  * *   **  *    * ",
    " * *   * ******  *    *  *       *    *  *       *       *    *  *    *     *         *  * *     *       *     * *    *  *    * ",
    " * *  ** *    *  *    *  *       *    *  *       *       *    *  *    *     *    *    *  *  *    *       *     * *    *  *    * ",
    " *  ** * *    *  *    *   *      *   *   *       *        *   *  *    *     *    *    *  *   *   *       *     * *    *  *    * ",
    " *       *    *  *****     ****  ****    ******  *         ****  *    *   *****   ****   *    *  ******  *     * *    *   ****  ",
    "  *                                                                                                                             ",
    "   *****                                                                                                                        ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                           ****  *       ****      *            ",
    " *****    ****   *****    *****  ******* *    *  *    *  *     * *     * *     * ******    *     *          *     * *           ",
    " *    *  *    *  *    *  *          *    *    *  *    *  *     * *     * *     *     *     *      *         *    *   *          ",
    " *    *  *    *  *    *  *          *    *    *  *    *  *     *  *   *   *   *     *      *      *         *                   ",
    " *    *  *    *  *    *   *         *    *    *  *    *  *     *   * *     * *      *      *       *        *                   ",
    " *****   *    *  *****     **       *    *    *  *    *  *  *  *    *       *      *       *       *        *                   ",
    " *       *    *  *  *        *      *    *    *   *  *   *  *  *   * *      *      *       *        *       *                   ",
    " *       *    *  *   *        *     *    *    *   *  *   *  *  *  *   *     *     *        *        *       *                   ",
    " *       *    *  *    *       *     *    *    *    **    * * * * *     *    *     *        *         *      *                   ",
    " *        ****   *    *  *****      *     ****     **    **   ** *     *    *    ******    *         *      *                   ",
    "             *                                                                             *          *     *                   ",
    "              *                                                                            ****       *  ****           ********",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "  *                                                                                                                             ",
    "   *             *                    *             ***          *          *        *   *        ***                           ",
    "    *            *                    *            *             *          *        *   *          *                           ",
    "     *           *                    *            *             *                       *          *                           ",
    "          ****   *****    ****    *****   ****   ******   *****  *****    ***      ***   *   *      *    *** *** *****    ****  ",
    "              *  *    *  *    *  *    *  *    *    *     *    *  *    *     *        *   *  *       *    *  *  * *    *  *    * ",
    "              *  *    *  *       *    *  *    *    *     *    *  *    *     *        *   * *        *    *  *  * *    *  *    * ",
    "          *****  *    *  *       *    *  ******    *     *    *  *    *     *        *   ***        *    *  *  * *    *  *    * ",
    "         *    *  *    *  *       *    *  *         *     *    *  *    *     *        *   *  *       *    *  *  * *    *  *    * ",
    "         *    *  *    *  *    *  *    *  *         *     *    *  *    *     *        *   *   *      *    *  *  * *    *  *    * ",
    "          *****  *****    ****    *****   *****    *      *****  *    *   *****      *   *    *   *****  *  *  * *    *   ****  ",
    "                                                              *                      *                                          ",
    "                                                              *                      *                                          ",
    "                                                          ****                    ***                                           ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                            ***     *    ***                    ",
    "                                                                                           *        *       *     **   *        ",
    "                                   *                                                       *        *       *    *  *  *        ",
    "                                   *                                                       *        *       *    *   **         ",
    " *****    *****   * ***   *****  ******  *    *  *    *  *     * *     * *    *  ******    *        *       *                   ",
    " *    *  *    *   **     *         *     *    *  *    *  *     *  *   *  *    *      *     *        *       *                   ",
    " *    *  *    *   *       *        *     *    *  *    *  *     *   * *   *    *     *   ***         *        ***                ",
    " *    *  *    *   *        **      *     *    *   *  *   *  *  *    *    *    *    *       *        *       *                   ",
    " *    *  *    *   *          *     *     *    *   *  *   *  *  *   * *   *    *   *        *        *       *                   ",
    " *    *  *    *   *           *    *     *    *    **    * * * *  *   *  *    *  *         *        *       *                   ",
    " *****    *****   *      *****      ***   *****    **    **   ** *     *  *****  ******    *        *       *                   ",
    " *            *                                                               *            *        *       *                   ",
    " *            *                                                               *             ***     *    ***                    ",
    " *            *                                                           ****                                                  ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
    "                                                                                                                                ",
};

#endif // FONT_HPP