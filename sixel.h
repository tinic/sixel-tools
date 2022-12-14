#ifndef _SIXEL_H_
#define _SIXEL_H_
#include <cstdint>
#include <cstring>
#include <cmath>
#include <array>

namespace sixel {

    template <typename T> struct rect {

        T x = 0;
        T y = 0;
        T w = 0;
        T h = 0;

        rect operator&(const rect &other) const {
            T nax = std::max(x, other.x);
            T nay = std::max(y, other.y);
            T nix = std::min(x + w, other.x + other.w);
            T niy = std::min(x + h, other.y + other.h);
            return {nax, nay, nix - nax, niy - nay};
        }

        rect &operator&=(const rect &other) {
            T nax = std::max(x, other.x);
            T nay = std::max(y, other.y);
            T nix = std::min(x + w, other.x + other.w);
            T niy = std::min(x + h, other.y + other.h);
            x = nax;
            y = nay;
            w = nix - nax;
            h = niy - nay;
            return *this;
        }

    };

    class format { 
    public:
        template <typename F> static constexpr void sixel_header(F &&charOut) {
            charOut(0x1b);
            charOut('P');
            charOut('0');
            charOut(';');
            charOut('0');
            charOut(';');
            charOut('0');
            charOut('q');
         }

        template <typename F> static constexpr void sixel_number(F &&charOut, uint16_t u) {
            if (u < 10) {
                charOut(static_cast<uint8_t>('0'+u));
            } else if (u < 100) {
                charOut(static_cast<uint8_t>('0'+(((u/10)%10))));
                charOut(static_cast<uint8_t>('0'+(u%10)));
            } else if (u < 1000) {
                charOut(static_cast<uint8_t>('0'+((u/100)%10)));
                charOut(static_cast<uint8_t>('0'+((u/10)%10)));
                charOut(static_cast<uint8_t>('0'+(u%10)));
            } else if (u < 10000) {
                charOut(static_cast<uint8_t>('0'+((u/1000)%10)));
                charOut(static_cast<uint8_t>('0'+((u/100)%10)));
                charOut(static_cast<uint8_t>('0'+((u/10)%10)));
                charOut(static_cast<uint8_t>('0'+(u%10)));
            } else {
                charOut(static_cast<uint8_t>('0'+((u/10000)%10)));
                charOut(static_cast<uint8_t>('0'+((u/1000)%10)));
                charOut(static_cast<uint8_t>('0'+((u/100)%10)));
                charOut(static_cast<uint8_t>('0'+((u/10)%10)));
                charOut(static_cast<uint8_t>('0'+(u%10)));
            }
         }

        template <typename F> static constexpr void sixel_color(F &&charOut, size_t index, uint32_t col) {
            charOut('#');
            sixel_number(charOut, index);
            charOut(';');
            charOut('2');
            charOut(';');
            for (size_t c = 0; c < 3; c++) {
                sixel_number(charOut, static_cast<uint16_t>((((col>>(8*(2-c)))&0xFF)*100)/255));
                charOut(';');
            }
         }

        template <typename F> static constexpr void sixel_end(F &&charOut) {
            charOut(0x1b);
            charOut('\\');
         }

        template <size_t W, size_t H, typename P, typename F, typename C> 
        static constexpr void sixel_image(const uint8_t *data, const P &palette, F &&charOut, const rect<int32_t> &_r, const C &collect6) {
            sixel_header(charOut);
            for (size_t c = 0; c < palette.size(); c++) {
                sixel_color(charOut, c, palette.data()[c]);
            }
            const auto r = _r & rect<int32_t>{0, 0, W, H};
            for (size_t y = r.y; y < r.y+r.h; y += 6) {
                for (size_t c = 0; c < palette.size(); c++) {
                    uint8_t test6 = 0;
                    for (size_t x = r.x; x < r.x+r.w; x++) {
                        test6 |= collect6(data, x, c, y);
                    }
                    if (!test6) {
                       continue;
                    }
                    if ( c != 0 ) {
                        charOut('$');
                    }
                    charOut('#');
                    sixel_number(charOut, static_cast<uint16_t>(c));
                    for (size_t x = r.x; x < r.x+r.w; x++) {
                        uint8_t bits6 = collect6(data, x, c, y);
                        uint16_t repeatCount = 0;
                        for (size_t xr = x + 1; xr < std::min(x + 255, W); xr++) {
                            if (bits6 == collect6(data, xr, c, y)) {
                                repeatCount++;
                                continue;
                            }
                            break;
                        }
                        if (repeatCount > 3) {
                            charOut('!');
                            sixel_number(charOut, repeatCount + 1);
                            x += repeatCount;
                        }
                        charOut('?' + bits6);
                    }
                }
                charOut('-');
            }
            sixel_end(charOut);
        }
    };

