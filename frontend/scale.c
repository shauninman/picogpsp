#include "common.h"
#include "gba_cc_lut.h"
#include "frontend/main.h"
#include "frontend/libpicofe/fonts.h"

#define Average(A, B) ((((A) & 0xF7DE) >> 1) + (((B) & 0xF7DE) >> 1) + ((A) & (B) & 0x0821))

/* Calculates the average of two pairs of RGB565 pixels. The result is, in
 * the lower bits, the average of both lower pixels, and in the upper bits,
 * the average of both upper pixels. */
#define Average32(A, B) ((((A) & 0xF7DEF7DE) >> 1) + (((B) & 0xF7DEF7DE) >> 1) + ((A) & (B) & 0x08210821))

/* Raises a pixel from the lower half to the upper half of a pair. */
#define Raise(N) ((N) << 16)

/* Extracts the upper pixel of a pair into the lower pixel of a pair. */
#define Hi(N) ((N) >> 16)

/* Extracts the lower pixel of a pair. */
#define Lo(N) ((N) & 0xFFFF)

/* Calculates the average of two RGB565 pixels. The source of the pixels is
 * the lower 16 bits of both parameters. The result is in the lower 16 bits.
 * The average is weighted so that the first pixel contributes 3/4 of its
 * color and the second pixel contributes 1/4. */
#define AverageQuarters3_1(A, B) ( (((A) & 0xF7DE) >> 1) + (((A) & 0xE79C) >> 2) + (((B) & 0xE79C) >> 2) + ((( (( ((A) & 0x1863) + ((A) & 0x0821) ) << 1) + ((B) & 0x1863) ) >> 2) & 0x1863) )

