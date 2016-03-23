//
// "$Id$"
//
// Image drawing routines for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2016 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     http://www.fltk.org/COPYING.php
//
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//

// I hope a simple and portable method of drawing color and monochrome
// images.  To keep this simple, only a single storage type is
// supported: 8 bit unsigned data, byte order RGB, and pixels are
// stored packed into rows with the origin at the top-left.  It is
// possible to alter the size of pixels with the "delta" argument, to
// add alpha or other information per pixel.  It is also possible to
// change the origin and direction of the image data by messing with
// the "delta" and "linedelta", making them negative, though this may
// defeat some of the shortcuts in translating the image for X.


// A list of assumptions made about the X display:

// bits_per_pixel must be one of 8, 16, 24, 32.

// scanline_pad must be a power of 2 and greater or equal to 8.

// PsuedoColor visuals must have 8 bits_per_pixel (although the depth
// may be less than 8).  This is the only limitation that affects any
// modern X displays, you can't use 12 or 16 bit colormaps.

// The mask bits in TrueColor visuals for each color are
// contiguous and have at least one bit of each color.  This
// is not checked for.

// For 24 and 32 bit visuals there must be at least 8 bits of each color.

////////////////////////////////////////////////////////////////

#include <config.h>
#include "Fl_Xlib_Graphics_Driver.H"
#  include <FL/Fl.H>
#  include <FL/fl_draw.H>
#  include <FL/x.H>
#  include <FL/Fl_Image_Surface.H>
#  include "../../Fl_XColor.H"
#  include "../../flstring.h"
#include <X11/Xregion.h>

static XImage xi;	// template used to pass info to X
static int bytes_per_pixel;
static int scanline_add;
static int scanline_mask;

static void (*converter)(const uchar *from, uchar *to, int w, int delta);
static void (*mono_converter)(const uchar *from, uchar *to, int w, int delta);

static int dir;		// direction-alternator
static int ri,gi,bi;	// saved error-diffusion value

#  if USE_COLORMAP
////////////////////////////////////////////////////////////////
// 8-bit converter with error diffusion

static void color8_converter(const uchar *from, uchar *to, int w, int delta) {
  int r=ri, g=gi, b=bi;
  int d, td;
  if (dir) {
    dir = 0;
    from = from+(w-1)*delta;
    to = to+(w-1);
    d = -delta;
    td = -1;
  } else {
    dir = 1;
    d = delta;
    td = 1;
  }
  for (; w--; from += d, to += td) {
    r += from[0]; if (r < 0) r = 0; else if (r>255) r = 255;
    g += from[1]; if (g < 0) g = 0; else if (g>255) g = 255;
    b += from[2]; if (b < 0) b = 0; else if (b>255) b = 255;
    Fl_Color i = fl_color_cube(r*FL_NUM_RED/256,g*FL_NUM_GREEN/256,b*FL_NUM_BLUE/256);
    Fl_XColor& xmap = fl_xmap[0][i];
    if (!xmap.mapped) {if (!fl_redmask) fl_xpixel(r,g,b); else fl_xpixel(i);}
    r -= xmap.r;
    g -= xmap.g;
    b -= xmap.b;
    *to = uchar(xmap.pixel);
  }
  ri = r; gi = g; bi = b;
}

static void mono8_converter(const uchar *from, uchar *to, int w, int delta) {
  int r=ri, g=gi, b=bi;
  int d, td;
  if (dir) {
    dir = 0;
    from = from+(w-1)*delta;
    to = to+(w-1);
    d = -delta;
    td = -1;
  } else {
    dir = 1;
    d = delta;
    td = 1;
  }
  for (; w--; from += d, to += td) {
    r += from[0]; if (r < 0) r = 0; else if (r>255) r = 255;
    g += from[0]; if (g < 0) g = 0; else if (g>255) g = 255;
    b += from[0]; if (b < 0) b = 0; else if (b>255) b = 255;
    Fl_Color i = fl_color_cube(r*FL_NUM_RED/256,g*FL_NUM_GREEN/256,b*FL_NUM_BLUE/256);
    Fl_XColor& xmap = fl_xmap[0][i];
    if (!xmap.mapped) {if (!fl_redmask) fl_xpixel(r,g,b); else fl_xpixel(i);}
    r -= xmap.r;
    g -= xmap.g;
    b -= xmap.b;
    *to = uchar(xmap.pixel);
  }
  ri = r; gi = g; bi = b;
}

