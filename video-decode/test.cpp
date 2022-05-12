#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include "resource.h"

const int marg=16;

unsigned char clamp(int v)
{
  return v < 0 ? 0 : v > 255 ? 255 : v;
}

void yuv_to_rgb(unsigned char *rgb, int y, int u, int v)
{
  y -= 16;
  u -= 128;
  v -= 128;
  rgb[3] = 0;
  rgb[2] = clamp((y*298 + v*409 + 128) / 256);
  rgb[1] = clamp((y*298 - u*100 - v*208 + 128) / 256);
  rgb[0] = clamp((y*298 + u*516 + 128) / 256);
}

INT_PTR CALLBACK wndproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  static HBITMAP rawbmp = NULL, adjbmp=NULL;
  static unsigned int srcw = 0, srch = 0;

  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      MFStartup(MF_VERSION);

      IMFSourceReader *reader = NULL;
      MFCreateSourceReaderFromURL(L"C:\\Users\\schwa\\Downloads\\test_smpte_420_800x600.mp4", NULL, &reader);
      reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
      reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);

      IMFMediaType *fmt = NULL;
      MFCreateMediaType(&fmt);
      fmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
      fmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YV12);
      reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, fmt);
      fmt->Release();

      reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &fmt);
      MFGetAttributeSize(fmt, MF_MT_FRAME_SIZE, &srcw, &srch);
      fmt->Release();

      IMFSample *sample = NULL;
      DWORD flags=0;
      INT64 readpos=0;
      IMFMediaBuffer *buffer = NULL;
      DWORD bufsz=0;
      reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, &readpos, &sample);
      sample->GetTotalLength(&bufsz);
      sample->ConvertToContiguousBuffer(&buffer);
      sample->Release();
      reader->Release();

      BITMAPINFO bi = {0};
      bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = srcw;
      bi.bmiHeader.biHeight = srch;
      bi.bmiHeader.biPlanes = 1;
      bi.bmiHeader.biBitCount = 32;
      bi.bmiHeader.biCompression = BI_RGB;
      unsigned char *raw = NULL, *adj=NULL;
      rawbmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&raw, NULL, 0);
      adjbmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&adj, NULL, 0);

      IMF2DBuffer *buffer2d = NULL;
      BYTE *bptr = NULL;
      LONG bstride = 0;
      buffer->QueryInterface(&buffer2d);
      buffer2d->Lock2D(&bptr, &bstride);

      // unexpected adjustment
      int reqsz = srch*bstride*3/2;
      int rowoffs = (bufsz-reqsz)/bstride*2/3;
      int uoffs = rowoffs*bstride;
      int voffs = (rowoffs+rowoffs/4)*bstride;

      unsigned char *rawptr = raw + srcw*(srch-1)*4;
      unsigned char *adjptr = adj + srcw*(srch-1)*4;
      unsigned char *yptr = bptr;
      unsigned char *uptr = bptr + srch*bstride*5/4;
      unsigned char *vptr = bptr + srch*bstride;

      for (unsigned int y=0; y < srch; ++y)
      {
        for (unsigned int x=0; x < srcw; ++x)
        {
          yuv_to_rgb(rawptr+x*4, yptr[x], uptr[x/2], vptr[x/2]);
          yuv_to_rgb(adjptr+x*4, yptr[x], uptr[x/2+offs], vptr[x/2+offs]);
        }

        rawptr -= srcw*4;
        adjptr -= srcw*4;
        yptr += bstride;
        if (y&1) uptr += bstride/2;
        if (y&1) vptr += bstride/2;
      }

      buffer2d->Unlock2D();
      buffer2d->Release();
      buffer->Release();

      SetWindowPos(hwndDlg, NULL, 0, 0, srcw+2*marg, 2*(srch+2*marg), SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
    }
    return 0;

    case WM_DESTROY:
    {
      DeleteObject(rawbmp);
      DeleteObject(adjbmp);
    }
    return 0;

    case WM_PAINT:
    {
      RECT r;
      GetClientRect(hwndDlg, &r);
      int w=r.right, h=r.bottom;

      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwndDlg, &ps);

      HDC srcdc = CreateCompatibleDC(dc);
      SelectObject(srcdc, rawbmp);
      BitBlt(dc, marg/2, marg/2, srcw, srch, srcdc, 0, 0, SRCCOPY);
      SelectObject(srcdc, adjbmp);
      BitBlt(dc, marg/2, (h+marg)/2, srcw, srch, srcdc, 0, 0, SRCCOPY);
      EndPaint(hwndDlg, &ps);
      ReleaseDC(hwndDlg, srcdc);
    }
    return 0;

    case WM_COMMAND:
      if (LOWORD(wParam) == IDCANCEL)
      {
        EndDialog(hwndDlg, 0);
      }
    return 0;
  }

  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
  DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG), GetDesktopWindow(), wndproc);
  return 0;
}