static inline void gba_upscale(uint16_t *to, uint16_t *from,
    uint32_t src_x, uint32_t src_y, uint32_t src_pitch, uint32_t dst_pitch)
{
  /* Before:
   *    a b c d e f
   *    g h i j k l
   *
   * After (multiple letters = average):
   *    a    ab   bc   c    d    de   ef   f
   *    ag   abgh bchi ci   dj   dejk efkl fl
   *    g    gh   hi   i    j    jk   kl   l
   */

  const uint32_t dst_x = src_x * 4 / 3;
  const uint32_t src_skip = src_pitch - src_x * sizeof(uint16_t),
                 dst_skip = dst_pitch - dst_x * sizeof(uint16_t);

  uint32_t x, y;

  for (y = 0; y < src_y; y += 2) {
    for (x = 0; x < src_x / 6; x++) {
      // -- Row 1 --
      // Read RGB565 elements in the source grid.
      // The notation is high_low (little-endian).
      uint32_t b_a = (*(uint32_t*) (from    )),
               d_c = (*(uint32_t*) (from + 2)),
               f_e = (*(uint32_t*) (from + 4));

      // Generate ab_a from b_a.
      *(uint32_t*) (to) = likely(Hi(b_a) == Lo(b_a))
        ? b_a
        : Lo(b_a) /* 'a' verbatim to low pixel */ |
          Raise(Average(Hi(b_a), Lo(b_a))) /* ba to high pixel */;

      // Generate c_bc from b_a and d_c.
      *(uint32_t*) (to + 2) = likely(Hi(b_a) == Lo(d_c))
        ? Lo(d_c) | Raise(Lo(d_c))
        : Raise(Lo(d_c)) /* 'c' verbatim to high pixel */ |
          Average(Lo(d_c), Hi(b_a)) /* bc to low pixel */;

      // Generate de_d from d_c and f_e.
      *(uint32_t*) (to + 4) = likely(Hi(d_c) == Lo(f_e))
        ? Lo(f_e) | Raise(Lo(f_e))
        : Hi(d_c) /* 'd' verbatim to low pixel */ |
          Raise(Average(Lo(f_e), Hi(d_c))) /* de to high pixel */;

      // Generate f_ef from f_e.
      *(uint32_t*) (to + 6) = likely(Hi(f_e) == Lo(f_e))
        ? f_e
        : Raise(Hi(f_e)) /* 'f' verbatim to high pixel */ |
          Average(Hi(f_e), Lo(f_e)) /* ef to low pixel */;

      if (likely(y + 1 < src_y))  // Is there a source row 2?
      {
        // -- Row 2 --
        uint32_t h_g = (*(uint32_t*) ((uint8_t*) from + src_pitch    )),
                 j_i = (*(uint32_t*) ((uint8_t*) from + src_pitch + 4)),
                 l_k = (*(uint32_t*) ((uint8_t*) from + src_pitch + 8));

        // Generate abgh_ag from b_a and h_g.
        uint32_t bh_ag = Average32(b_a, h_g);
        *(uint32_t*) ((uint8_t*) to + dst_pitch) = likely(Hi(bh_ag) == Lo(bh_ag))
          ? bh_ag
          : Lo(bh_ag) /* ag verbatim to low pixel */ |
            Raise(Average(Hi(bh_ag), Lo(bh_ag))) /* abgh to high pixel */;

        // Generate ci_bchi from b_a, d_c, h_g and j_i.
        uint32_t ci_bh =
          Hi(bh_ag) /* bh verbatim to low pixel */ |
          Raise(Average(Lo(d_c), Lo(j_i))) /* ci to high pixel */;
        *(uint32_t*) ((uint8_t*) to + dst_pitch + 4) = likely(Hi(ci_bh) == Lo(ci_bh))
          ? ci_bh
          : Raise(Hi(ci_bh)) /* ci verbatim to high pixel */ |
            Average(Hi(ci_bh), Lo(ci_bh)) /* bchi to low pixel */;

        // Generate fl_efkl from f_e and l_k.
        uint32_t fl_ek = Average32(f_e, l_k);
        *(uint32_t*) ((uint8_t*) to + dst_pitch + 12) = likely(Hi(fl_ek) == Lo(fl_ek))
          ? fl_ek
          : Raise(Hi(fl_ek)) /* fl verbatim to high pixel */ |
            Average(Hi(fl_ek), Lo(fl_ek)) /* efkl to low pixel */;

        // Generate dejk_dj from d_c, f_e, j_i and l_k.
        uint32_t ek_dj =
          Raise(Lo(fl_ek)) /* ek verbatim to high pixel */ |
          Average(Hi(d_c), Hi(j_i)) /* dj to low pixel */;
        *(uint32_t*) ((uint8_t*) to + dst_pitch + 8) = likely(Hi(ek_dj) == Lo(ek_dj))
          ? ek_dj
          : Lo(ek_dj) /* dj verbatim to low pixel */ |
            Raise(Average(Hi(ek_dj), Lo(ek_dj))) /* dejk to high pixel */;

        // -- Row 3 --
        // Generate gh_g from h_g.
        *(uint32_t*) ((uint8_t*) to + dst_pitch * 2) = likely(Hi(h_g) == Lo(h_g))
          ? h_g
          : Lo(h_g) /* 'g' verbatim to low pixel */ |
            Raise(Average(Hi(h_g), Lo(h_g))) /* gh to high pixel */;

        // Generate i_hi from g_h and j_i.
        *(uint32_t*) ((uint8_t*) to + dst_pitch * 2 + 4) = likely(Hi(h_g) == Lo(j_i))
          ? Lo(j_i) | Raise(Lo(j_i))
          : Raise(Lo(j_i)) /* 'i' verbatim to high pixel */ |
            Average(Lo(j_i), Hi(h_g)) /* hi to low pixel */;

        // Generate jk_j from j_i and l_k.
        *(uint32_t*) ((uint8_t*) to + dst_pitch * 2 + 8) = likely(Hi(j_i) == Lo(l_k))
          ? Lo(l_k) | Raise(Lo(l_k))
          : Hi(j_i) /* 'j' verbatim to low pixel */ |
            Raise(Average(Hi(j_i), Lo(l_k))) /* jk to high pixel */;

        // Generate l_kl from l_k.
        *(uint32_t*) ((uint8_t*) to + dst_pitch * 2 + 12) = likely(Hi(l_k) == Lo(l_k))
          ? l_k
          : Raise(Hi(l_k)) /* 'l' verbatim to high pixel */ |
            Average(Hi(l_k), Lo(l_k)) /* kl to low pixel */;
      }

      from += 6;
      to += 8;
    }

    // Skip past the waste at the end of the first line, if any,
    // then past 1 whole lines of source and 2 of destination.
    from = (uint16_t*) ((uint8_t*) from + src_skip +     src_pitch);
    to   = (uint16_t*) ((uint8_t*) to   + dst_skip + 2 * dst_pitch);
  }
}

