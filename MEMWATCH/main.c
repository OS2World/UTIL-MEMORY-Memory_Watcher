//
//Free memory including virtual memory is
//[TotalAvailMem] - Free space on drive - SwapPathMinFree
//
//Total memory
//[TotPhysMem] + SwapFileSize
//
//----------------------------------------------------
// Version  Who   When        What
// 1.0.00   JUL   09 jan 1998 Initial version
// 1.1.00   JUL   24 apr 1998 Added file
// 1.2.00   JUL   19 mar 1999 
//----------------------------------------------------

#define INCL_WIN
#define INCL_GPI
//#define INCL_DOSMISC
//#define INCL_DOSMEMMGR
#define INCL_DOS

//#include "e:\watcom\h\os21x\bsedos.h"
#include <os2.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "..\\common\\about.h"

#define FONT_SIZE 15
#define INFO_HEIGHT 15*5

MRESULT EXPENTRY ClientWndProc (HWND, ULONG, MPARAM, MPARAM);
ULONG GetFileSize (char *pszFile);
ULONG getFreeMemory (BOOL);
ULONG getUsedMemory (BOOL);
ULONG getMaxMemory (void);
ULONG queryDiskSpace (ULONG ulDriveNumber);
void hideTitleBar (void);
void displayTitleBar (void);


void typeInfo (void);
void insertData (void);
void init ();
void drawGraph (HWND hWnd, HPS hps);
void clearBackGround (HPS hps, long x, long y, long cx, long cy, ULONG color);
void drawInfo (HWND hWnd, HPS hps);

HAB   hab;
HWND  hWndFrame,
      hWndClient,
      hWndMenu;
char  szTitle[64];

#define MAX_MEM_SLOT 125
ULONG gulBuffPointer;
ULONG gulVirtualMemBuff[MAX_MEM_SLOT];
ULONG gulPhysicMemBuff[MAX_MEM_SLOT];
ULONG gulPhysMem;                //RAM installed in the PC
ULONG gulSwapFileMin;            //Setting in Config.sys MinFree parameter
ULONG gulSwapDrive;              //The drive number where the swapper.dat is located
char gszSwapFileName[1024];      //Path to swap file

//--- Options ---
void writeIniSettings (HWND);
void readIniSettingsDlg (HWND hWnd);
#define INI_FILE "memwatch.ini"
#define INI_SECTION "options"
BOOL  gbShowGrid;
ULONG gulRefrashRate;
BOOL  gbShowPhysicalMemory;
BOOL  gbShowPhysicalMemoryLimit;
BOOL  gbShowDetail;
BOOL  gbShortText;

#define SLOW_RATE   2000UL
#define NORMAL_RATE 1000UL
#define FASTE_RATE   500UL
ULONG gulTimer;
ULONG gulFrameStyle = FCF_TITLEBAR | FCF_SYSMENU | FCF_ICON | FCF_SIZEBORDER |
                      FCF_MINMAX | FCF_MENU | FCF_SHELLPOSITION | FCF_TASKLIST;
CHAR    gszIniFileWithPath[2048];
//*********************************************************************
//*********************************************************************
int main (int argc, char *argv[])
{
   HMQ   hmq;
   QMSG  qmsg;
   char szClientClass[] = "CLIENT";
   LONG  l;
   SWP   swp;
   HINI  hini;

   if ((argc >= 2) && (argv[1][1] == 's'))
   {
      init ();
      typeInfo ();
      return (0);
   }

   strcpy (gszIniFileWithPath, argv[0]);
   l = strlen (gszIniFileWithPath) - 1;
   while (gszIniFileWithPath[l] != '\\')
      l--;

   gszIniFileWithPath[l+1] = 0;
   strcat (gszIniFileWithPath, INI_FILE);
   strlwr (gszIniFileWithPath);

   hab = WinInitialize (0);
   hmq = WinCreateMsgQueue (hab, 0);

   WinRegisterClass (hab, szClientClass, (PFNWP)ClientWndProc, 0, 0);
   WinLoadString (hab, 0, ID_APPNAME, sizeof(szTitle), szTitle);

   hWndFrame = WinCreateStdWindow (HWND_DESKTOP, 0, //WS_VISIBLE,
                  &gulFrameStyle, szClientClass, szTitle, 0, 0,
                  ID_APPNAME, &hWndClient);

   hini = PrfOpenProfile (hab, gszIniFileWithPath);
   swp.cx = PrfQueryProfileInt (hini, INI_SECTION, (PSZ)"cx", 50);
   swp.cy = PrfQueryProfileInt (hini, INI_SECTION, (PSZ)"cy", 50);
   swp.x  = PrfQueryProfileInt (hini, INI_SECTION, (PSZ)"x" , 50);
   swp.y  = PrfQueryProfileInt (hini, INI_SECTION, (PSZ)"y" , 50);
   PrfCloseProfile (hini);
   WinSetWindowPos (hWndFrame, HWND_TOP, swp.x, swp.y, swp.cx, swp.cy,
                        SWP_SHOW | SWP_MOVE | SWP_SIZE | SWP_ZORDER);

   //Is the first param -s?
   //Then user want staus out on stdio, close the app when done

   while (WinGetMsg (hab, &qmsg, 0, 0, 0))
      WinDispatchMsg (hab, &qmsg);

   WinDestroyWindow (hWndFrame);
   WinDestroyMsgQueue (hmq);
   WinTerminate (hab);
   return (0);
}

