/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2013 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#ifndef XCSOAR_SCREEN_RASTER_HPP
#define XCSOAR_SCREEN_RASTER_HPP

#include "Bresenham.hpp"
#include "Murphy.hpp"
#include "Util/AllocatedArray.hpp"

#include <assert.h>
#include <string.h>

struct GreyscalePixelTraits {
  typedef uint8_t *pointer_type;
  typedef uint8_t color_type;

  int CalcIncrement(int delta) const {
    return delta;
  }

  pointer_type Increment(pointer_type p, int delta) const {
    return p + CalcIncrement(delta);
  }

  void SetPixel(pointer_type p, color_type c) const {
    *p = c;
  }

  void FillPixels(pointer_type p, unsigned n, color_type c) const {
    memset(p, c, n);
  }
};

/**
 * A software renderer for various primitives.
 *
 * Much of this code was copied from SDL_gfx.
 */
template<typename PixelTraits>
class RasterCanvas : private PixelTraits {
  typedef typename PixelTraits::pointer_type pointer_type;

public:
  typedef typename PixelTraits::color_type color_type;

  struct Point {
    int x, y;

    Point() = default;
    constexpr Point(int _x, int _y):x(_x), y(_y) {}
    constexpr Point(unsigned _x, unsigned _y):x(_x), y(_y) {}
  };

private:
  const pointer_type buffer;
  const unsigned pitch, width, height;

  AllocatedArray<int> polygon_buffer;

public:
  RasterCanvas(pointer_type _buffer,
               unsigned _pitch, unsigned _width, unsigned _height,
               PixelTraits _traits=PixelTraits())
    :PixelTraits(_traits), buffer(_buffer),
     pitch(_pitch), width(_width), height(_height) {}

protected:
  gcc_pure
  bool Check(unsigned x, unsigned y) const {
    return x < width && y < height;
  }

  pointer_type At(unsigned x, unsigned y) {
    assert(x < width);
    assert(y < height);

    return PixelTraits::Increment(buffer, pitch * y + x);
  }

  static constexpr unsigned CLIP_LEFT_EDGE = 0x1;
  static constexpr unsigned CLIP_RIGHT_EDGE = 0x2;
  static constexpr unsigned CLIP_BOTTOM_EDGE = 0x4;
  static constexpr unsigned CLIP_TOP_EDGE = 0x8;

  static constexpr bool CLIP_INSIDE(unsigned a) {
    return !a;
  }

  static constexpr bool CLIP_REJECT(unsigned a, unsigned b) {
    return a & b;
  }

  static constexpr bool CLIP_ACCEPT(unsigned a, unsigned b) {
    return !(a | b);
  }

  gcc_pure
  unsigned ClipEncode(int x, int y) const {
    unsigned code = 0;

    if (x < 0)
      code |= CLIP_LEFT_EDGE;
    else if (unsigned(x) >= width)
      code |= CLIP_RIGHT_EDGE;

    if (y < 0)
      code |= CLIP_TOP_EDGE;
    else if (unsigned(y) >= height)
      code |= CLIP_BOTTOM_EDGE;

    return code;
  }

  bool ClipLine(int &x1, int &y1, int &x2, int &y2) {
    while (true) {
      unsigned code1 = ClipEncode(x1, y1);
      unsigned code2 = ClipEncode(x2, y2);
      if (CLIP_ACCEPT(code1, code2))
        return true;

      if (CLIP_REJECT(code1, code2))
        return false;

      if (CLIP_INSIDE(code1)) {
        std::swap(x1, x2);
        std::swap(y1, y2);
        std::swap(code1, code2);
      }

      const float m = x1 != x2
        ? float(y2 - y1) / float(x2 - x1)
        : 1.0f;

      if (code1 & CLIP_LEFT_EDGE) {
        y1 += -x1 * m;
        x1 = 0;
      } else if (code1 & CLIP_RIGHT_EDGE) {
        y1 += int((width - x1) * m);
        x1 = width - 1;
      } else if (code1 & CLIP_BOTTOM_EDGE) {
        if (x2 != x1)
          x1 += int((height - y1) / m);

        y1 = height - 1;
      } else if (code1 & CLIP_TOP_EDGE) {
        if (x2 != x1)
          x1 += -y1 / m;

        y1 = 0;
      }
    }
  }

public:
  void DrawPixel(int x, int y, color_type c) {
    if (Check(x, y))
      PixelTraits::SetPixel(At(x, y), c);
  }

  void FillRectangle(int x1, int y1, int x2, int y2, color_type c) {
    if (x1 < 0)
      x1 = 0;

    if (y1 < 0)
      y1 = 0;

    if (x2 > int(width))
      x2 = width;

    if (y2 > int(height))
      y2 = height;

    if (x1 >= x2 || y1 >= y2)
      return;

    const unsigned columns = x2 - x1;

    pointer_type p = At(x1, y1);
    for (unsigned rows = y2 - y1; rows > 0; --rows, p += pitch)
      PixelTraits::FillPixels(p, columns, c);
  }

  void DrawHLine(int x1, int x2, int y, color_type c) {
    if (y < 0 || unsigned(y) >= height)
      return;

    if (x1 < 0)
      x1 = 0;

    if (x2 > int(width))
      x2 = width;

    if (x1 >= x2)
      return;

    pointer_type p = At(x1, y);
    PixelTraits::FillPixels(p, x2 - x1, c);
  }