static inline void gba_upscale_aspect(uint16_t *to, uint16_t *from,
    uint32_t src_x, uint32_t src_y, uint32_t src_pitch, uint32_t dst_pitch)
{
  /* Before:
   *    a b c d e f
   *    g h i j k l
   *    m n o p q r
   *
   * After (multiple letters = average):
   *    a    ab   bc   c    d    de   ef   f
   *    ag   abgh bchi ci   dj   dejk efkl fl
   *    gm   ghmn hino io   jp   jkpq klqr lr
   *    m    mn   no   o    p    pq   qr   r
   */

  const uint32_t dst_x = src_x * 4 / 3;
  const uint32_t src_skip = src_pitch - src_x * sizeof(uint16_t),
                 dst_skip = dst_pitch - dst_x * sizeof(uint16_t);

  uint32_t x, y;

  for (y = 0; y < src_y; y += 3) {
    for (x = 0; x < src_x / 6; x++) {
      // -- Row 1 --
      // Read RGB565 elements in the source grid.
      // The notation is high_low (little-endian).
      uint32_t b_a = (*(uint32_t*) (from    )),
               d_c = (*(uint32_t*) (from + 2)),
               f_e = (*(uint32_t*) (from + 4));

      // Generate ab_a from b_a.
      *(uint32_t*) (to) = likely(Hi(b_a) == Lo(b_a))
        ? b_a
        : Lo(b_a) /* 'a' verbatim to low pixel */ |
          Raise(Average(Hi(b_a), Lo(b_a))) /* ba to high pixel */;

      // Generate c_bc from b_a and d_c.
      *(uint32_t*) (to + 2) = likely(Hi(b_a) == Lo(d_c))
        ? Lo(d_c) | Raise(Lo(d_c))
        : Raise(Lo(d_c)) /* 'c' verbatim to high pixel */ |
          Average(Lo(d_c), Hi(b_a)) /* bc to low pixel */;

      // Generate de_d from d_c and f_e.
      *(uint32_t*) (to + 4) = likely(Hi(d_c) == Lo(f_e))
        ? Lo(f_e) | Raise(Lo(f_e))
        : Hi(d_c) /* 'd' verbatim to low pixel */ |
          Raise(Average(Lo(f_e), Hi(d_c))) /* de to high pixel */;

      // Generate f_ef from f_e.
      *(uint32_t*) (to + 6) = likely(Hi(f_e) == Lo(f_e))
        ? f_e
        : Raise(Hi(f_e)) /* 'f' verbatim to high pixel */ |
          Average(Hi(f_e), Lo(f_e)) /* ef to low pixel */;

      if (likely(y + 1 < src_y))  // Is there a source row 2?
      {
        // -- Row 2 --
        uint32_t h_g = (*(uint32_t*) ((uint8_t*) from + src_pitch    )),
                 j_i = (*(uint32_t*) ((uint8_t*) from + src_pitch + 4)),
                 l_k = (*(uint32_t*) ((uint8_t*) from + src_pitch + 8));

        // Generate abgh_ag from b_a and h_g.
        uint32_t bh_ag = Average32(b_a, h_g);
        *(uint32_t*) ((uint8_t*) to + dst_pitch) = likely(Hi(bh_ag) == Lo(bh_ag))
          ? bh_ag
          : Lo(bh_ag) /* ag verbatim to low pixel */ |
            Raise(Average(Hi(bh_ag), Lo(bh_ag))) /* abgh to high pixel */;

        // Generate ci_bchi from b_a, d_c, h_g and j_i.
        uint32_t ci_bh =
          Hi(bh_ag) /* bh verbatim to low pixel */ |
          Raise(Average(Lo(d_c), Lo(j_i))) /* ci to high pixel */;
        *(uint32_t*) ((uint8_t*) to + dst_pitch + 4) = likely(Hi(ci_bh) == Lo(ci_bh))
          ? ci_bh
          : Raise(Hi(ci_bh)) /* ci verbatim to high pixel */ |
            Average(Hi(ci_bh), Lo(ci_bh)) /* bchi to low pixel */;

        // Generate fl_efkl from f_e and l_k.
        uint32_t fl_ek = Average32(f_e, l_k);
        *(uint32_t*) ((uint8_t*) to + dst_pitch + 12) = likely(Hi(fl_ek) == Lo(fl_ek))
          ? fl_ek
          : Raise(Hi(fl_ek)) /* fl verbatim to high pixel */ |
            Average(Hi(fl_ek), Lo(fl_ek)) /* efkl to low pixel */;

        // Generate dejk_dj from d_c, f_e, j_i and l_k.
        uint32_t ek_dj =
          Raise(Lo(fl_ek)) /* ek verbatim to high pixel */ |
          Average(Hi(d_c), Hi(j_i)) /* dj to low pixel */;
        *(uint32_t*) ((uint8_t*) to + dst_pitch + 8) = likely(Hi(ek_dj) == Lo(ek_dj))
          ? ek_dj
          : Lo(ek_dj) /* dj verbatim to low pixel */ |
            Raise(Average(Hi(ek_dj), Lo(ek_dj))) /* dejk to high pixel */;

        if (likely(y + 2 < src_y))  // Is there a source row 3?
        {
          // -- Row 3 --
          uint32_t n_m = (*(uint32_t*) ((uint8_t*) from + src_pitch * 2    )),
                   p_o = (*(uint32_t*) ((uint8_t*) from + src_pitch * 2 + 4)),
                   r_q = (*(uint32_t*) ((uint8_t*) from + src_pitch * 2 + 8));

          // Generate ghmn_gm from h_g and n_m.
          uint32_t hn_gm = Average32(h_g, n_m);
          *(uint32_t*) ((uint8_t*) to + dst_pitch * 2) = likely(Hi(hn_gm) == Lo(hn_gm))
            ? hn_gm
            : Lo(hn_gm) /* gm verbatim to low pixel */ |
              Raise(Average(Hi(hn_gm), Lo(hn_gm))) /* ghmn to high pixel */;

          // Generate io_hino from h_g, j_i, n_m and p_o.
          uint32_t io_hn =
            Hi(hn_gm) /* hn verbatim to low pixel */ |
            Raise(Average(Lo(j_i), Lo(p_o))) /* io to high pixel */;
          *(uint32_t*) ((uint8_t*) to + dst_pitch * 2 + 4) = likely(Hi(io_hn) == Lo(io_hn))
            ? io_hn
            : Raise(Hi(io_hn)) /* io verbatim to high pixel */ |
              Average(Hi(io_hn), Lo(io_hn)) /* hino to low pixel */;

          // Generate lr_klqr from l_k and r_q.
          uint32_t lr_kq = Average32(l_k, r_q);
          *(uint32_t*) ((uint8_t*) to + dst_pitch * 2 + 12) = likely(Hi(lr_kq) == Lo(lr_kq))
            ? lr_kq
            : Raise(Hi(lr_kq)) /* lr verbatim to high pixel */ |
              Average(Hi(lr_kq), Lo(lr_kq)) /* klqr to low pixel */;

          // Generate jkpq_jp from j_i, l_k, p_o and r_q.
          uint32_t kq_jp =
            Raise(Lo(lr_kq)) /* kq verbatim to high pixel */ |
            Average(Hi(j_i), Hi(p_o)) /* jp to low pixel */;
          *(uint32_t*) ((uint8_t*) to + dst_pitch * 2 + 8) = likely(Hi(kq_jp) == Lo(kq_jp))
            ? kq_jp
            : Lo(kq_jp) /* jp verbatim to low pixel */ |
              Raise(Average(Hi(kq_jp), Lo(kq_jp))) /* jkpq to high pixel */;

          // -- Row 4 --
          // Generate mn_m from n_m.
          *(uint32_t*) ((uint8_t*) to + dst_pitch * 3) = likely(Hi(n_m) == Lo(n_m))
            ? n_m
            : Lo(n_m) /* 'm' verbatim to low pixel */ |
              Raise(Average(Hi(n_m), Lo(n_m))) /* mn to high pixel */;

          // Generate o_no from n_m and p_o.
          *(uint32_t*) ((uint8_t*) to + dst_pitch * 3 + 4) = likely(Hi(n_m) == Lo(p_o))
            ? Lo(p_o) | Raise(Lo(p_o))
            : Raise(Lo(p_o)) /* 'o' verbatim to high pixel */ |
              Average(Lo(p_o), Hi(n_m)) /* no to low pixel */;

          // Generate pq_p from p_o and r_q.
          *(uint32_t*) ((uint8_t*) to + dst_pitch * 3 + 8) = likely(Hi(p_o) == Lo(r_q))
            ? Lo(r_q) | Raise(Lo(r_q))
            : Hi(p_o) /* 'p' verbatim to low pixel */ |
              Raise(Average(Hi(p_o), Lo(r_q))) /* pq to high pixel */;

          // Generate r_qr from r_q.
          *(uint32_t*) ((uint8_t*) to + dst_pitch * 3 + 12) = likely(Hi(r_q) == Lo(r_q))
            ? r_q
            : Raise(Hi(r_q)) /* 'r' verbatim to high pixel */ |
              Average(Hi(r_q), Lo(r_q)) /* qr to low pixel */;
        }
      }

      from += 6;
      to += 8;
    }

    // Skip past the waste at the end of the first line, if any,
    // then past 2 whole lines of source and 3 of destination.
    from = (uint16_t*) ((uint8_t*) from + src_skip + 2 * src_pitch);
    to   = (uint16_t*) ((uint8_t*) to   + dst_skip + 3 * dst_pitch);
  }
}