#  endif

////////////////////////////////////////////////////////////////
// 16 bit TrueColor converters with error diffusion
// Cray computers have no 16-bit type, so we use character pointers
// (which may be slow)

#  ifdef U16
#    define OUTTYPE U16
#    define OUTSIZE 1
#    define OUTASSIGN(v) *t = v
#  else
#    define OUTTYPE uchar
#    define OUTSIZE 2
#    define OUTASSIGN(v) int tt=v; t[0] = uchar(tt>>8); t[1] = uchar(tt)
#  endif

static void color16_converter(const uchar *from, uchar *to, int w, int delta) {
  OUTTYPE *t = (OUTTYPE *)to;
  int d, td;
  if (dir) {
    dir = 0;
    from = from+(w-1)*delta;
    t = t+(w-1)*OUTSIZE;
    d = -delta;
    td = -OUTSIZE;
  } else {
    dir = 1;
    d = delta;
    td = OUTSIZE;
  }
  int r=ri, g=gi, b=bi;
  for (; w--; from += d, t += td) {
    r = (r&~fl_redmask)  +from[0]; if (r>255) r = 255;
    g = (g&~fl_greenmask)+from[1]; if (g>255) g = 255;
    b = (b&~fl_bluemask) +from[2]; if (b>255) b = 255;
    OUTASSIGN((
      ((r&fl_redmask)<<fl_redshift)+
      ((g&fl_greenmask)<<fl_greenshift)+
      ((b&fl_bluemask)<<fl_blueshift)
      ) >> fl_extrashift);
  }
  ri = r; gi = g; bi = b;
}

static void mono16_converter(const uchar *from,uchar *to,int w, int delta) {
  OUTTYPE *t = (OUTTYPE *)to;
  int d, td;
  if (dir) {
    dir = 0;
    from = from+(w-1)*delta;
    t = t+(w-1)*OUTSIZE;
    d = -delta;
    td = -OUTSIZE;
  } else {
    dir = 1;
    d = delta;
    td = OUTSIZE;
  }
  uchar mask = fl_redmask & fl_greenmask & fl_bluemask;
  int r=ri;
  for (; w--; from += d, t += td) {
    r = (r&~mask) + *from; if (r > 255) r = 255;
    uchar m = r&mask;
    OUTASSIGN((
      (m<<fl_redshift)+
      (m<<fl_greenshift)+
      (m<<fl_blueshift)
      ) >> fl_extrashift);
  }
  ri = r;
}

// special-case the 5r6g5b layout used by XFree86:

static void c565_converter(const uchar *from, uchar *to, int w, int delta) {
  OUTTYPE *t = (OUTTYPE *)to;
  int d, td;
  if (dir) {
    dir = 0;
    from = from+(w-1)*delta;
    t = t+(w-1)*OUTSIZE;
    d = -delta;
    td = -OUTSIZE;
  } else {
    dir = 1;
    d = delta;
    td = OUTSIZE;
  }
  int r=ri, g=gi, b=bi;
  for (; w--; from += d, t += td) {
    r = (r&7)+from[0]; if (r>255) r = 255;
    g = (g&3)+from[1]; if (g>255) g = 255;
    b = (b&7)+from[2]; if (b>255) b = 255;
    OUTASSIGN(((r&0xf8)<<8) + ((g&0xfc)<<3) + (b>>3));
  }
  ri = r; gi = g; bi = b;
}

