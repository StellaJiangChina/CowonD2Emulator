#include "cowond2.h"

HDC        hdcBmp;
HBITMAP    hBitmap;              // All drawing is cached here
HWND       hWnd;                 // hWnd of main window
USHORT     *hBmp;
BITMAPINFO *pBmpInfo;
USHORT     img[XSIZE*YSIZE];
DWORD      RunThread;
char       infoArr[sizeof(BITMAPINFO)+12];
char       fildat[0x1080];
int        xMouse=-1, yMouse;

void DrawBitmap(void)
{
  int y, x;
  USHORT r, g, b, val;
  FILE   *fil;

  if(lcd_buffer)
  {
    for(y=0; y<YSIZE; y++)
    {
      for(x=0; x<XSIZE; x++)
      {
/* for rockbox.bin display: 16bit color
        val = lcd_buffer[x+(YSIZE-y-1)*XSIZE];
        r = (val >> 11) & 0x1f;
        g = (val >>  6) & 0x1f;
        b = (val >>  0) & 0x1f;
*/
        b = (((ULONG*)lcd_buffer)[x + (YSIZE-y-1)*XSIZE] >>  3) & 31;
        g = (((ULONG*)lcd_buffer)[x + (YSIZE-y-1)*XSIZE] >> 11) & 31;
        r = (((ULONG*)lcd_buffer)[x + (YSIZE-y-1)*XSIZE] >> 19) & 31;

        img[x+y*XSIZE] = (r << 10) | (g << 5) | (b << 0);
      }
    }
    SetDIBitsToDevice(hdcBmp, 0, 0, XSIZE, YSIZE, 0, 0, 0, YSIZE, img, pBmpInfo, DIB_RGB_COLORS);
    InvalidateRect(hWnd, 0, 0);
  }
}

LRESULT PASCAL WndProc(HWND hThis, UINT message, WPARAM wParam, LPARAM lParam)
{
  PAINTSTRUCT    ps;
  static RECT    rect;
  static HDC     hdcTmp;

  int            x = LOWORD(lParam);
  int            y = HIWORD(lParam);

  switch(message)
  {
    case WM_CREATE:         GetWindowRect(hThis, &rect);
                            hdcTmp  = GetDC(hWnd); // Screen DC for reference
                            hBitmap = CreateCompatibleBitmap(hdcTmp, XSIZE, YSIZE);
                            hdcBmp  = CreateCompatibleDC(hdcTmp); // Create memory DC
                            pBmpInfo= (BITMAPINFO*)infoArr;
                            pBmpInfo->bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
                            pBmpInfo->bmiHeader.biWidth        = XSIZE;
                            pBmpInfo->bmiHeader.biHeight       = YSIZE;
                            pBmpInfo->bmiHeader.biPlanes       =    1;
                            pBmpInfo->bmiHeader.biBitCount     =   16;
                            pBmpInfo->bmiHeader.biCompression  = BI_RGB;
                            pBmpInfo->bmiHeader.biSizeImage    =    0;
                            pBmpInfo->bmiHeader.biXPelsPerMeter=    0;
                            pBmpInfo->bmiHeader.biYPelsPerMeter=    0;
                            pBmpInfo->bmiHeader.biClrUsed      =    0;
                            pBmpInfo->bmiHeader.biClrImportant =    0;
                            ReleaseDC(hWnd, hdcTmp);  // Done with this
                            SelectObject(hdcBmp, hBitmap);
                            break;

    case WM_LBUTTONDOWN:    xMouse = GET_X_LPARAM(lParam);
                            yMouse = GET_Y_LPARAM(lParam);
                            break;

    case WM_LBUTTONUP:      xMouse = -1; break;
   
    case WM_CHAR:           SendMessage(hWnd, WM_COMMAND, 0, 0); break;

    case WM_COMMAND:        if(wParam)
                              SetWindowText(hWnd, (char*)lParam);
                            DrawBitmap(); break;

    case WM_PAINT:          BeginPaint(hWnd, &ps);
                            BitBlt(ps.hdc, 0, 0, XSIZE, YSIZE, hdcBmp, 0, 0, SRCCOPY);
                            EndPaint(hWnd, &ps);
                            return 0;

    case WM_CLOSE:          if(RunThread == 1)
                              RunThread = 0;

                            if(RunThread == 2)
                              return DefWindowProc(hThis, message, wParam, lParam);
                            else
                              break;
                            

    case WM_DESTROY:        DeleteObject(hBitmap); // Delete bitmap and memory DC
                            DeleteDC(hdcBmp);
                            PostQuitMessage(0);  break;

    default:                return DefWindowProc(hThis, message, wParam, lParam);
  }

  return 0;
}

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int cmdShow)
{
  MSG       m;
  WNDCLASS  wc;
  DWORD     dw;

  wc.lpszClassName = "CowonD2";
  wc.hInstance     = hInstance;
  wc.lpfnWndProc   = WndProc;
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hIcon         = 0;
  wc.lpszMenuName  = 0;
  wc.hbrBackground = GetStockObject(BLACK_BRUSH);
  wc.style         = CS_HREDRAW | CS_VREDRAW;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  RegisterClass(&wc);

  hWnd = CreateWindow("CowonD2", "CowonD2", WS_VISIBLE | WS_CAPTION | WS_SYSMENU, 400, 0, XSIZE+6, YSIZE+32, NULL, NULL, hInstance, NULL);
  RunThread = 1;
  CreateThread(0, 0x2000, RunCode, (void*)&RunThread, 0, &dw);
//  RunCode(&RunThread); /* alternatively run the code direct (without display) */

  while(GetMessage(&m, NULL, 0, 0))
  {
    TranslateMessage(&m);
    DispatchMessage(&m);
  }

  return m.wParam;
}
