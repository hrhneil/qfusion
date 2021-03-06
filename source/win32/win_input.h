/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdbool.h>

// Raw input includes
#ifndef WINUSERAPI
#define WINUSERAPI
#endif

#ifndef RIM_TYPEMOUSE
#define WM_INPUT 255

#undef QS_INPUT
#define QS_RAWINPUT 1024
#define QS_INPUT 1031

#define RIM_INPUT     0x00000000
#define RIM_INPUTSINK 0x00000001
#define RIM_TYPEMOUSE    0x00000000
#define RIM_TYPEKEYBOARD 0x00000001
#define RIM_TYPEHID      0x00000002
#define MOUSE_MOVE_RELATIVE      0x00000000
#define MOUSE_MOVE_ABSOLUTE      0x00000001
#define MOUSE_VIRTUAL_DESKTOP    0x00000002
#define MOUSE_ATTRIBUTES_CHANGED 0x00000004
#define RI_MOUSE_LEFT_BUTTON_DOWN   0x0001
#define RI_MOUSE_LEFT_BUTTON_UP     0x0002
#define RI_MOUSE_RIGHT_BUTTON_DOWN  0x0004
#define RI_MOUSE_RIGHT_BUTTON_UP    0x0008
#define RI_MOUSE_MIDDLE_BUTTON_DOWN 0x0010
#define RI_MOUSE_MIDDLE_BUTTON_UP   0x0020
#define RI_MOUSE_BUTTON_1_DOWN      RI_MOUSE_LEFT_BUTTON_DOWN
#define RI_MOUSE_BUTTON_1_UP        RI_MOUSE_LEFT_BUTTON_UP
#define RI_MOUSE_BUTTON_2_DOWN      RI_MOUSE_RIGHT_BUTTON_DOWN
#define RI_MOUSE_BUTTON_2_UP        RI_MOUSE_RIGHT_BUTTON_UP
#define RI_MOUSE_BUTTON_3_DOWN      RI_MOUSE_MIDDLE_BUTTON_DOWN
#define RI_MOUSE_BUTTON_3_UP        RI_MOUSE_MIDDLE_BUTTON_UP
#define RI_MOUSE_BUTTON_4_DOWN      0x0040
#define RI_MOUSE_BUTTON_4_UP        0x0080
#define RI_MOUSE_BUTTON_5_DOWN      0x0100
#define RI_MOUSE_BUTTON_5_UP        0x0200
#define RI_MOUSE_WHEEL              0x0400
#define KEYBOARD_OVERRUN_MAKE_CODE 0x00ff
#define RI_KEY_MAKE            0x0000
#define RI_KEY_BREAK           0x0001
#define RI_KEY_E0              0x0002
#define RI_KEY_E1              0x0004
#define RI_KEY_TERMSRV_SET_LED 0x0008
#define RI_KEY_TERMSRV_SHADOW  0x0010
#define RID_INPUT  0x10000003
#define RID_HEADER 0x10000005
#define RIDI_PREPARSEDDATA 0x20000005
#define RIDI_DEVICENAME    0x20000007
#define RIDI_DEVICEINFO    0x2000000b
#define RIDEV_REMOVE       0x00000001
#define RIDEV_EXCLUDE      0x00000010
#define RIDEV_PAGEONLY     0x00000020
#define RIDEV_NOLEGACY     0x00000030
#define RIDEV_INPUTSINK    0x00000100
#define RIDEV_CAPTUREMOUSE 0x00000200
#define RIDEV_NOHOTKEYS    0x00000200
#define RIDEV_APPKEYS      0x00000400

DECLARE_HANDLE( HRAWINPUT );
typedef struct tagRAWINPUTHEADER {
	DWORD dwType;
	DWORD dwSize;
	HANDLE hDevice;
	WPARAM wParam;
} RAWINPUTHEADER,*PRAWINPUTHEADER;
typedef struct tagRAWMOUSE {
	USHORT usFlags;
	union {
		ULONG ulButtons;
		struct {
			USHORT usButtonFlags;
			USHORT usButtonData;
		};
	};
	ULONG ulRawButtons;
	LONG lLastX;
	LONG lLastY;
	ULONG ulExtraInformation;
} RAWMOUSE,*PRAWMOUSE,*LPRAWMOUSE;
typedef struct tagRAWKEYBOARD {
	USHORT MakeCode;
	USHORT Flags;
	USHORT Reserved;
	USHORT VKey;
	UINT Message;
	ULONG ExtraInformation;
} RAWKEYBOARD,*PRAWKEYBOARD,*LPRAWKEYBOARD;
typedef struct tagRAWHID {
	DWORD dwSizeHid;
	DWORD dwCount;
	BYTE bRawData;
} RAWHID,*PRAWHID,*LPRAWHID;
typedef struct tagRAWINPUT {
	RAWINPUTHEADER header;
	union {
		RAWMOUSE mouse;
		RAWKEYBOARD keyboard;
		RAWHID hid;
	} data;
} RAWINPUT,*PRAWINPUT,*LPRAWINPUT;
typedef struct tagRAWINPUTDEVICE {
	USHORT usUsagePage;
	USHORT usUsage;
	DWORD dwFlags;
	HWND hwndTarget;
} RAWINPUTDEVICE,*PRAWINPUTDEVICE,*LPRAWINPUTDEVICE;
typedef const RAWINPUTDEVICE *PCRAWINPUTDEVICE;
typedef struct tagRAWINPUTDEVICELIST {
	HANDLE hDevice;
	DWORD dwType;
} RAWINPUTDEVICELIST,*PRAWINPUTDEVICELIST;

WINUSERAPI LRESULT WINAPI DefRawInputProc( PRAWINPUT*,INT,UINT );
WINUSERAPI UINT WINAPI GetRawInputBuffer( PRAWINPUT,PUINT,UINT );
WINUSERAPI UINT WINAPI GetRawInputData( HRAWINPUT,UINT,LPVOID,PUINT,UINT );
WINUSERAPI UINT WINAPI GetRawInputDeviceInfoA( HANDLE,UINT,LPVOID,PUINT );
WINUSERAPI UINT WINAPI GetRawInputDeviceInfoW( HANDLE,UINT,LPVOID,PUINT );
WINUSERAPI UINT WINAPI GetRawInputDeviceList( PRAWINPUTDEVICELIST,PUINT,UINT );
WINUSERAPI UINT WINAPI GetRegisteredRawInputDevices( PRAWINPUTDEVICE,PUINT,UINT );
WINUSERAPI BOOL WINAPI RegisterRawInputDevices( PCRAWINPUTDEVICE,UINT,UINT );

#endif

extern int mouse_buttons;
extern int mouse_wheel_type;

void IN_Activate( bool active );

void IN_MouseEvent( int mstate );

void IN_RawInput_MouseRead( HANDLE in_device_handle );

void IN_WinIME_Init( void );
void IN_WinIME_AssociateContext( void );
void IN_WinIME_Shutdown( void );