static void m565_converter(const uchar *from,uchar *to,int w, int delta) {
  OUTTYPE *t = (OUTTYPE *)to;
  int d, td;
  if (dir) {
    dir = 0;
    from = from+(w-1)*delta;
    t = t+(w-1)*OUTSIZE;
    d = -delta;
    td = -OUTSIZE;
  } else {
    dir = 1;
    d = delta;
    td = OUTSIZE;
  }
  int r=ri;
  for (; w--; from += d, t += td) {
    r = (r&7) + *from; if (r > 255) r = 255;
    OUTASSIGN((r>>3) * 0x841);
  }
  ri = r;
}

////////////////////////////////////////////////////////////////
// 24bit TrueColor converters:

static void rgb_converter(const uchar *from, uchar *to, int w, int delta) {
  int d = delta-3;
  for (; w--; from += d) {
    *to++ = *from++;
    *to++ = *from++;
    *to++ = *from++;
  }
}

static void bgr_converter(const uchar *from, uchar *to, int w, int delta) {
  for (; w--; from += delta) {
    uchar r = from[0];
    uchar g = from[1];
    *to++ = from[2];
    *to++ = g;
    *to++ = r;
  }
}

static void rrr_converter(const uchar *from, uchar *to, int w, int delta) {
  for (; w--; from += delta) {
    *to++ = *from;
    *to++ = *from;
    *to++ = *from;
  }
}

////////////////////////////////////////////////////////////////
// 32bit TrueColor converters on a 32 or 64-bit machine:

#  ifdef U64
#    define STORETYPE U64
#    if WORDS_BIGENDIAN
#      define INNARDS32(f) \
  U64 *t = (U64*)to; \
  int w1 = w/2; \
  for (; w1--; from += delta) {U64 i = f; from += delta; *t++ = (i<<32)|(f);} \
  if (w&1) *t++ = (U64)(f)<<32;
#    else
#      define INNARDS32(f) \
  U64 *t = (U64*)to; \
  int w1 = w/2; \
  for (; w1--; from += delta) {U64 i = f; from += delta; *t++ = ((U64)(f)<<32)|i;} \
  if (w&1) *t++ = (U64)(f);
#    endif
#  else
#    define STORETYPE U32
#    define INNARDS32(f) \
  U32 *t = (U32*)to; for (; w--; from += delta) *t++ = f
#  endif

static void rgbx_converter(const uchar *from, uchar *to, int w, int delta) {
  INNARDS32((unsigned(from[0])<<24)+(from[1]<<16)+(from[2]<<8));
}

static void xbgr_converter(const uchar *from, uchar *to, int w, int delta) {
  INNARDS32((from[0])+(from[1]<<8)+(from[2]<<16));
}

static void xrgb_converter(const uchar *from, uchar *to, int w, int delta) {
  INNARDS32((from[0]<<16)+(from[1]<<8)+(from[2]));
}

static void argb_premul_converter(const uchar *from, uchar *to, int w, int delta) {
  INNARDS32((unsigned(from[3]) << 24) +
             (((from[0] * from[3]) / 255) << 16) +
             (((from[1] * from[3]) / 255) << 8) +
             ((from[2] * from[3]) / 255));
}

static void bgrx_converter(const uchar *from, uchar *to, int w, int delta) {
  INNARDS32((from[0]<<8)+(from[1]<<16)+(unsigned(from[2])<<24));
}

static void rrrx_converter(const uchar *from, uchar *to, int w, int delta) {
  INNARDS32(unsigned(*from) * 0x1010100U);
}

static void xrrr_converter(const uchar *from, uchar *to, int w, int delta) {
  INNARDS32(*from * 0x10101U);
}

static void
color32_converter(const uchar *from, uchar *to, int w, int delta) {
  INNARDS32(
    (from[0]<<fl_redshift)+(from[1]<<fl_greenshift)+(from[2]<<fl_blueshift));
}

static void
mono32_converter(const uchar *from,uchar *to,int w, int delta) {
  INNARDS32(
    (*from << fl_redshift)+(*from << fl_greenshift)+(*from << fl_blueshift));
}