    template<size_t W, size_t H> class format_1bit : public format {
    public:
        static constexpr size_t bits_per_pixel = 1;
        static constexpr size_t bytes_per_line = (W * bits_per_pixel + 7) / 8;
        static constexpr size_t internal_height = ( ( H + 5 ) / 6 ) * 6;
        static constexpr size_t image_size = internal_height * bytes_per_line;
        static constexpr std::array<uint32_t, (1UL << bits_per_pixel)> palette = { 
            0x00000000,
            0x00ffffff};

        static constexpr void plot(std::array<uint8_t, image_size> &data, size_t x0, size_t y, uint32_t col) {
            col %= 1UL<<bits_per_pixel;
            size_t x8 = x0 / 8; x0 %= 8;
            uint8_t *yptr = &data.data()[y * bytes_per_line];
            yptr[x8] &= ~static_cast<uint8_t>(1UL << (7-x0));
            yptr[x8] |=  static_cast<uint8_t>(col << (7-x0));
        }

        static constexpr void span(std::array<uint8_t, image_size> &data, size_t xl0, size_t xr0, size_t y, uint32_t col) {
            col %= 1UL<<bits_per_pixel;
            size_t xl8 = xl0 / 8; xl0 %= 8;
            size_t xr8 = xr0 / 8; xr0 %= 8;
            size_t xs8 = xr8 - xl8;
            uint8_t c8 = static_cast<uint8_t>(col << 7 | col << 6 | col << 5 | col << 4 | col << 3 | col << 2 | col << 1 | col << 0);
            constexpr uint8_t ml[] = { 0b11111111, 0b01111111, 0b00111111, 0b00011111, 0b00001111, 0b00000111, 0b00000011, 0b00000001 };
            constexpr uint8_t mr[] = { 0b00000000, 0b10000000, 0b11000000, 0b11100000, 0b11110000, 0b11111000, 0b11111100, 0b11111110 };
            uint8_t *yptr = &data.data()[y * bytes_per_line];
            if (xs8 > 0) {
                yptr[xl8] &= ~ml[xl0]; 
                yptr[xl8] |=  ml[xl0] & c8; 
                for (size_t x = xl8+1; x < xr8; x++) {
                    yptr[x] = c8;
                }
                yptr[xr8] &= ~mr[xr0]; 
                yptr[xr8] |=  mr[xr0] & c8; 
            } else {
                yptr[xl8] &= ~(ml[xl0] & mr[xr0]); 
                yptr[xl8] |=  (ml[xl0] & mr[xr0] & c8); 
            }
        }

        template <typename F> static constexpr void sixel(std::array<uint8_t, image_size> &data, F &&charOut, const rect<int32_t> &r) {
            sixel_image<W, H>(data.data(), palette, charOut, r, [](const uint8_t *data, size_t x, size_t col, size_t y) {
                const uint8_t *ptr = &data[y * bytes_per_line + x / 8];
                size_t x8 = x % 8; 
                uint8_t out = 0;
                for (size_t y6 = 0; y6 < 6; y6++) {
                    out >>= 1;
                    if (( y + y6 ) < H) {
                        out |= ( ( ( (*ptr) >> (7 - x8)) & 1 ) == col ) ? (1UL<<5) : 0;
                    }
                    ptr += bytes_per_line;
                }
                return out;
            });
        }

    };