//*********************************************************************
//*********************************************************************
MRESULT EXPENTRY ClientWndProc (HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   HPS      hps;
   BOOL     bHandled = TRUE;
   MRESULT  mReturn = 0;
   RECTL    rclUpdateRegion;

   switch (msg)
   {
      case WM_CREATE:
         hWndMenu = WinWindowFromID (WinQueryWindow(hWnd, QW_PARENT), FID_MENU);
         gulTimer = WinStartTimer (hab, hWnd, 0, NORMAL_RATE);
         init ();
         readIniSettingsDlg (hWnd);
      break;

      case WM_OPEN:
         //gulRefrashRate = IDM_NORMAL;
         //gbShowPhysicalMemory = TRUE;
         //gbShowPhysicalMemoryLimit = TRUE;
         //gbShowDetail = TRUE;
      break;

      case WM_CLOSE:
         WinStopTimer (hab, hWnd, gulTimer);
         bHandled = FALSE;
         writeIniSettings (hWnd);
      break;
      
      case WM_TIMER:
      {
         hps = WinGetPS (hWnd);
         insertData ();
         //drawGraph (hWnd, hps);
         drawGraph (hWndClient, hps);
         WinReleasePS (hps);
      }  
      break;
      
      case WM_PAINT:
         hps = WinBeginPaint (hWnd, 0, 0);
         drawGraph (hWnd, hps);
         WinEndPaint (hps);
      break;
      
      case WM_ERASEBACKGROUND:
      {
         long x, y, cx, cy;
         RECTL *pr;

         pr = (PRECTL)mp2;
         x = pr->xLeft;
         y = pr->yBottom;
         cx= pr->xRight - x;
         cy= pr->yTop - y;
         clearBackGround ((HPS)mp1, x, y, cx, cy, CLR_BLACK);

         mReturn = MRFROMLONG(1L);
      }
      break;

      case WM_BUTTON1DBLCLK:
         displayTitleBar ();
      break;
      
      case WM_COMMAND:
         switch (LOUSHORT(mp1))
         {
            case IDM_ABOUT:
               DisplayAbout (hWnd, szTitle);
            break;
            
            case IDM_EXIT:
               WinPostMsg (hWnd, WM_CLOSE, MPVOID, MPVOID);
            break;

            case IDM_GRID:
 
               if (gbShowGrid)   gbShowGrid = FALSE;
               else              gbShowGrid = TRUE;
               WinCheckMenuItem (hWndMenu, IDM_GRID, gbShowGrid);
               WinQueryWindowRect (hWnd, &rclUpdateRegion);
               WinInvalidateRect(hWnd, &rclUpdateRegion, FALSE);
            break;
            
            case IDM_FAST:
            case IDM_NORMAL:
            case IDM_SLOW:
            case IDM_PAUSE:
               
               WinStopTimer (hab, hWnd, gulTimer);

               WinCheckMenuItem (hWndMenu, gulRefrashRate, FALSE);
               gulRefrashRate = LOUSHORT(mp1);
               WinCheckMenuItem (hWndMenu, LOUSHORT(mp1), TRUE);
               
               if (LOUSHORT(mp1) == IDM_FAST)
                  gulTimer = WinStartTimer (hab, hWnd, 0, FASTE_RATE);
               else if (LOUSHORT(mp1) == IDM_NORMAL)
                  gulTimer = WinStartTimer (hab, hWnd, 0, NORMAL_RATE);
               else if (LOUSHORT(mp1) == IDM_SLOW)
                  gulTimer = WinStartTimer (hab, hWnd, 0, SLOW_RATE);
               else
                  gulTimer = 0;
               
            break;

            case IDM_PHYSICAL_MEM:
               if (gbShowPhysicalMemory)
                  gbShowPhysicalMemory = FALSE;
               else
                  gbShowPhysicalMemory = TRUE;
                  
               WinCheckMenuItem (hWndMenu, IDM_PHYSICAL_MEM, gbShowPhysicalMemory);
               WinQueryWindowRect (hWnd, &rclUpdateRegion);
               WinInvalidateRect(hWnd, &rclUpdateRegion, FALSE);
            break;
            
            case IDM_PHYSICAL_MEM_LIMIT:
               if (gbShowPhysicalMemoryLimit)
                  gbShowPhysicalMemoryLimit = FALSE;
               else
                  gbShowPhysicalMemoryLimit = TRUE;
                  
               WinCheckMenuItem (hWndMenu, IDM_PHYSICAL_MEM_LIMIT, gbShowPhysicalMemoryLimit);
               WinQueryWindowRect (hWnd, &rclUpdateRegion);
               WinInvalidateRect(hWnd, &rclUpdateRegion, FALSE);
            break;
            case IDM_DETAIL:
               if (gbShowDetail)
                  gbShowDetail = FALSE;
               else
                  gbShowDetail = TRUE;
               WinCheckMenuItem (hWndMenu, IDM_DETAIL, gbShowDetail);
               WinQueryWindowRect (hWnd, &rclUpdateRegion);
               WinInvalidateRect(hWnd, &rclUpdateRegion, FALSE);
            break;
            case IDM_SHORTTEXT:
               if (gbShortText)
                  gbShortText = FALSE;
               else
                  gbShortText = TRUE;
               WinCheckMenuItem (hWndMenu, IDM_SHORTTEXT, gbShortText);
               WinQueryWindowRect (hWnd, &rclUpdateRegion);
               WinInvalidateRect(hWnd, &rclUpdateRegion, FALSE);
            break;
            case IDMTITLEBAR:
               hideTitleBar ();
            break;
         }
         
      default:
         bHandled = FALSE;
      break;
   }

   if (!bHandled)
      mReturn = WinDefWindowProc (hWnd, msg, mp1, mp2);

   return mReturn;      
}