////////////////////////////////////////////////////////////////

static void figure_out_visual() {

  fl_xpixel(FL_BLACK); // setup fl_redmask, etc, in fl_color.cxx
  fl_xpixel(FL_WHITE); // also make sure white is allocated

  static XPixmapFormatValues *pfvlist;
  static int FL_NUM_pfv;
  if (!pfvlist) pfvlist = XListPixmapFormats(fl_display,&FL_NUM_pfv);
  XPixmapFormatValues *pfv;
  for (pfv = pfvlist; pfv < pfvlist+FL_NUM_pfv; pfv++)
    if (pfv->depth == fl_visual->depth) break;
  xi.format = ZPixmap;
  xi.byte_order = ImageByteOrder(fl_display);
//i.bitmap_unit = 8;
//i.bitmap_bit_order = MSBFirst;
//i.bitmap_pad = 8;
  xi.depth = fl_visual->depth;
  xi.bits_per_pixel = pfv->bits_per_pixel;

  if (xi.bits_per_pixel & 7) bytes_per_pixel = 0; // produce fatal error
  else bytes_per_pixel = xi.bits_per_pixel/8;

  unsigned int n = pfv->scanline_pad/8;
  if (pfv->scanline_pad & 7 || (n&(n-1)))
    Fl::fatal("Can't do scanline_pad of %d",pfv->scanline_pad);
  if (n < sizeof(STORETYPE)) n = sizeof(STORETYPE);
  scanline_add = n-1;
  scanline_mask = -n;

#  if USE_COLORMAP
  if (bytes_per_pixel == 1) {
    converter = color8_converter;
    mono_converter = mono8_converter;
    return;
  }
  if (!fl_visual->red_mask)
    Fl::fatal("Can't do %d bits_per_pixel colormap",xi.bits_per_pixel);
#  endif

  // otherwise it is a TrueColor visual:

  int rs = fl_redshift;
  int gs = fl_greenshift;
  int bs = fl_blueshift;

  switch (bytes_per_pixel) {

  case 2:
    // All 16-bit TrueColor visuals are supported on any machine with
    // 24 or more bits per integer.
#  ifdef U16
    xi.byte_order = WORDS_BIGENDIAN;
#  else
    xi.byte_order = 1;
#  endif
    if (rs == 11 && gs == 6 && bs == 0 && fl_extrashift == 3) {
      converter = c565_converter;
      mono_converter = m565_converter;
    } else {
      converter = color16_converter;
      mono_converter = mono16_converter;
    }
    break;

  case 3:
    if (xi.byte_order) {rs = 16-rs; gs = 16-gs; bs = 16-bs;}
    if (rs == 0 && gs == 8 && bs == 16) {
      converter = rgb_converter;
      mono_converter = rrr_converter;
    } else if (rs == 16 && gs == 8 && bs == 0) {
      converter = bgr_converter;
      mono_converter = rrr_converter;
    } else {
      Fl::fatal("Can't do arbitrary 24bit color");
    }
    break;

  case 4:
    if ((xi.byte_order!=0) != WORDS_BIGENDIAN)
      {rs = 24-rs; gs = 24-gs; bs = 24-bs;}
    if (rs == 0 && gs == 8 && bs == 16) {
      converter = xbgr_converter;
      mono_converter = xrrr_converter;
    } else if (rs == 24 && gs == 16 && bs == 8) {
      converter = rgbx_converter;
      mono_converter = rrrx_converter;
    } else if (rs == 8 && gs == 16 && bs == 24) {
      converter = bgrx_converter;
      mono_converter = rrrx_converter;
    } else if (rs == 16 && gs == 8 && bs == 0) {
      converter = xrgb_converter;
      mono_converter = xrrr_converter;
    } else {
      xi.byte_order = WORDS_BIGENDIAN;
      converter = color32_converter;
      mono_converter = mono32_converter;
    }
    break;

  default:
    Fl::fatal("Can't do %d bits_per_pixel",xi.bits_per_pixel);
  }

}