static inline void gba_nofilter_noscale(uint16_t *dst, uint32_t dst_h, uint32_t dst_pitch, uint16_t *src) {
  int dst_x = ((dst_pitch - GBA_SCREEN_PITCH) / 2);
  int dst_y = ((dst_h - GBA_SCREEN_HEIGHT) / 2);

  for (int y = 0; y < GBA_SCREEN_HEIGHT; y++) {
    memcpy(dst + (dst_y + y) * dst_pitch + dst_x,
           src + y * GBA_SCREEN_PITCH,
           GBA_SCREEN_PITCH * sizeof(src[0]));
  }
}

/* drowsnug's nofilter upscaler, edited by eggs for smoothness */
#define AVERAGE16(c1, c2) (((c1) + (c2) + (((c1) ^ (c2)) & 0x0821))>>1)  //More accurate
static inline void gba_smooth_upscale(uint16_t *dst, uint16_t *src, int h)
{
  int Eh = 0;
  int dh = 0;
  int width = 240;
  int vf = 0;

  dst += ((240-h)/2) * 320;  // blank upper border. h=240(full) or h=214(aspect)

  int x, y;
  for (y = 0; y < h; y++)
  {
    int source = dh * width;
    for (x = 0; x < 320/4; x++)
    {
      register uint16_t a, b, c;

      a = src[source];
      b = src[source+1];
      c = src[source+2];

      if(vf == 1){
        a = AVERAGE16(a, src[source+width]);
        b = AVERAGE16(b, src[source+width+1]);
        c = AVERAGE16(c, src[source+width+2]);
      }

      *dst++ = a;
      *dst++ = AVERAGE16(AVERAGE16(a,b),b);
      *dst++ = AVERAGE16(b,AVERAGE16(b,c));
      *dst++ = c;
      source+=3;

    }
    Eh += 160;
    if(Eh >= h) {
      Eh -= h;
      dh++;
      vf = 0;
    }
    else
      vf = 1;
  }
}

