#pragma once

// DeckLink
#if _WIN32
#include <comdef.h>
#endif
#include <DeckLinkAPI.h>

// Nodos
#include <Nodos/Types.h>

// stl
#include <functional>
#include <optional>
#include <atomic>
#include <chrono>
#include <string>

namespace nos::decklink
{
template <typename T>
void Release(T& obj)
{
	if (obj)
	{
		obj->Release();
		obj = nullptr;
	}
}

#if _WIN32
#define dlbool_t	BOOL
#define dlstring_t	BSTR
inline const std::function<void(dlstring_t)> DeleteString = SysFreeString;
inline const std::function<std::string(dlstring_t)> DlToStdString = [](dlstring_t dl_str) -> std::string
{
	int wlen = ::SysStringLen(dl_str);
	int mblen = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)dl_str, wlen, NULL, 0, NULL, NULL);

	std::string ret_str(mblen, '\0');
	mblen = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)dl_str, wlen, &ret_str[0], mblen, NULL, NULL);

	return ret_str;
};
inline const std::function<dlstring_t(std::string)> StdToDlString = [](std::string std_str) -> dlstring_t
{
	int wlen = ::MultiByteToWideChar(CP_ACP, 0, std_str.data(), (int)std_str.length(), NULL, 0);

	dlstring_t ret_str = ::SysAllocStringLen(NULL, wlen);
	::MultiByteToWideChar(CP_ACP, 0, std_str.data(), (int)std_str.length(), ret_str, wlen);

	return ret_str;
};
#endif


template <typename T>
class Object : public T
{
public:
	Object() :
		RefCount(1)
	{
	}

	// IUnknown needs only a dummy implementation
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override
	{
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++RefCount;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		ULONG newRefValue = --RefCount;

		if (newRefValue == 0)
			delete this;

		return newRefValue;
	}

private:
	std::atomic<int32_t> RefCount;
};

struct IOHandlerBaseI
{
	virtual ~IOHandlerBaseI() = default;
	bool IsActive = false;

	BMDTimeValue FrameDuration = 0;
	BMDTimeScale TimeScale = 0;

	virtual bool Open(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat) = 0;
	virtual bool Close() = 0;

	virtual bool WaitFrame(std::chrono::milliseconds timeout) = 0;
	virtual void DmaTransfer(void* buffer, size_t size) = 0;
	std::optional<nosVec2u> GetDeltaSeconds() const;
};
	
template <typename T>
struct IOHandlerBase : IOHandlerBaseI
{
	T* Interface = nullptr;
	~IOHandlerBase() override = default;
	operator bool() const
	{
		return Interface != nullptr;
	}
	T* operator->() const
	{
		return Interface;
	}
};

inline std::optional<nosVec2u> IOHandlerBaseI::GetDeltaSeconds() const
{
	if (!IsActive)
		return std::nullopt;
	return nosVec2u{ (uint32_t)FrameDuration, (uint32_t)TimeScale };
}
}