#  define MAXBUFFER 0x40000 // 256k

static void innards(const uchar *buf, int X, int Y, int W, int H,
		    int delta, int linedelta, int mono,
		    Fl_Draw_Image_Cb cb, void* userdata,
		    const bool alpha, GC gc)
{
  if (!linedelta) linedelta = W*abs(delta);

  int dx, dy, w, h;
  fl_clip_box(X,Y,W,H,dx,dy,w,h);
  if (w<=0 || h<=0) return;
  dx -= X;
  dy -= Y;

  if (!bytes_per_pixel) figure_out_visual();
  const unsigned oldbpp = bytes_per_pixel;
  static GC gc32 = None;
  xi.width = w;
  xi.height = h;

  void (*conv)(const uchar *from, uchar *to, int w, int delta) = converter;
  if (mono) conv = mono_converter;
  if (alpha) {
    // This flag states the destination format is ARGB32 (big-endian), pre-multiplied.
    bytes_per_pixel = 4;
    conv = argb_premul_converter;
    xi.depth = 32;
    xi.bits_per_pixel = 32;

    // Do we need a new GC?
    if (fl_visual->depth != 32) {
      if (gc32 == None)
        gc32 = XCreateGC(fl_display, fl_window, 0, NULL);
      gc = gc32;
    }
  }

  // See if the data is already in the right format.  Unfortunately
  // some 32-bit x servers (XFree86) care about the unknown 8 bits
  // and they must be zero.  I can't confirm this for user-supplied
  // data, so the 32-bit shortcut is disabled...
  // This can set bytes_per_line negative if image is bottom-to-top
  // I tested it on Linux, but it may fail on other Xlib implementations:
  if (buf && (
#  if 0	// set this to 1 to allow 32-bit shortcut
      delta == 4 &&
#    if WORDS_BIGENDIAN
      conv == rgbx_converter
#    else
      conv == xbgr_converter
#    endif
      ||
#  endif
      conv == rgb_converter && delta==3
      ) && !(linedelta&scanline_add)) {
    xi.data = (char *)(buf+delta*dx+linedelta*dy);
    xi.bytes_per_line = linedelta;

  } else {
    int linesize = ((w*bytes_per_pixel+scanline_add)&scanline_mask)/sizeof(STORETYPE);
    int blocking = h;
    static STORETYPE *buffer;	// our storage, always word aligned
    static long buffer_size;
    {int size = linesize*h;
    if (size > MAXBUFFER) {
      size = MAXBUFFER;
      blocking = MAXBUFFER/linesize;
    }
    if (size > buffer_size) {
      delete[] buffer;
      buffer_size = size;
      buffer = new STORETYPE[size];
    }}
    xi.data = (char *)buffer;
    xi.bytes_per_line = linesize*sizeof(STORETYPE);
    if (buf) {
      buf += delta*dx+linedelta*dy;
      for (int j=0; j<h; ) {
	STORETYPE *to = buffer;
	int k;
	for (k = 0; j<h && k<blocking; k++, j++) {
	  conv(buf, (uchar*)to, w, delta);
	  buf += linedelta;
	  to += linesize;
	}
	XPutImage(fl_display,fl_window,gc, &xi, 0, 0, X+dx, Y+dy+j-k, w, k);
      }
    } else {
      STORETYPE* linebuf = new STORETYPE[(W*delta+(sizeof(STORETYPE)-1))/sizeof(STORETYPE)];
      for (int j=0; j<h; ) {
	STORETYPE *to = buffer;
	int k;
	for (k = 0; j<h && k<blocking; k++, j++) {
	  cb(userdata, dx, dy+j, w, (uchar*)linebuf);
	  conv((uchar*)linebuf, (uchar*)to, w, delta);
	  to += linesize;
	}
	XPutImage(fl_display,fl_window,gc, &xi, 0, 0, X+dx, Y+dy+j-k, w, k);
      }

      delete[] linebuf;
    }
  }

  if (alpha) {
    bytes_per_pixel = oldbpp;
    xi.depth = fl_visual->depth;
    xi.bits_per_pixel = oldbpp * 8;
  }
}