//*********************************************************************
//   Fills the buffer with memory amount values
//*********************************************************************
void insertData (void)
{
   gulPhysicMemBuff [gulBuffPointer] = getUsedMemory (TRUE);
   gulVirtualMemBuff[gulBuffPointer] = getUsedMemory (FALSE);
   gulBuffPointer++;
   if (gulBuffPointer == MAX_MEM_SLOT) gulBuffPointer = 0;
}


//*********************************************************************
//*********************************************************************
void clearBackGround (HPS hps, long x, long y, long cx, long cy, ULONG color)
{
   POINTL   ptlPoint;
   
   //Clear background
   GpiSetColor (hps, color);
   
   ptlPoint.x = x; ptlPoint.y = y;
   GpiMove (hps, &ptlPoint);
   
   ptlPoint.x = cx; ptlPoint.y = cy;
   GpiBox (hps, DRO_OUTLINEFILL, &ptlPoint, 0, 0);
}

//*********************************************************************
//   Draws the background grid
//*********************************************************************
void drawGrid (HPS hps, long x, long y, long cx, long cy)
{
   #define  LINE_JUMP 10
   long     lx, ly;
   POINTL    p;

   GpiSetColor (hps, CLR_DARKGREEN);
   for (lx=x; lx < x+cx; lx+=LINE_JUMP)
   {
      p.x = lx; p.y = y;
      GpiMove (hps, &p);
      
      p.x = lx; p.y = y+cy;
      GpiLine (hps, &p);
   }
   for (ly=y; ly<y+cy; ly+=LINE_JUMP)
   {
      p.x = x; p.y = ly;
      GpiMove (hps, &p);
      
      p.x = x+cx; p.y = ly;
      GpiLine (hps, &p);
   }
}
//*********************************************************************
//*********************************************************************
long adjustCordinate (ULONG value, ULONG max, ULONG maxValue)
{
   float f;
   
   f  = (float)value / (float)max;
   f *= (float)maxValue;

   return (long)f;
}
//*********************************************************************
//   Draws the memory graph
//*********************************************************************
void drawGraph (HWND hWnd, HPS hps)
{
   ULONG    x, ulMaxMem, index;
   POINTL   ptlPoint;
   SWP      swp;

   drawInfo (hWnd, hps);

   //Get window width and height
   WinQueryWindowPos (hWnd, &swp);

   //Make room for detail info
   if (gbShowDetail)
      swp.cy -= INFO_HEIGHT;
      
   if (swp.cy < 0) swp.cy = 0;

   clearBackGround (hps, 0, 0, swp.cx, swp.cy, CLR_BLACK);

   if (gbShowGrid) drawGrid (hps, 0, 0, swp.cx, swp.cy);
      
   //*** VIRTUAL GRAPH ***
   //Prepare graph drawing
   GpiSetColor (hps, CLR_GREEN);
   ptlPoint.x = 0;
   ptlPoint.y = 0;
   GpiMove (hps, &ptlPoint);
   ulMaxMem = getMaxMemory ();

   //Draw the graph
   index = gulBuffPointer;
   for (x = 0; x < MAX_MEM_SLOT; x++)
   {
      ptlPoint.x = adjustCordinate (x, MAX_MEM_SLOT, swp.cx);
      ptlPoint.y = adjustCordinate (gulVirtualMemBuff[index], ulMaxMem, swp.cy);
      if (ptlPoint.y > swp.cy-1) ptlPoint.y= swp.cy -1;
      
      GpiLine (hps, &ptlPoint);

      index++;
      if (index == MAX_MEM_SLOT) index = 0;
   }

   //*** PHYSICAL GRAPH ***
   if (gbShowPhysicalMemory)
   {
      //Prepare graph drawing
      GpiSetColor (hps, CLR_YELLOW);
      ptlPoint.x = 0;
      ptlPoint.y = 0;
      GpiMove (hps, &ptlPoint);
      ulMaxMem = getMaxMemory ();

      //Draw the graph
      index = gulBuffPointer;
      for (x = 0; x < MAX_MEM_SLOT; x++)
      {
         ptlPoint.x = adjustCordinate (x, MAX_MEM_SLOT, swp.cx);
         ptlPoint.y = adjustCordinate (gulPhysicMemBuff[index], ulMaxMem, swp.cy);
         if (ptlPoint.y > swp.cy-1) ptlPoint.y= swp.cy -1;
      
         GpiLine (hps, &ptlPoint);

         index++;
         if (index == MAX_MEM_SLOT) index = 0;
      }
   }

   //*** Draw physical memory limit ***
   if (gbShowPhysicalMemoryLimit)
   {
      GpiSetColor (hps, CLR_DARKGRAY);
      ptlPoint.x = 0;
      ptlPoint.y = adjustCordinate (gulPhysMem, ulMaxMem, swp.cy);
      GpiMove (hps, &ptlPoint);
      ptlPoint.x = swp.cx;
      GpiLine (hps, &ptlPoint);
   }
}
//*********************************************************************
//*********************************************************************
void typeInfo (void)
{
   printf ("Information produced by Memory Watcher\n");
   printf ("Copyright (C) Jostein Ullestad\n");
   printf ("http:\\www.powerutilities.no\n");
   printf ("\n");
   printf ("Available virtual memory %u KB\n"  , getFreeMemory (FALSE) / 1024);
   printf ("Available physical memory %u KB\n" , getFreeMemory (TRUE) / 1024);
   printf ("Page file size %lu KB\n"           , GetFileSize (gszSwapFileName) / 1024);
   printf ("Available Page file space %lu KB\n", queryDiskSpace (gulSwapDrive) / 1024);
   printf ("Installed memory %lu KB\n"         , gulPhysMem / 1024);
}
//*********************************************************************
//*********************************************************************
void drawInfo (HWND hWnd, HPS hps)
{
   SWP      swp;
   char    szTiles[33];
   RECTL    rectl;

   if (gbShowDetail)
   {
      //Get window width and height
      WinQueryWindowPos (hWnd, &swp);
   
      clearBackGround (hps, 0, swp.cy - INFO_HEIGHT, swp.cx, swp.cy, CLR_PALEGRAY);

      strcpy (szTiles, "8.Helv"); //""System Proportional");
      WinSetPresParam (hWnd, PP_FONTNAMESIZE, (ULONG)strlen (szTiles)+1, (PVOID)szTiles);

      rectl.xLeft  = 0;
      rectl.xRight = swp.cx;
      rectl.yTop   = swp.cy;
      rectl.yBottom= swp.cy - FONT_SIZE;

      if (gbShortText)
         sprintf (szTiles, "VM %u KB", getFreeMemory (FALSE) / 1024);
      else
         sprintf (szTiles, "Available virtual memory %u KB", getFreeMemory (FALSE) / 1024);
      WinDrawText (hps, -1, szTiles, &rectl, CLR_BLACK, CLR_DARKGRAY, 0L);

      rectl.yTop   = swp.cy - FONT_SIZE;
      rectl.yBottom= swp.cy - FONT_SIZE*2;
      if (gbShortText)
         sprintf (szTiles, "PM %lu KB", getFreeMemory (TRUE) / 1024);
      else
         sprintf (szTiles, "Available physical memory %u KB", getFreeMemory (TRUE) / 1024);
      WinDrawText (hps, -1, szTiles, &rectl, CLR_BLACK, CLR_DARKGRAY, 0L);

      rectl.yTop   = swp.cy - FONT_SIZE*2;
      rectl.yBottom= swp.cy - FONT_SIZE*3;
      if (gbShortText)
         sprintf (szTiles, "PF %lu KB", GetFileSize (gszSwapFileName) / 1024);
      else
         sprintf (szTiles, "Page file size %lu KB", GetFileSize (gszSwapFileName) / 1024);
      WinDrawText (hps, -1, szTiles, &rectl, CLR_BLACK, CLR_DARKGRAY, 0L);

      rectl.yTop   = swp.cy - FONT_SIZE*3;
      rectl.yBottom= swp.cy - FONT_SIZE*4;
      if (gbShortText)
         sprintf (szTiles, "FS %lu KB", queryDiskSpace (gulSwapDrive) / 1024);
      else
         sprintf (szTiles, "Available Page file space %lu KB", queryDiskSpace (gulSwapDrive) / 1024);
      WinDrawText (hps, -1, szTiles, &rectl, CLR_BLACK, CLR_DARKGRAY, 0L);

      rectl.yTop   = swp.cy - FONT_SIZE*4;
      rectl.yBottom= swp.cy - FONT_SIZE*5;
      if (gbShortText)
         sprintf (szTiles, "IM %lu KB", gulPhysMem / 1024);
      else
         sprintf (szTiles, "Installed memory %lu KB", gulPhysMem / 1024);
      WinDrawText (hps, -1, szTiles, &rectl, CLR_BLACK, CLR_DARKGRAY, 0L);
   }  //if (gbShowDetail)
}