#define EXTRACT(c, mask, offset) ((c >> offset) & mask)
#define BLENDCHANNEL(cl, cm, cr, mask, offset) ((((EXTRACT(cl, mask, offset) + 2 * EXTRACT(cm, mask, offset) + EXTRACT(cr, mask, offset)) >> 2) & mask) << offset)
#define BLENDB(cl, cm, cr) BLENDCHANNEL(cl, cm, cr, 0b0000000000011111, 0)
#define BLENDG(cl, cm, cr) BLENDCHANNEL(cl, cm, cr, 0b0000011111100000, 0)
#define BLENDR(cl, cm, cr) BLENDCHANNEL(cl, cm, cr, 0b0011111000000000, 2)
static inline void gba_smooth_subpx_upscale(uint16_t *dst, uint16_t *src, int h)
{
  int Eh = 0;
  int dh = 0;
  int width = 240;
  int vf = 0;

  dst += ((240-h)/2) * 320;  // blank upper border. h=240(full) or h=214(aspect)

  int x, y;
  for (y = 0; y < h; y++)
  {
    int source = dh * width;
    for (x = 0; x < 320/4; x++)
    {
      register uint16_t a, b, c;

      a = src[source];
      b = src[source+1];
      c = src[source+2];

      if(vf == 1){
        a = AVERAGE16(a, src[source+width]);
        b = AVERAGE16(b, src[source+width+1]);
        c = AVERAGE16(c, src[source+width+2]);
      }

      *dst++ = a;
      *dst++ = BLENDB(a, a, b) | BLENDG(a, b, b) | BLENDR(b, b, b);
      *dst++ = BLENDB(b, b, b) | BLENDG(b, b, c) | BLENDR(b, c, c);
      *dst++ = c;

      source+=3;
    }
    Eh += 160;
    if(Eh >= h) {
      Eh -= h;
      dh++;
      vf = 0;
    }
    else
      vf = 1;
  }
}