    template<size_t W, size_t H> class format_2bit : public format {
    public:
        static constexpr size_t bits_per_pixel = 2;
        static constexpr size_t bytes_per_line = (W * bits_per_pixel + 7) / 8;
        static constexpr size_t internal_height = ( ( H + 5 ) / 6 ) * 6;
        static constexpr size_t image_size = internal_height * bytes_per_line;
        static constexpr std::array<uint32_t, (1UL << bits_per_pixel)> palette = { 
            0x000000,
            0xffffff,
            0xff0000,
            0x00ff00};

        static constexpr void plot(std::array<uint8_t, image_size> &data, size_t x0, size_t y, uint32_t col) {
            col %= 1UL<<bits_per_pixel;
            size_t x4 = x0 / 4; x0 %= 4;
            uint8_t *yptr = &data.data()[y * bytes_per_line];
            yptr[x4] &= ~static_cast<uint8_t>(3UL << (6-x0*2));
            yptr[x4] |=  static_cast<uint8_t>(col << (6-x0*2));
        }

        static constexpr void span(std::array<uint8_t, image_size> &data, size_t xl0, size_t xr0, size_t y, uint32_t col) {
            col %= 1UL<<bits_per_pixel;
            size_t xl4 = xl0 / 4; xl0 %= 4;
            size_t xr4 = xr0 / 4; xr0 %= 4;
            size_t xs4 = xr4 - xl4;
            uint8_t c4 = static_cast<uint8_t>(col << 6 | col << 4 | col << 2 | col << 0);
            constexpr uint8_t ml[] = { 0b11111111, 0b00111111, 0b00001111, 0b00000011 };
            constexpr uint8_t mr[] = { 0b00000000, 0b11000000, 0b11110000, 0b11111100 };
            uint8_t *yptr = &data.data()[y * bytes_per_line];
            if (xs4 > 0) {
                yptr[xl4] &= ~ml[xl0]; 
                yptr[xl4] |=  ml[xl0] & c4; 
                for (size_t x = xl4+1; x < xr4; x++) {
                    yptr[+ x] = c4;
                }
                yptr[xr4] &= ~mr[xr0]; 
                yptr[xr4] |=  mr[xr0] & c4; 
            } else {
                yptr[xl4] &= ~(ml[xl0] & mr[xr0]); 
                yptr[xl4] |=  (ml[xl0] & mr[xr0] & c4); 
            }
        }

        template <typename F> static constexpr void sixel(std::array<uint8_t, image_size> &data, F &&charOut, const rect<int32_t> &r) {
            sixel_image<W, H>(data.data(), palette, charOut, r, [](const uint8_t *data, size_t x, size_t col, size_t y) {
                const uint8_t *ptr = &data[y * bytes_per_line + x / 4];
                size_t x4 = x % 4; 
                uint8_t out = 0;
                for (size_t y6 = 0; y6 < 6; y6++) {
                    out >>= 1;
                    if (( y + y6 ) < H) {
                        out |= ( ( ( (*ptr) >> (6 - x4*2)) & 3 ) == col ) ? (1UL<<5) : 0;
                    }
                    ptr += bytes_per_line;
                }
                return out;
            });
        }
    };

    template<size_t W, size_t H> class format_4bit : public format {
    public:
        static constexpr size_t bits_per_pixel = 4;
        static constexpr size_t bytes_per_line = (W * bits_per_pixel + 7) / 8;
        static constexpr size_t internal_height = ( ( H + 5 ) / 6 ) * 6;
        static constexpr size_t image_size = internal_height * bytes_per_line;
        static constexpr std::array<uint32_t, (1UL << bits_per_pixel)> palette = { 
            0x000000,
            0xffffff,
            0xff0000,
            0x00ff00,
            0x0000ff,
            0xffff00,
            0x00ffff,
            0xff00ff,
            0x333333,
            0x666666,
            0x999999,
            0xcccccc,
            0x7f0000,
            0x007f00,
            0x00007f,
            0x7f7f00};

        static constexpr void plot(std::array<uint8_t, image_size> &data, size_t x0, size_t y, uint32_t col) {
            col %= 1UL<<bits_per_pixel;
            size_t x2 = x0 / 2; x0 %= 2;
            uint8_t *yptr = &data.data()[y * bytes_per_line];
            yptr[x2] &= ~static_cast<uint8_t>(0xFUL << (4-x0*4));
            yptr[x2] |=  static_cast<uint8_t>(col   << (4-x0*4));
        }

        static constexpr void span(std::array<uint8_t, image_size> &data, size_t xl0, size_t xr0, size_t y, uint32_t col) {
            col %= 1UL<<bits_per_pixel;
            size_t xl2 = xl0 / 2; xl0 %= 2;
            size_t xr2 = xr0 / 2; xr0 %= 2;
            size_t xs2 = xr2 - xl2;
            uint8_t c2 = static_cast<uint8_t>(col << 4 | col << 0);
            constexpr uint8_t ml[] = { 0b11111111, 0b00001111 };
            constexpr uint8_t mr[] = { 0b00000000, 0b11110000 };
            uint8_t *yptr = &data.data()[y * bytes_per_line];
            if (xs2 > 0) {
                yptr[xl2] &= ~ml[xl0]; 
                yptr[xl2] |=  ml[xl0] & c2; 
                for (size_t x = xl2+1; x < xr2; x++) {
                    yptr[+ x] = c2;
                }
                yptr[xr2] &= ~mr[xr0]; 
                yptr[xr2] |=  mr[xr0] & c2; 
            } else {
                yptr[xl2] &= ~(ml[xl0] & mr[xr0]); 
                yptr[xl2] |=  (ml[xl0] & mr[xr0] & c2); 
            }
        }

        template <typename F> static constexpr void sixel(std::array<uint8_t, image_size> &data, F &&charOut, const rect<int32_t> &r) {
            sixel_image<W, H>(data.data(), palette, charOut, r, [](const uint8_t *data, size_t x, size_t col, size_t y) {
                const uint8_t *ptr = &data[y * bytes_per_line + x / 2];
                size_t x2 = x % 2; 
                uint8_t out = 0;
                for (size_t y6 = 0; y6 < 6; y6++) {
                    out >>= 1;
                    if (( y + y6 ) < H) {
                        out |= ( ( ( (*ptr) >> (4 - x2*4)) & 0xF ) == col ) ? (1UL<<5) : 0;
                    }
                    ptr += bytes_per_line;
                }
                return out;
            });
        }
    };