void Fl_Xlib_Graphics_Driver::draw_image(const uchar* buf, int x, int y, int w, int h, int d, int l){

  const bool alpha = !!(abs(d) & FL_IMAGE_WITH_ALPHA);
  if (alpha) d ^= FL_IMAGE_WITH_ALPHA;
  const int mono = (d>-3 && d<3);

  innards(buf,x,y,w,h,d,l,mono,0,0,alpha,gc_);
}

void Fl_Xlib_Graphics_Driver::draw_image(Fl_Draw_Image_Cb cb, void* data,
		   int x, int y, int w, int h,int d) {

  const bool alpha = !!(abs(d) & FL_IMAGE_WITH_ALPHA);
  if (alpha) d ^= FL_IMAGE_WITH_ALPHA;
  const int mono = (d>-3 && d<3);

  innards(0,x,y,w,h,d,0,mono,cb,data,alpha,gc_);
}

void Fl_Xlib_Graphics_Driver::draw_image_mono(const uchar* buf, int x, int y, int w, int h, int d, int l){
  innards(buf,x,y,w,h,d,l,1,0,0,0,gc_);
}

void Fl_Xlib_Graphics_Driver::draw_image_mono(Fl_Draw_Image_Cb cb, void* data,
		   int x, int y, int w, int h,int d) {
  innards(0,x,y,w,h,d,0,1,cb,data,0,gc_);
}

void fl_rectf(int x, int y, int w, int h, uchar r, uchar g, uchar b) {
  if (fl_visual->depth > 16) {
    fl_color(r,g,b);
    fl_rectf(x,y,w,h);
  } else {
    uchar c[3];
    c[0] = r; c[1] = g; c[2] = b;
    innards(c,x,y,w,h,0,0,0,0,0,0,(GC)fl_graphics_driver->gc());
  }
}

Fl_Bitmask Fl_Xlib_Graphics_Driver::create_bitmask(int w, int h, const uchar *data) {
  return XCreateBitmapFromData(fl_display, fl_window, (const char *)data,
                               (w+7)&-8, h);
}

void Fl_Xlib_Graphics_Driver::delete_bitmask(Fl_Bitmask bm) {
  fl_delete_offscreen((Fl_Offscreen)bm);
}

void Fl_Xlib_Graphics_Driver::draw(Fl_Bitmap *bm, int XP, int YP, int WP, int HP, int cx, int cy) {
  int X, Y, W, H;
  if (bm->start(XP, YP, WP, HP, cx, cy, X, Y, W, H)) {
    return;
  }

  XSetStipple(fl_display, gc_, bm->id_);
  int ox = X-cx; if (ox < 0) ox += bm->w();
  int oy = Y-cy; if (oy < 0) oy += bm->h();
  XSetTSOrigin(fl_display, gc_, ox, oy);
  XSetFillStyle(fl_display, gc_, FillStippled);
  XFillRectangle(fl_display, fl_window, gc_, X, Y, W, H);
  XSetFillStyle(fl_display, gc_, FillSolid);
}


static int start(Fl_RGB_Image *img, int XP, int YP, int WP, int HP, int w, int h, int &cx, int &cy,
                 int &X, int &Y, int &W, int &H)
{
  // account for current clip region (faster on Irix):
  fl_clip_box(XP,YP,WP,HP,X,Y,W,H);
  cx += X-XP; cy += Y-YP;
  // clip the box down to the size of image, quit if empty:
  if (cx < 0) {W += cx; X -= cx; cx = 0;}
  if (cx+W > w) W = w-cx;
  if (W <= 0) return 1;
  if (cy < 0) {H += cy; Y -= cy; cy = 0;}
  if (cy+H > h) H = h-cy;
  if (H <= 0) return 1;
  return 0;
}


