#include <windows.h>
#include "framebuffer.h"

static int fb_get_format(const struct fb *fb)
{
  if (fb->bpp == 16)
    return FB_FORMAT_RGB565;

  if (fb->alpha_offset == 0 && fb->red_offset == 8)
    return FB_FORMAT_ARGB8888;

  if (fb->alpha_offset == 0 && fb->red_offset == 24 && fb->green_offset == 16 && fb->blue_offset == 8)
    return FB_FORMAT_RGBX8888;

  if (fb->alpha_offset == 0 && fb->blue_offset == 8)
    return FB_FORMAT_ABGR8888;

  if (fb->red_offset == 0)
    return FB_FORMAT_RGBA8888;

  if (fb->blue_offset == 0)
    return FB_FORMAT_BGRA8888;

  return FB_FORMAT_UNKNOWN;
}

template<typename TIN, typename TOUT>
void convert_colors(const TIN src, TOUT dst)
{
  dst->r = src->r;
  dst->g = src->g;
  dst->b = src->b;
}

template<typename TOUT>
void convert_rgb565(const struct rgb565 *src, TOUT dst)
{
  convert_colors(src, dst);
  dst->r <<= 3;
  dst->g <<= 2;
  dst->b <<= 3;
}

template<typename TIN, typename TOUT>
void convert_colors(int format, const TIN src, TOUT dst)
{
  switch (format) {
  case FB_FORMAT_RGB565:
    convert_rgb565(reinterpret_cast<struct rgb565 *>(src), dst);
    break;
  case FB_FORMAT_ARGB8888:
    convert_colors(reinterpret_cast<struct argb8888 *>(src), dst);
    break;
  case FB_FORMAT_ABGR8888:
    convert_colors(reinterpret_cast<struct abgr8888 *>(src), dst);
    break;
  case FB_FORMAT_BGRA8888:
    convert_colors(reinterpret_cast<struct bgra8888 *>(src), dst);
    break;
  case FB_FORMAT_RGBA8888:
    convert_colors(reinterpret_cast<struct rgba8888 *>(src), dst);
    break;
  }
}

int SaveToClipboard(const struct fb* fb)
{
  if (fb->width<=0 || fb->height<=0) return FALSE;
  auto format = fb_get_format(fb);
  if (format == FB_FORMAT_UNKNOWN) return FALSE;

  BITMAPINFO m_bmi;
  int m_bmi_size = sizeof(BITMAPINFOHEADER);
  memset(&m_bmi, 0, m_bmi_size);
  m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  m_bmi.bmiHeader.biWidth = fb->width;
  m_bmi.bmiHeader.biHeight = fb->height;
  m_bmi.bmiHeader.biPlanes = 1;
  m_bmi.bmiHeader.biBitCount = 24;
  m_bmi.bmiHeader.biCompression = BI_RGB;
  m_bmi.bmiHeader.biSizeImage = 0;
  m_bmi.bmiHeader.biXPelsPerMeter = 0;
  m_bmi.bmiHeader.biYPelsPerMeter = 0;
  m_bmi.bmiHeader.biClrImportant = 0;
  m_bmi.bmiHeader.biClrUsed = 0;

  if (!OpenClipboard(nullptr)) return FALSE;
  if (!EmptyClipboard()) return FALSE;

  HGLOBAL hResult;
  hResult = GlobalAlloc(GHND | GMEM_SHARE, m_bmi_size + fb->width * fb->height * 3);
  if (!hResult) return FALSE;

  auto dst = static_cast<char*>(GlobalLock(hResult));
  memcpy(dst, &m_bmi, m_bmi_size);
  dst += m_bmi_size;

  auto color = ::new bgr888;
  for (int y = static_cast<int>(fb->height) - 1; y >= 0; y--)
  {
    for (auto x = 0; x < static_cast<int>(fb->width); x++)
    {
      convert_colors(format, static_cast<char*>(fb->data) + (y * fb->width + x) * fb->bpp / 8, color);
      memcpy(dst, color, 3);
      dst += 3;
    }
  }
  delete color;

  GlobalUnlock(hResult);
  SetClipboardData(CF_DIB, hResult);
  CloseClipboard();
  GlobalFree(hResult);
  return TRUE;
}