//*********************************************************************
//Function which returns file size in bytes
//
//   Returns
//   0 : No file, or error
//   x : file size in bytes
//*********************************************************************
ULONG GetFileSize (char *pszFile)
{
   HDIR    fhandle;
   ULONG   count;
   ULONG   fsize;
   USHORT  frc;
   FILEFINDBUF buffer;   ///* file information struct 

   count = 1;
   fhandle = 0xFFFF;
   frc = DosFindFirst (pszFile, &fhandle, 0, &buffer, sizeof(buffer), &count, 1L);
   DosFindClose (fhandle);
   if (frc != 0)
      return(0L);

   fsize = buffer.cbFileAlloc;
   return ((ULONG)fsize);
}

//*********************************************************************
//   Reads a given file and place its contents in the given
//   pre-allocated buffer.
//
//   Returns
//   -1 : Error
//    0 : Ok
//*********************************************************************
long readFile (char *pszFile, char *buffer, ULONG ulSize)
{
   FILE  *fp;
   long  rtc;

   rtc = -1;
   fp = fopen (pszFile, "rt");
   if (fp != NULL)
   {
      fread (buffer, ulSize, 1, fp);
      fclose (fp);
      rtc = 0;
   }
      
   return rtc;
}

//*********************************************************************
//   Reads the corresponding value in the config.sys file
//   eg.
//   SwapPath=e:\os2\system\swapper.dat
//
//   In
//   file     must point to valid OS/2 config.sys file
//   variable the setting you want to read, eg. swappath
//
//   Out
//   value    the resulting value, eg. E:\OS2\SYSTEM\SWAPPER.DAT
//            in upper case.
//
//   Returns
//   -1 : Error or not found
//    0 : Found, "value" contains a valid value.
//*********************************************************************
long getConfigValue (char *file, char *variable, char *value)
{
   char *buffer;
   ULONG ulSize;
   long  lRtc;

   lRtc = -1;
   ulSize = GetFileSize (file);
   if (ulSize > 0)
   {
      buffer = (char *)malloc (ulSize +2);
      if (buffer != NULL)
      {
         if (readFile (file, buffer, ulSize) == 0)
         {
            char *delims = {"\n\r"};
            char *p, *f, *s;
            
            strcat (buffer, "\r");
            strupr (buffer);
            strupr (variable);
            
            p = strtok (buffer, delims);
            while (p != NULL)
            {
               f = strstr (p, variable);
               if (f != NULL)
               {
                  //Trim the string
                  while (*f == ' ' || *f == '\t') f++;
                  //Make sure that the "variable" is the first part of the string
                  s = strstr (f, variable);
                  if (s == f)
                  {
                     p = NULL;
                     s = strstr (f, "=");
                     if (s != NULL)
                     {
                        //Don't include the =
                        strcpy (value, &s[1]);
                        lRtc = 0;
                     }
                  }
                  else
                     p = strtok (NULL, delims);
               }
               else
                  p = strtok (NULL, delims);
            }
         }
         free (buffer);
      }
   }
   return lRtc;
}

