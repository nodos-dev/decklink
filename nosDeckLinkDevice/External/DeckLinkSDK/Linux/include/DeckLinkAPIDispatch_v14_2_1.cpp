/* -LICENSE-START-
** Copyright (c) 2009 Blackmagic Design
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
**/

#include <stdio.h>
#include <pthread.h>
#include <dlfcn.h>

#include "DeckLinkAPI_v14_2_1.h"

#define kDeckLinkAPI_Name "libDeckLinkAPI.so"
#define KDeckLinkPreviewAPI_Name "libDeckLinkPreviewAPI.so"

typedef IDeckLinkIterator* (*CreateIteratorFunc)(void);
typedef IDeckLinkAPIInformation* (*CreateAPIInformationFunc)(void);
typedef IDeckLinkGLScreenPreviewHelper_v14_2_1* (*CreateOpenGLScreenPreviewHelperFunc)(void);
typedef IDeckLinkGLScreenPreviewHelper_v14_2_1* (*CreateOpenGL3ScreenPreviewHelperFunc)(void);
typedef IDeckLinkVideoConversion_v14_2_1* (*CreateVideoConversionInstanceFunc)(void);
typedef IDeckLinkDiscovery* (*CreateDeckLinkDiscoveryInstanceFunc)(void);
typedef IDeckLinkVideoFrameAncillaryPackets* (*CreateVideoFrameAncillaryPacketsInstanceFunc)(void);

static pthread_once_t					gDeckLinkOnceControl = PTHREAD_ONCE_INIT;
static pthread_once_t					gPreviewOnceControl = PTHREAD_ONCE_INIT;

static bool								gLoadedDeckLinkAPI = false;

static CreateIteratorFunc					gCreateIteratorFunc = NULL;
static CreateAPIInformationFunc				gCreateAPIInformationFunc = NULL;
static CreateOpenGLScreenPreviewHelperFunc	gCreateOpenGLPreviewFunc = NULL;
static CreateOpenGL3ScreenPreviewHelperFunc	gCreateOpenGL3PreviewFunc = NULL;
static CreateVideoConversionInstanceFunc	gCreateVideoConversionFunc	= NULL;
static CreateDeckLinkDiscoveryInstanceFunc	gCreateDeckLinkDiscoveryFunc = NULL;
static CreateVideoFrameAncillaryPacketsInstanceFunc	gCreateVideoFrameAncillaryPacketsFunc = NULL;

static void	InitDeckLinkAPI (void)
{
	void *libraryHandle;
	
	libraryHandle = dlopen(kDeckLinkAPI_Name, RTLD_NOW|RTLD_GLOBAL);
	if (!libraryHandle)
	{
		fprintf(stderr, "%s\n", dlerror());
		return;
	}
	
	gLoadedDeckLinkAPI = true;
	
	gCreateIteratorFunc = (CreateIteratorFunc)dlsym(libraryHandle, "CreateDeckLinkIteratorInstance_0004");
	if (!gCreateIteratorFunc)
		fprintf(stderr, "%s\n", dlerror());
	gCreateAPIInformationFunc = (CreateAPIInformationFunc)dlsym(libraryHandle, "CreateDeckLinkAPIInformationInstance_0001");
	if (!gCreateAPIInformationFunc)
		fprintf(stderr, "%s\n", dlerror());
	gCreateVideoConversionFunc = (CreateVideoConversionInstanceFunc)dlsym(libraryHandle, "CreateVideoConversionInstance_0001");
	if (!gCreateVideoConversionFunc)
		fprintf(stderr, "%s\n", dlerror());
	gCreateDeckLinkDiscoveryFunc = (CreateDeckLinkDiscoveryInstanceFunc)dlsym(libraryHandle, "CreateDeckLinkDiscoveryInstance_0003");
	if (!gCreateDeckLinkDiscoveryFunc)
		fprintf(stderr, "%s\n", dlerror());
	gCreateVideoFrameAncillaryPacketsFunc = (CreateVideoFrameAncillaryPacketsInstanceFunc)dlsym(libraryHandle, "CreateVideoFrameAncillaryPacketsInstance_0001");
	if (!gCreateVideoFrameAncillaryPacketsFunc)
		fprintf(stderr, "%s\n", dlerror());
}