    template<template<size_t, size_t> class T, size_t W, size_t H> class image {

        static_assert(sizeof(W)>=sizeof(uint32_t));
        static_assert(sizeof(H)>=sizeof(uint32_t));

        static_assert(W<=16384 && H<=16384);

    public:

        size_t size() const {
            return T<W, H>::image_size;
        }

        size_t width() const { 
            return W; 
        }

        size_t height() const { 
            return H; 
        }

        void clear() {
            memset(data.data(),0,data.size());
        } 

        void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t col, uint32_t width = 1) {
            int32_t steep = abs(y1 - y0) > abs(x1 - x0);

            if (steep) {
                x0 ^= y0;
                y0 ^= x0;
                x0 ^= y0;
                x1 ^= y1;
                y1 ^= x1;
                x1 ^= y1;
            }

            if (x0 > x1) {
                x0 ^= x1;
                x1 ^= x0;
                x0 ^= x1;
                y0 ^= y1;
                y1 ^= y0;
                y0 ^= y1;
            }

            int32_t dx, dy;
            dx = x1 - x0;
            dy = abs(y1 - y0);

            int32_t err = dx / 2;
            int32_t ystep;
            if (y0 < y1) {
                ystep = 1;
            } else {
                ystep = -1;
            }

            if (width == 1) {
                for (; x0 <= x1; x0++) {
                    if (steep) {
                        plot(y0, x0, col);
                    } else {
                        plot(x0, y0, col);
                    }
                    err -= dy;
                    if (err < 0) {
                        y0 += ystep;
                        err += dx;
                    }
                }
            } else if (width > 1) {
                for (; x0 <= x1; x0++) {
                    if (steep) {
                        fillcircle(y0, x0, (width+1)/2, col);
                    } else {
                        fillcircle(x0, y0, (width+1)/2, col);
                    }
                    err -= dy;
                    if (err < 0) {
                        y0 += ystep;
                        err += dx;
                    }
                }
            }
        }

        void fillrect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t col) {
            h+=y;
            for (;y<h;y++) {
                span(x,w,y,col);
            }
        }

        void fillcircle(int32_t x, int32_t y, int32_t r, uint32_t col) {
            span(x - r, 2 * r + 1, y, col);
            fillarc(x, y, r, 3, 0, col);
        }

        template <typename F> constexpr void sixel(F &&charOut) {
            T<W, H>::sixel(data, charOut, {0, 0, W, H});
        }

        template <typename F> constexpr void sixel(F &&charOut, const rect<int32_t> &r) {
            T<W, H>::sixel(data, charOut, r);
        }

    private:

        void plot(int32_t x, int32_t y, uint32_t col) {
            size_t _x = static_cast<size_t>(x); _x %= W;
            size_t _y = static_cast<size_t>(y); _y %= H;
            T<W, H>::plot(data, _x, _y, col);
        }

        void span(int32_t x,  int32_t w, int32_t y, uint32_t col) {
            while(y<0) { y+=H; }
            while(x<0) { x+=W; }
            size_t _xl = static_cast<size_t>(x  ); _xl %= W;
            size_t _xr = static_cast<size_t>(x+w); _xr %= W;
            size_t _y  = static_cast<size_t>(y  ); _y  %= H;
            if ( _xl + w < W ) {
                T<W, H>::span(data, _xl, _xr, _y, col);
            } else { 
                T<W, H>::span(data, _xl,   W, _y, col);
                T<W, H>::span(data,   0, _xr, _y, col);
            }
        }

        void fillarc(int32_t x0, int32_t y0, int32_t r, uint8_t corners, int32_t delta, uint32_t col) {
            int32_t f = 1 - r;
            int32_t ddx = -2 * r;
            int32_t ddy = 1;
            int32_t x = r;
            int32_t y = 0;
            int32_t px = x;
            int32_t py = y;
            delta++;
            while (y < x) {
                if (f >= 0) {
                    x--;
                    ddx += 2;
                    f += ddx;
                }
                ddy += 2;
                f += ddy;
                if (++y < (x + 1)) {
                    if (corners & 1)
                        span(x0 - x, 2 * x + delta, y0 + y, col);
                    if (corners & 2)
                        span(x0 - x, 2 * x + delta, y0 - y, col);
                }
                if (x != px) {
                    if (corners & 1)
                        span(x0 - py, 2 * py + delta, y0 + px, col);
                    if (corners & 2)
                        span(x0 - py, 2 * py + delta, y0 - px, col);
                    px = x;
                }
                py = y;
            }
        }

        std::array<uint8_t, T<W, H>::image_size> data;
        T<W, H> format;
    };

}
#endif  // #ifndef _SIXEL_H_