//*********************************************************************
//Fills the global varible "gszSwapFileName" with the path and name
//of the systems page-file.
//
//Also fills the global variable "gulSwapFileMin" with the value of the
//minimum free space that the drive must have.
//
//gulSwapDrive gets the swapper drive number, A=1, B=2, C=3, ...
//*********************************************************************
void getSwapSettings (ULONG ulBootDrive)
{
   char szFile[2048];
   char variable[30];
   char value[300];
      
   szFile[0] = ulBootDrive + 'A' - 1; //A = 1, B = 2, C = 3, ...
   szFile[1] = 0;
   strcat (szFile, ":\\CONFIG.SYS");

   strcpy (variable, "MEMMAN");     //can have one or more of the following settings
                                    //(SWAP, NOSWAP, MOVE, NOMOVE, COMMIT, PROTECT)
   if (getConfigValue (szFile, &variable, &value) == 0)
   {
      //Did the user specify all but NOSWAP ?
      if (strstr (value, "NOSWAP") == NULL)
      {
         //Get the path to the swapper file...
         strcpy (variable, "SWAPPATH");
         if (getConfigValue (szFile, &variable, &value) == 0)
         {
            char *p;
            char *delims = {" "};
            long l;

            //Extract the path from the value
            p = strtok (value, delims);
            strcpy (gszSwapFileName, value);
            l = strlen (gszSwapFileName);
            if (gszSwapFileName[l-1] != '\\')
               strcat (gszSwapFileName, "\\");
            strcat (gszSwapFileName, "SWAPPER.DAT");
            gulSwapDrive = gszSwapFileName[0] - 'A' + 1;

            //Extract the minimum free value
            p = strtok (NULL, delims);
            gulSwapFileMin = strtoul (p, NULL, 10);
         }
      }
   }
}      