// Composite an image with alpha on systems that don't have accelerated
// alpha compositing...
static void alpha_blend(Fl_RGB_Image *img, int X, int Y, int W, int H, int cx, int cy) {
  int ld = img->ld();
  if (ld == 0) ld = img->w() * img->d();
  uchar *srcptr = (uchar*)img->array + cy * ld + cx * img->d();
  int srcskip = ld - img->d() * W;

  uchar *dst = new uchar[W * H * 3];
  uchar *dstptr = dst;

  fl_read_image(dst, X, Y, W, H, 0);

  uchar srcr, srcg, srcb, srca;
  uchar dstr, dstg, dstb, dsta;

  if (img->d() == 2) {
    // Composite grayscale + alpha over RGB...
    for (int y = H; y > 0; y--, srcptr+=srcskip)
      for (int x = W; x > 0; x--) {
        srcg = *srcptr++;
        srca = *srcptr++;

        dstr = dstptr[0];
        dstg = dstptr[1];
        dstb = dstptr[2];
        dsta = 255 - srca;

        *dstptr++ = (srcg * srca + dstr * dsta) >> 8;
        *dstptr++ = (srcg * srca + dstg * dsta) >> 8;
        *dstptr++ = (srcg * srca + dstb * dsta) >> 8;
      }
  } else {
    // Composite RGBA over RGB...
    for (int y = H; y > 0; y--, srcptr+=srcskip)
      for (int x = W; x > 0; x--) {
        srcr = *srcptr++;
        srcg = *srcptr++;
        srcb = *srcptr++;
        srca = *srcptr++;

        dstr = dstptr[0];
        dstg = dstptr[1];
        dstb = dstptr[2];
        dsta = 255 - srca;

        *dstptr++ = (srcr * srca + dstr * dsta) >> 8;
        *dstptr++ = (srcg * srca + dstg * dsta) >> 8;
        *dstptr++ = (srcb * srca + dstb * dsta) >> 8;
      }
  }

  fl_draw_image(dst, X, Y, W, H, 3, 0);

  delete[] dst;
}


void Fl_Xlib_Graphics_Driver::draw(Fl_RGB_Image *img, int XP, int YP, int WP, int HP, int cx, int cy) {
  int X, Y, W, H;
  // Don't draw an empty image...
  if (!img->d() || !img->array) {
    img->draw_empty(XP, YP);
    return;
  }
  if (start(img, XP, YP, WP, HP, img->w(), img->h(), cx, cy, X, Y, W, H)) {
    return;
  }
  if (!img->id_) {
    Fl_Image_Surface *surface = NULL;
    int depth = img->d();
    if (depth == 1 || depth == 3) {
      surface = new Fl_Image_Surface(img->w(), img->h());
    } else if (depth == 4 && fl_can_do_alpha_blending()) {
      Fl_Offscreen pixmap = XCreatePixmap(fl_display, RootWindow(fl_display, fl_screen), img->w(), img->h(), 32);
      surface = new Fl_Image_Surface(img->w(), img->h(), 0, pixmap);
      depth |= FL_IMAGE_WITH_ALPHA;
    }
    if (surface) {
      surface->set_current();
      fl_draw_image(img->array, 0, 0, img->w(), img->h(), depth, img->ld());
      surface->end_current();
      img->id_ = surface->get_offscreen_before_delete();
      delete surface;
    }
  }
  if (img->id_) {
    if (img->mask_) {
      // I can't figure out how to combine a mask with existing region,
      // so cut the image down to a clipped rectangle:
      int nx, ny; fl_clip_box(X,Y,W,H,nx,ny,W,H);
      cx += nx-X; X = nx;
      cy += ny-Y; Y = ny;
      // make X use the bitmap as a mask:
      XSetClipMask(fl_display, gc_, img->mask_);
      int ox = X-cx; if (ox < 0) ox += img->w();
      int oy = Y-cy; if (oy < 0) oy += img->h();
      XSetClipOrigin(fl_display, gc_, X-cx, Y-cy);
    }

    if (img->d() == 4 && fl_can_do_alpha_blending())
      copy_offscreen_with_alpha(X, Y, W, H, img->id_, cx, cy);
    else
      copy_offscreen(X, Y, W, H, img->id_, cx, cy);

    if (img->mask_) {
      // put the old clip region back
      XSetClipOrigin(fl_display, gc_, 0, 0);
      fl_restore_clip();
    }
  } else {
    // Composite image with alpha manually each time...
    alpha_blend(img, X, Y, W, H, cx, cy);
  }
}