static void	InitDeckLinkPreviewAPI (void)
{
	void *libraryHandle;
	
	libraryHandle = dlopen(KDeckLinkPreviewAPI_Name, RTLD_NOW|RTLD_GLOBAL);
	if (!libraryHandle)
	{
		fprintf(stderr, "%s\n", dlerror());
		return;
	}
	gCreateOpenGLPreviewFunc = (CreateOpenGLScreenPreviewHelperFunc)dlsym(libraryHandle, "CreateOpenGLScreenPreviewHelper_0001");
	if (!gCreateOpenGLPreviewFunc)
		fprintf(stderr, "%s\n", dlerror());
	gCreateOpenGL3PreviewFunc = (CreateOpenGL3ScreenPreviewHelperFunc)dlsym(libraryHandle, "CreateOpenGL3ScreenPreviewHelper_0001");
	if (!gCreateOpenGL3PreviewFunc)
		fprintf(stderr, "%s\n", dlerror());
}

bool		IsDeckLinkAPIPresent_v14_2_1 (void)
{
	// If the DeckLink API dynamic library was successfully loaded, return this knowledge to the caller
	return gLoadedDeckLinkAPI;
}

IDeckLinkIterator*		CreateDeckLinkIteratorInstance_v14_2_1 (void)
{
	pthread_once(&gDeckLinkOnceControl, InitDeckLinkAPI);
	
	if (gCreateIteratorFunc == NULL)
		return NULL;
	return gCreateIteratorFunc();
}

IDeckLinkAPIInformation*	CreateDeckLinkAPIInformationInstance_v14_2_1 (void)
{
	pthread_once(&gDeckLinkOnceControl, InitDeckLinkAPI);
	
	if (gCreateAPIInformationFunc == NULL)
		return NULL;
	return gCreateAPIInformationFunc();
}

IDeckLinkGLScreenPreviewHelper_v14_2_1*		CreateOpenGLScreenPreviewHelper_v14_2_1 (void)
{
	pthread_once(&gDeckLinkOnceControl, InitDeckLinkAPI);
	pthread_once(&gPreviewOnceControl, InitDeckLinkPreviewAPI);
	
	if (gCreateOpenGLPreviewFunc == NULL)
		return NULL;
	return gCreateOpenGLPreviewFunc();
}

IDeckLinkGLScreenPreviewHelper_v14_2_1*		CreateOpenGL3ScreenPreviewHelper_v14_2_1 (void)
{
	pthread_once(&gDeckLinkOnceControl, InitDeckLinkAPI);
	pthread_once(&gPreviewOnceControl, InitDeckLinkPreviewAPI);

	if (gCreateOpenGL3PreviewFunc == NULL)
		return NULL;
	return gCreateOpenGL3PreviewFunc();
}

IDeckLinkVideoConversion_v14_2_1* CreateVideoConversionInstance_v14_2_1 (void)
{
	pthread_once(&gDeckLinkOnceControl, InitDeckLinkAPI);
	
	if (gCreateVideoConversionFunc == NULL)
		return NULL;
	return gCreateVideoConversionFunc();
}

IDeckLinkDiscovery* CreateDeckLinkDiscoveryInstance_v14_2_1 (void)
{
	pthread_once(&gDeckLinkOnceControl, InitDeckLinkAPI);
	
	if (gCreateDeckLinkDiscoveryFunc == NULL)
		return NULL;
	return gCreateDeckLinkDiscoveryFunc();
}

IDeckLinkVideoFrameAncillaryPackets* CreateVideoFrameAncillaryPacketsInstance_v14_2_1 (void)
{
	pthread_once(&gDeckLinkOnceControl, InitDeckLinkAPI);
	
	if (gCreateVideoFrameAncillaryPacketsFunc == NULL)
		return NULL;
	return gCreateVideoFrameAncillaryPacketsFunc();
}