//*********************************************************************
//*********************************************************************
void init (void)
{
   ULONG ulValues[1];
   ULONG ulBootDrive;
   ULONG ulCounter;

   //Get the RAM amount in the PC
   DosQuerySysInfo (QSV_TOTPHYSMEM, QSV_TOTPHYSMEM, &ulValues, sizeof(ulValues));
   gulPhysMem = ulValues[0];

   //No swap name means we have no swap file. or no swaping enabled in the config.sys
   strcpy (gszSwapFileName, "");
   gulSwapFileMin = 0;
   //Get the BOOT drive
   DosQuerySysInfo (QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulValues, sizeof(ulValues));
   ulBootDrive = ulValues[0];
   //It must be an harddisk
   if (ulBootDrive >=3 ) getSwapSettings (ulBootDrive);
   //
   gulBuffPointer = 0;

   for (ulCounter = 0; ulCounter<MAX_MEM_SLOT; ulCounter++)
      gulVirtualMemBuff[ulCounter] = 0;

   for (ulCounter = 0; ulCounter<MAX_MEM_SLOT; ulCounter++)
      gulPhysicMemBuff[ulCounter] = 0;

}


//*********************************************************************
//   Returns the amount of diskspace on given drive
//*********************************************************************
ULONG queryDiskSpace (ULONG ulDriveNumber)
{
   FSALLOCATE fsAllocate;
   ULONG      ulRtc;

   DosError (FERR_DISABLEHARDERR);
   ulRtc = 0; 
   if (!DosQueryFSInfo (ulDriveNumber, FSIL_ALLOC, &fsAllocate, sizeof(FSALLOCATE)))
      ulRtc = fsAllocate.cSectorUnit * fsAllocate.cUnitAvail * fsAllocate.cbSector;
   DosError (FERR_ENABLEHARDERR);

   return ulRtc;
}
///*********************************************************************
//   Returns the amount of free memory in the system
//
//   Arg:
//      TRUE returns the amount of free physic memory (RAM)
//      FALSE returns the amount of free virtual memory
//      
//Free memory including virtual memory is
//[TotalAvailMem] - Free space on drive - SwapPathMinFree
//   
//*********************************************************************
ULONG getFreeMemory (BOOL bPhysicMem)
{
   ULONG ulValues[1];
   ULONG ulDiskSpace;
   ULONG ulRtc;
   ULONG ulA, ulB;

   if (bPhysicMem)
   {
      APIRET16 APIENTRY16 Dos16MemAvail ( PULONG pulAvailMem ) ;
      ULONG PhysMemFree ;
      Dos16MemAvail ( &PhysMemFree ) ;
      ulRtc = PhysMemFree;
   }
   else
   {
      if (strcmp (gszSwapFileName, "") == 0)
         ulDiskSpace = 0;
      else
         ulDiskSpace = queryDiskSpace (gulSwapDrive);

      //Get the maximum number of bytes an process can allocate
      DosQuerySysInfo (QSV_TOTAVAILMEM, QSV_TOTAVAILMEM, &ulValues, sizeof(ulValues));
   
      ulA  = ulDiskSpace;
      ulA -= gulSwapFileMin*1000;
      ulB  = ulValues[0];


      if (ulB < ulA)
         ulRtc = 0;
      else
         ulRtc = ulB - ulA;
   }

   return ulRtc;
}
///*********************************************************************
//   Returns the amount of free memory in the system
//   
//   Arg:
//      TRUE returns the amount of free physic memory (RAM)
//      FALSE returns the amount of free virtual memory
//
//Free memory including virtual memory is
//[TotalAvailMem] - Free space on drive - SwapPathMinFree
//*********************************************************************
ULONG getUsedMemory (BOOL bPhysicMem)
{
   ULONG ulRtc;
   if (bPhysicMem)
      ulRtc = gulPhysMem - getFreeMemory (TRUE);
   else
      ulRtc = getMaxMemory () - getFreeMemory (FALSE);
   return ulRtc;
}