void video_clear_msg(uint16_t *dst, uint32_t h, uint32_t pitch)
{
  memset(dst + (h - 10) * pitch, 0, 10 * pitch * sizeof(uint16_t));
}

void video_print_msg(uint16_t *dst, uint32_t h, uint32_t pitch, char *msg)
{
  basic_text_out16_nf(dst, pitch, 2, h - 10, msg);
}

#define A32( ) (0xFF << 24)
#define R32(c) (((((c >> 11) & 0x1F) * 527 + 23 ) >> 6) << 16)
#define G32(c) (((((c >>  5) & 0x3F) * 259 + 33 ) >> 6) <<  8)
#define B32(c) (((((c      ) & 0x1F) * 527 + 23 ) >> 6)      )

void scale2x_8888(uint32_t* dst, uint32_t* src) {
    uint16_t* Src16 = (uint16_t*) src;
    uint32_t* Dst32 = (uint32_t*) dst;

    // There are 240 pixels horizontally, and 160 vertically.
    // Each pixel becomes 2x2 with an added grid pattern.

    uint8_t BlockX, BlockY;
    uint16_t* BlockSrc;
    uint32_t* BlockDst;
    for (BlockY = 0; BlockY < 160; BlockY++)
    {
        BlockSrc = Src16 + BlockY * 240 * 1;
        BlockDst = Dst32 + BlockY * 640 * 2;
        for (BlockX = 0; BlockX < 240; BlockX++)
        {
            // Before:          After:
            // (a)              (a)(a)
            //                  (a)(a)

			uint16_t c = *(BlockSrc);
			uint32_t c32 = A32() | R32(c) | G32(c) | B32(c);

            // -- Row 1 --
            *(BlockDst               ) = c32;
            *(BlockDst            + 1) = c32;

            // -- Row 2 --
            *(BlockDst + 640 *  1    ) = c32;
            *(BlockDst + 640 *  1 + 1) = c32;

            BlockSrc += 1;
            BlockDst += 2;
        }
    }
}

void scale2x_lcd_8888(uint32_t* dst, uint32_t* src) {
    uint16_t* Src16 = (uint16_t*) src;
    uint32_t* Dst32 = (uint32_t*) dst;

    // There are 240 pixels horizontally, and 160 vertically.
    // Each pixel becomes 2x2 with an added grid pattern.

    uint8_t BlockX, BlockY;
    uint16_t* BlockSrc;
    uint32_t* BlockDst;
	uint32_t k = 0xff000000;
    for (BlockY = 0; BlockY < 160; BlockY++)
    {
        BlockSrc = Src16 + BlockY * 240 * 1;
        BlockDst = Dst32 + BlockY * 640 * 2;
        for (BlockX = 0; BlockX < 240; BlockX++)
        {
            // Before:          After:
            // (a)              (a)(a)
            //                  (a)(a)

			uint16_t c = *(BlockSrc);
			uint32_t r = A32() | R32(c);
			uint32_t g = A32() | G32(c);
			uint32_t b = A32() | B32(c);
			
            // -- Row 1 --
            *(BlockDst          ) = r;
            *(BlockDst       + 1) = b;

            // -- Row 2 --
            *(BlockDst + 640    ) = g;
            *(BlockDst + 640 + 1) = k;

            BlockSrc += 1;
            BlockDst += 2;
        }
    }
}

