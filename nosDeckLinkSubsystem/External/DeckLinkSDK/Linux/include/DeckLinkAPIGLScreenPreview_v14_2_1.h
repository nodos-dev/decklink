/* -LICENSE-START-
** Copyright (c) 2022 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation (the
** "Software") to use, reproduce, display, distribute, sub-license, execute,
** and transmit the Software, and to prepare derivative works of the Software,
** and to permit third-parties to whom the Software is furnished to do so, in
** accordance with:
**
** (1) if the Software is obtained from Blackmagic Design, the End User License
** Agreement for the Software Development Kit (“EULA”) available at
** https://www.blackmagicdesign.com/EULA/DeckLinkSDK; or
**
** (2) if the Software is obtained from any third party, such licensing terms
** as notified by that third party,
**
** and all subject to the following:
**
** (3) the copyright notices in the Software and this entire statement,
** including the above license grant, this restriction and the following
** disclaimer, must be included in all copies of the Software, in whole or in
** part, and all derivative works of the Software, unless such copies or
** derivative works are solely in the form of machine-executable object code
** generated by a source language processor.
**
** (4) THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
** OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
**
** A copy of the Software is available free of charge at
** https://www.blackmagicdesign.com/desktopvideo_sdk under the EULA.
**
** -LICENSE-END-
*/

#ifndef BMD_DECKLINKAPIGLSCREENPREVIEW_v14_2_1_H
#define BMD_DECKLINKAPIGLSCREENPREVIEW_v14_2_1_H

#include "DeckLinkAPI.h"
#include "DeckLinkAPIVideoFrame_v14_2_1.h"

// Type Declarations

BMD_CONST REFIID IID_IDeckLinkGLScreenPreviewHelper_v14_2_1           = /* 504E2209-CAC7-4C1A-9FB4-C5BB6274D22F */ { 0x50, 0x4E, 0x22, 0x09, 0xCA, 0xC7, 0x4C, 0x1A, 0x9F, 0xB4, 0xC5, 0xBB, 0x62, 0x74, 0xD2, 0x2F };

/* Interface IDeckLinkGLScreenPreviewHelper - Created with CoCreateInstance on platforms with native COM support or from CreateOpenGLScreenPreviewHelper/CreateOpenGL3ScreenPreviewHelper on other platforms. */

class BMD_PUBLIC IDeckLinkGLScreenPreviewHelper_v14_2_1 : public IUnknown
{
public:

	/* Methods must be called with OpenGL context set */

	virtual HRESULT InitializeGL (void) = 0;
	virtual HRESULT PaintGL (void) = 0;
	virtual HRESULT SetFrame (/* in */ IDeckLinkVideoFrame_v14_2_1* theFrame) = 0;
	virtual HRESULT Set3DPreviewFormat (/* in */ BMD3DPreviewFormat previewFormat) = 0;

protected:
	virtual ~IDeckLinkGLScreenPreviewHelper_v14_2_1 () {} // call Release method to drop reference count
};

#endif