//*********************************************************************
//   Returns the amount of total memory in the system
//
//Total memory
//[TotPhysMem] + SwapFileSize
//*********************************************************************
ULONG getMaxMemory (void)
{
   ULONG ulRtc;
   
   if (strcmp (gszSwapFileName, "") == 0)
      ulRtc = gulPhysMem;
   else
      ulRtc = gulPhysMem + GetFileSize (gszSwapFileName);

   return ulRtc;
}

//****************************************************************
// Writes the options into the ini file
//****************************************************************
void writeIniSettings (HWND hWnd)
{
   HINI  hini;
   char  cBuffer[255];
   SWP   swp;

   WinQueryWindowPos (hWndFrame, (PSWP)&swp);

   hini = PrfOpenProfile (hab, gszIniFileWithPath);
   sprintf (cBuffer, "%d", swp.cx); PrfWriteProfileString (hini, INI_SECTION, (PSZ)"cx",   (PSZ)cBuffer);
   sprintf (cBuffer, "%d", swp.cy); PrfWriteProfileString (hini, INI_SECTION, (PSZ)"cy",   (PSZ)cBuffer);
   sprintf (cBuffer, "%d", swp.x) ; PrfWriteProfileString (hini, INI_SECTION, (PSZ)"x" ,   (PSZ)cBuffer);
   sprintf (cBuffer, "%d", swp.y) ; PrfWriteProfileString (hini, INI_SECTION, (PSZ)"y" ,   (PSZ)cBuffer);

   sprintf (cBuffer, "%d", gbShowGrid);                PrfWriteProfileString (hini, INI_SECTION, (PSZ)"grid",   (PSZ)cBuffer);
   sprintf (cBuffer, "%d", gulRefrashRate);            PrfWriteProfileString (hini, INI_SECTION, (PSZ)"refrate",(PSZ)cBuffer);
   sprintf (cBuffer, "%d", gbShowPhysicalMemory);      PrfWriteProfileString (hini, INI_SECTION, (PSZ)"phymem", (PSZ)cBuffer);
   sprintf (cBuffer, "%d", gbShowPhysicalMemoryLimit); PrfWriteProfileString (hini, INI_SECTION, (PSZ)"phymeml",(PSZ)cBuffer);
   sprintf (cBuffer, "%d", gbShowDetail);              PrfWriteProfileString (hini, INI_SECTION, (PSZ)"detail", (PSZ)cBuffer);
   sprintf (cBuffer, "%d", gbShortText);               PrfWriteProfileString (hini, INI_SECTION, (PSZ)"shortxt",(PSZ)cBuffer);

   PrfCloseProfile (hini);
}