void Fl_Xlib_Graphics_Driver::uncache(Fl_RGB_Image*, fl_uintptr_t &id_, fl_uintptr_t &mask_)
{
  if (id_) {
    XFreePixmap(fl_display, (Fl_Offscreen)id_);
    id_ = 0;
  }

  if (mask_) {
    fl_delete_bitmask((Fl_Bitmask)mask_);
    mask_ = 0;
  }
}

fl_uintptr_t Fl_Xlib_Graphics_Driver::cache(Fl_Bitmap*, int w, int h, const uchar *array) {
  return (fl_uintptr_t)create_bitmask(w, h, array);
}

void Fl_Xlib_Graphics_Driver::draw(Fl_Pixmap *pxm, int XP, int YP, int WP, int HP, int cx, int cy) {
  int X, Y, W, H;
  if (pxm->prepare(XP, YP, WP, HP, cx, cy, X, Y, W, H)) return;
  if (pxm->mask_) {
    // make X use the bitmap as a mask:
    XSetClipMask(fl_display, gc_, pxm->mask_);
    XSetClipOrigin(fl_display, gc_, X-cx, Y-cy);
    if (clip_region()) {
      // At this point, XYWH is the bounding box of the intersection between
      // the current clip region and the (portion of the) pixmap we have to draw.
      // The current clip region is often a rectangle. But, when a window with rounded
      // corners is moved above another window, expose events may create a complex clip
      // region made of several (e.g., 10) rectangles. We have to draw only in the clip
      // region, and also to mask out the transparent pixels of the image. This can't
      // be done in a single Xlib call for a multi-rectangle clip region. Thus, we
      // process each rectangle of the intersection between the clip region and XYWH.
      // See also STR #3206.
      Region r = XRectangleRegion(X,Y,W,H);
      XIntersectRegion(r, clip_region(), r);
      int X1, Y1, W1, H1;
      for (int i = 0; i < r->numRects; i++) {
        X1 = r->rects[i].x1;
        Y1 = r->rects[i].y1;
        W1 = r->rects[i].x2 - r->rects[i].x1;
        H1 = r->rects[i].y2 - r->rects[i].y1;
        copy_offscreen(X1, Y1, W1, H1, pxm->id_, cx + (X1 - X), cy + (Y1 - Y));
      }
      XDestroyRegion(r);
    } else {
      copy_offscreen(X, Y, W, H, pxm->id_, cx, cy);
    }
    // put the old clip region back
    XSetClipOrigin(fl_display, gc_, 0, 0);
    restore_clip();
  }
  else copy_offscreen(X, Y, W, H, pxm->id_, cx, cy);
}


fl_uintptr_t Fl_Xlib_Graphics_Driver::cache(Fl_Pixmap *img, int w, int h, const char *const*data) {
  Fl_Offscreen id;
  id = fl_create_offscreen(w, h);
  fl_begin_offscreen(id);
  uchar *bitmap = 0;
  fl_graphics_driver->mask_bitmap(&bitmap);
  fl_draw_pixmap(data, 0, 0, FL_BLACK);
  fl_graphics_driver->mask_bitmap(0);
  if (bitmap) {
    img->mask_ = (fl_uintptr_t)fl_create_bitmask(w, h, bitmap);
    delete[] bitmap;
  }
  fl_end_offscreen();
  return (fl_uintptr_t)id;
}


//
// End of "$Id$".
//