void scale2x(uint16_t* dst, uint16_t* src)
{
    uint16_t* Src16 = (uint16_t*) src;
    uint16_t* Dst16 = (uint16_t*) dst;

    // There are 240 pixels horizontally, and 160 vertically.
    // Each pixel becomes 2x2

    uint8_t BlockX, BlockY;
    uint16_t* BlockSrc;
    uint16_t* BlockDst;
    for (BlockY = 0; BlockY < 160; BlockY++)
    {
        BlockSrc = Src16 + BlockY * 240 * 1;
        BlockDst = Dst16 + BlockY * 640 * 2;
        for (BlockX = 0; BlockX < 240; BlockX++)
        {
            // Before:          After:
            // (a)              (a)(a)
            //                  (a)(a)

            uint16_t  _1 = *(BlockSrc);

            // -- Row 1 --
            *(BlockDst               ) = _1;
            *(BlockDst            + 1) = _1;

            // -- Row 2 --
            *(BlockDst + 640 *  1    ) = _1;
            *(BlockDst + 640 *  1 + 1) = _1;

            BlockSrc += 1;
            BlockDst += 2;
        }
    }
}

void scale2x_lcd(uint16_t* dst, uint16_t* src) {
    uint16_t* Src16 = (uint16_t*) src;
    uint16_t* Dst16 = (uint16_t*) dst;

    // There are 240 pixels horizontally, and 160 vertically.
    // Each pixel becomes 2x2 with an added grid pattern.

    uint8_t BlockX, BlockY;
    uint16_t* BlockSrc;
    uint16_t* BlockDst;
	uint16_t k = 0x0000;
    for (BlockY = 0; BlockY < 160; BlockY++)
    {
        BlockSrc = Src16 + BlockY * 240 * 1;
        BlockDst = Dst16 + BlockY * 640 * 2;
        for (BlockX = 0; BlockX < 240; BlockX++)
        {
            // Before:          After:
            // (a)              (a)(a)
            //                  (a)(a)

			uint16_t  p = *(BlockSrc);
            uint16_t  r = (p & 0b1111100000000000);
            uint16_t  g = (p & 0b0000011111100000);
            uint16_t  b = (p & 0b0000000000011111);
			
            // -- Row 1 --
            *(BlockDst          ) = r;
            *(BlockDst       + 1) = b;

            // -- Row 2 --
            *(BlockDst + 640    ) = g;
            *(BlockDst + 640 + 1) = k;

            BlockSrc += 1;
            BlockDst += 2;
        }
    }
}

void video_scale(uint16_t *dst, uint32_t h, uint32_t pitch) {
    uint16_t* Dst16 = (uint16_t*)dst;
    Dst16 += ((480-320)/2 * 640) + (640-480)/2;

	uint16_t *gba_screen_pixels_buf = gba_screen_pixels;

	if (color_correct || lcd_blend)
		gba_screen_pixels_buf = gba_processed_pixels;

	switch (scaling_mode)
	{
	case SCALING_2X_LCD:
    	scale2x_lcd(Dst16, gba_screen_pixels_buf);
		break;
	case SCALING_2X:
	default:
	 	scale2x(Dst16, gba_screen_pixels_buf);
		break;
	}
}

/* Video post processing START */

/* Note: This code is intentionally W.E.T.
 * (Write Everything Twice). These functions
 * are performance critical, and we cannot
 * afford to do unnecessary comparisons/switches
 * inside the inner for loops */