  void DrawLine(int x1, int y1, int x2, int y2, color_type c) {
    /* optimised Bresenham algorithm */

    if (!ClipLine(x1, y1, x2, y2))
      return;

    int dx = x2 - x1;
    int dy = y2 - y1;
    const int sx = dx >= 0 ? 1 : -1;
    const int sy = dy >= 0 ? 1 : -1;

    dx = sx * dx + 1;
    dy = sy * dy + 1;

    pointer_type p = At(x1, y1);

    int pixx = PixelTraits::CalcIncrement(sx);
    int pixy = PixelTraits::CalcIncrement(sy * pitch);

    if (dx < dy) {
      std::swap(dx, dy);
      std::swap(pixx, pixy);
    }

    for (int x = 0, y = 0; x < dx; x++, p += pixx) {
      PixelTraits::SetPixel(p, c);

      y += dy;
      if (y >= dx) {
        y -= dx;
        p += pixy;
      }
    }
  }

  void DrawThickLine(int x1, int y1, int x2, int y2,
                     unsigned thickness, color_type c) {
    if (thickness == 0)
      return;

    /* Special case: thick "point" */
    if (x1 == x2 && y1 == y2) {
      const unsigned radius = thickness / 2u;
      FillRectangle(x1 - radius, y1 - radius, x1 + radius, y1 + radius, c);
      return;
    }

    MurphyIterator<RasterCanvas<PixelTraits>> murphy(*this, c);
    murphy.Wideline(x1, y1, x2, y2, thickness, 0);
    murphy.Wideline(x1, y1, x2, y2, thickness, 1);
  }

  void FillPolygon(const Point *points, unsigned n, color_type color) {
    assert(points != nullptr);

    if (n < 3)
      return;

    // Allocate temp array, only grow array
    polygon_buffer.GrowDiscard(n);
    int *const ints = polygon_buffer.begin();

    // Determine Y maxima
    int miny = points[0].y;
    int maxy = points[0].y;

    for (unsigned i = 1; i < n; i++) {
      if (points[i].y < miny)
        miny = points[i].y;
      else if (points[i].y > maxy)
        maxy = points[i].y;
    }

    // Draw, scanning y
    for (int y = miny; y <= maxy; y++) {
      unsigned n_ints = 0;
      for (unsigned i = 0; i < n; i++) {
        unsigned ind1, ind2;
        if (i == 0) {
          ind1 = n - 1;
          ind2 = 0;
        } else {
          ind1 = i - 1;
          ind2 = i;
        }

        int y1 = points[ind1].y;
        int y2 = points[ind2].y;
        int x1, x2;

        if (y1 < y2) {
          x1 = points[ind1].x;
          x2 = points[ind2].x;
        } else if (y1 > y2) {
          y2 = points[ind1].y;
          y1 = points[ind2].y;
          x2 = points[ind1].x;
          x1 = points[ind2].x;
        } else
          continue;

        if ( ((y >= y1) && (y < y2)) || ((y == maxy) && (y > y1) && (y <= y2)) ) {
          ints[n_ints++] = ((65536 * (y - y1)) / (y2 - y1)) * (x2 - x1) + (65536 * x1);
        }
      }

      std::sort(ints, ints + n_ints);

      for (unsigned i = 0; i < n_ints; i += 2) {
        int xa = ints[i] + 1;
        xa = (xa >> 16) + ((xa & 32768) >> 15);
        int xb = ints[i+1] - 1;
        xb = (xb >> 16) + ((xb & 32768) >> 15);
        DrawHLine(xa, xb, y, color);
      }
    }
  }

  void FillCircle(int x, int y, unsigned rad, color_type color) {
    // Special case for rad=0 - draw a point
    if (rad == 0) {
      DrawPixel(x, y, color);
      return;
    }

    /* Get circle and clipping boundary and test if bounding box of
       circle is visible */
    const int x2 = x + rad;
    if (x2 < 0)
      return;

    const int x1 = x - rad;
    if (x1 >= int(width))
      return;

    const int y2 = y + rad;
    if (y2 < 0)
      return;

    const int y1 = y - rad;
    if (y1 >= int(height))
      return;

    // draw
    int cx = 0, cy = rad;
    int ocx = 0xffff, ocy = 0xffff;
    int df = 1 - rad;
    int d_e = 3;
    int d_se = -2 * rad + 5;

    do {
      const int xpcx = x + cx, xpcy = x + cy;
      const int xmcx = x - cx, xmcy = x - cy;

      if (ocy != cy) {
        if (cy > 0) {
          const int ypcy = y + cy;
          const int ymcy = y - cy;
          DrawHLine(xmcx, xpcx, ypcy, color);
          DrawHLine(xmcx, xpcx, ymcy, color);
        } else {
          DrawHLine(xmcx, xpcx, y, color);
        }
        ocy = cy;
      }
      if (ocx != cx) {
        if (cx != cy) {
          if (cx > 0) {
            const int ypcx = y + cx;
            const int ymcx = y - cx;
            DrawHLine(xmcy, xpcy, ymcx, color);
            DrawHLine(xmcy, xpcy, ypcx, color);
          } else {
            DrawHLine(xmcy, xpcy, y, color);
          }
        }
        ocx = cx;
      }
      /*
       * Update
       */
      if (df < 0) {
        df += d_e;
        d_e += 2;
        d_se += 2;
      } else {
        df += d_se;
        d_e += 2;
        d_se += 4;
        cy--;
      }
      cx++;
    } while (cx <= cy);
  }
};

#endif