//****************************************************************
// Read the ini file settings and apply them
//****************************************************************
void readIniSettingsDlg (HWND hWnd)
{
   HINI  hini;
   RECTL rclUpdateRegion;
   LONG  lRate;

   hini = PrfOpenProfile (hab, gszIniFileWithPath);
   gbShowGrid                = PrfQueryProfileInt (hini, INI_SECTION, (PSZ)"grid"   , 1);
   lRate                     = PrfQueryProfileInt (hini, INI_SECTION, (PSZ)"refrate", IDM_NORMAL);
   gbShowPhysicalMemory      = PrfQueryProfileInt (hini, INI_SECTION, (PSZ)"phymem" , 1);
   gbShowPhysicalMemoryLimit = PrfQueryProfileInt (hini, INI_SECTION, (PSZ)"phymeml", 1);
   gbShowDetail              = PrfQueryProfileInt (hini, INI_SECTION, (PSZ)"detail" , 1);
   gbShortText               = PrfQueryProfileInt (hini, INI_SECTION, (PSZ)"shortxt", 0);

   //--- Update menu items ---
   // Show grid
   WinCheckMenuItem (hWndMenu, IDM_GRID, gbShowGrid);
   // Show Physical Memory
   WinCheckMenuItem (hWndMenu, IDM_PHYSICAL_MEM, gbShowPhysicalMemory);
   // Show Physical Memory Limit
   WinCheckMenuItem (hWndMenu, IDM_PHYSICAL_MEM_LIMIT, gbShowPhysicalMemoryLimit);
   // Show Detail
   WinCheckMenuItem (hWndMenu, IDM_DETAIL, gbShowDetail);
   // Show Short Description
   WinCheckMenuItem (hWndMenu, IDM_SHORTTEXT, gbShortText);

   WinQueryWindowRect (hWnd, &rclUpdateRegion);
   WinInvalidateRect(hWnd, &rclUpdateRegion, FALSE);
   // Refresh rate
   WinCheckMenuItem (hWndMenu, IDM_FAST  , FALSE);
   WinCheckMenuItem (hWndMenu, IDM_NORMAL, FALSE);
   WinCheckMenuItem (hWndMenu, IDM_SLOW  , FALSE);
   WinCheckMenuItem (hWndMenu, IDM_PAUSE , FALSE);
   WinSendMsg (hWnd, WM_COMMAND, (MPARAM)lRate, (MPARAM)0);
}

void hideTitleBar (void)
{
   SWP   swp;

   WinDestroyWindow (WinWindowFromID (hWndFrame, FID_TITLEBAR));
   WinDestroyWindow (WinWindowFromID (hWndFrame, FID_SYSMENU));
   WinDestroyWindow (WinWindowFromID (hWndFrame, FID_MINMAX));
   WinDestroyWindow (hWndMenu); hWndMenu = NULL;

   WinQueryWindowPos (hWndFrame, (PSWP)&swp);
   WinSetWindowPos (hWndFrame, HWND_TOP, swp.x, swp.y, swp.cx, swp.cy -1, SWP_SIZE);
   WinSetWindowPos (hWndFrame, HWND_TOP, swp.x, swp.y, swp.cx, swp.cy   , SWP_SIZE);
}

void displayTitleBar (void)
{
   FRAMECDATA pFcdata;
   SWP   swp;

   pFcdata.cb = sizeof(FRAMECDATA);
   pFcdata.flCreateFlags = gulFrameStyle | FCF_MENU;
   pFcdata.hmodResources = 0L;
   pFcdata.idResources = ID_APPNAME;

   WinCreateFrameControls (hWndFrame, &pFcdata, szTitle);
   hWndMenu = WinWindowFromID (hWndFrame, FID_MENU);

   WinQueryWindowPos (hWndFrame, (PSWP)&swp);
   WinSetWindowPos (hWndFrame, HWND_TOP, swp.x, swp.y, swp.cx, swp.cy -1, SWP_SIZE);
   WinSetWindowPos (hWndFrame, HWND_TOP, swp.x, swp.y, swp.cx, swp.cy   , SWP_SIZE);
}