static void video_post_process_cc(void)
{
   uint16_t *src = gba_screen_pixels;
   uint16_t *dst = gba_processed_pixels;
   size_t x, y;

   for (y = 0; y < GBA_SCREEN_HEIGHT; y++)
   {
      for (x = 0; x < GBA_SCREEN_PITCH; x++)
      {
         u16 src_color = *(src + x);

         /* Convert colour to RGB555 and perform lookup */
         *(dst + x) = *(gba_cc_lut + (((src_color & 0xFFC0) >> 1) | (src_color & 0x1F)));
      }

      src += GBA_SCREEN_PITCH;
      dst += GBA_SCREEN_PITCH;
   }
}

static void video_post_process_mix(void)
{
   uint16_t *src_curr = gba_screen_pixels;
   uint16_t *src_prev = gba_screen_pixels_prev;
   uint16_t *dst      = gba_processed_pixels;
   size_t x, y;

   for (y = 0; y < GBA_SCREEN_HEIGHT; y++)
   {
      for (x = 0; x < GBA_SCREEN_PITCH; x++)
      {
         /* Get colours from current + previous frames (RGB565) */
         uint16_t rgb_curr = *(src_curr + x);
         uint16_t rgb_prev = *(src_prev + x);

         /* Store colours for next frame */
         *(src_prev + x)   = rgb_curr;

         /* Mix colours
          * > "Mixing Packed RGB Pixels Efficiently"
          *   http://blargg.8bitalley.com/info/rgb_mixing.html */
         *(dst + x)        = (rgb_curr + rgb_prev + ((rgb_curr ^ rgb_prev) & 0x821)) >> 1;
      }

      src_curr += GBA_SCREEN_PITCH;
      src_prev += GBA_SCREEN_PITCH;
      dst      += GBA_SCREEN_PITCH;
   }
}

static void video_post_process_cc_mix(void)
{
   uint16_t *src_curr = gba_screen_pixels;
   uint16_t *src_prev = gba_screen_pixels_prev;
   uint16_t *dst      = gba_processed_pixels;
   size_t x, y;

   for (y = 0; y < GBA_SCREEN_HEIGHT; y++)
   {
      for (x = 0; x < GBA_SCREEN_PITCH; x++)
      {
         /* Get colours from current + previous frames (RGB565) */
         uint16_t rgb_curr = *(src_curr + x);
         uint16_t rgb_prev = *(src_prev + x);

         /* Store colours for next frame */
         *(src_prev + x)   = rgb_curr;

         /* Mix colours
          * > "Mixing Packed RGB Pixels Efficiently"
          *   http://blargg.8bitalley.com/info/rgb_mixing.html */
         uint16_t rgb_mix  = (rgb_curr + rgb_prev + ((rgb_curr ^ rgb_prev) & 0x821)) >> 1;

         /* Convert colour to RGB555 and perform lookup */
         *(dst + x) = *(gba_cc_lut + (((rgb_mix & 0xFFC0) >> 1) | (rgb_mix & 0x1F)));
      }

      src_curr += GBA_SCREEN_PITCH;
      src_prev += GBA_SCREEN_PITCH;
      dst      += GBA_SCREEN_PITCH;
   }
}

void video_post_process(void)
{
   size_t buf_size = GBA_SCREEN_PITCH * GBA_SCREEN_HEIGHT * sizeof(u16);

   /* If post processing is disabled, return
    * immediately */
   if (!color_correct && !lcd_blend)
      return;

   /* Initialise output buffer, if required */
   if (!gba_processed_pixels &&
       (color_correct || lcd_blend))
   {
      gba_processed_pixels = (u16*)malloc(buf_size);

      if (!gba_processed_pixels)
         return;

      memset(gba_processed_pixels, 0xFFFF, buf_size);
   }

   /* Initialise 'history' buffer, if required */
   if (!gba_screen_pixels_prev &&
       lcd_blend)
   {
      gba_screen_pixels_prev = (u16*)malloc(buf_size);

      if (!gba_screen_pixels_prev)
         return;

      memset(gba_screen_pixels_prev, 0xFFFF, buf_size);
   }

   /* Assign post processing function */
   if (color_correct && lcd_blend)
     video_post_process_cc_mix();
   else if (color_correct)
     video_post_process_cc();
   else if (lcd_blend)
     video_post_process_mix();
}

/* Video post processing END */
