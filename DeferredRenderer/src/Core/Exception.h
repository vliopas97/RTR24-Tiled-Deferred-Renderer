#pragma once
#include <string>
#include <exception>
#include <vector>

#include "Core.h"

class ExceptionBase : public std::exception
{
public:
	ExceptionBase(uint32_t line, const char* file, const std::string& customMessage = "") noexcept;
	virtual ~ExceptionBase() = default;
	const char* what() const noexcept override;
	virtual const char* GetType() const noexcept;

	std::string GetRawMessage() const noexcept;

	uint32_t GetLine() const noexcept;
	const std::string& GetFile() const noexcept;

protected:
	static std::string TranslateErrorCode(HRESULT result) noexcept;
private:
	uint32_t Line;
	std::string File;
	std::string CustomMessage;

protected:
	mutable std::string ErrorMessage;
};

#define EXCEPTION(x) ExceptionBase(__LINE__, __FILE__, x)

#define EXCEPTION_WRAP(x)\
	try\
	{\
		x\
	}\
	catch( const ExceptionBase& e )\
	{\
		MessageBoxA(nullptr, e.what(), e.GetType(), MB_OK | MB_ICONEXCLAMATION);\
	}\
	catch (const std::exception& e)\
	{\
		MessageBoxA(nullptr, e.what(), "Standard Exception", MB_OK | MB_ICONEXCLAMATION);\
	}\
	catch (...)\
	{\
		MessageBoxA(nullptr, "No details available", "Unknown Exception", MB_OK | MB_ICONEXCLAMATION);\
	}\


class WindowException : public ExceptionBase
{
public:
	WindowException(uint32_t line, const char* file, HRESULT result) noexcept;
	virtual ~WindowException() = default;
	const char* what() const noexcept override;
	virtual const char* GetType() const noexcept override;
	HRESULT GetErrorCode() const noexcept;

private:
	HRESULT Result;
};

#define WIN_EXCEPTION(result) WindowException(__LINE__, __FILE__, result)
#define WIN_EXCEPTION_LAST_ERROR WindowException(__LINE__, __FILE__, GetLastError())

///////////////////////
// Direct3D Exceptions
//////////////////////

struct DXGIInfoManager
{
	DXGIInfoManager();
	~DXGIInfoManager();
	DXGIInfoManager(const DXGIInfoManager&) = delete;
	DXGIInfoManager& operator=(const DXGIInfoManager) = delete;

	void Reset() noexcept;
	std::vector <std::string> GetMessages() const;
private:
	unsigned long long Next = 0u;
	struct IDXGIInfoQueue* InfoQueue = nullptr;
};

class GraphicsException : public ExceptionBase
{
public:
	GraphicsException(int line, const char* file, HRESULT result, std::vector<std::string> messages = {}) noexcept;
	const char* what() const noexcept override;
	virtual const char* GetType() const noexcept override;
	HRESULT GetErrorCode() const noexcept;
	std::string GetErrorInfo() const noexcept;
private:
	HRESULT Result;
	std::string Info;
};

class GraphicsExceptionInfo : public ExceptionBase
{
public:
	GraphicsExceptionInfo(int line, const char* file, std::vector<std::string> messages = {}) noexcept;
	const char* what() const noexcept override;
	virtual const char* GetType() const noexcept override;
	std::string GetErrorInfo() const noexcept;

private:
	std::string Info;
};

class DeviceRemovedException : public GraphicsException
{
	using GraphicsException::GraphicsException;
public:
	const char* GetType() const noexcept override;
};

class NoGraphicsException : public ExceptionBase
{
public:
	using ExceptionBase::ExceptionBase;
	const char* GetType() const noexcept override;
private:
	std::string Reason;
};

#define NO_GRAPHICS_EXCEPTION() NoGraphicsException( __LINE__,__FILE__ )
#define GRAPHICS_EXCEPTION_NOINFO(hr) GraphicsException(__LINE__,__FILE__,(hr));
#define GRAPHICS_ASSERT_NOINFO(hrcall) \
{ \
    HRESULT hr = (hrcall); \
    if (FAILED(hr)) \
        throw GraphicsException(__LINE__, __FILE__, hr); \
}


//#ifndef NDEBUG
#define ASSERT(condition, msg)\
if(!condition) throw EXCEPTION(msg);

#define GRAPHICS_ASSERT(hrcall)\
{	DXGIInfoManager InfoManager;\
	InfoManager.Reset();\
    HRESULT hr = (hrcall); \
    if (FAILED(hr)) \
        throw GraphicsException(__LINE__, __FILE__, hr, InfoManager.GetMessages()); \
}

#define GRAPHICS_DEVICE_REMOVED_EXCEPTION(hr) DeviceRemovedException( __LINE__,__FILE__,(hr),InfoManager.GetMessages() )
#define GRAPHICS_INFO_ONLY(call) {DXGIInfoManager InfoManager; InfoManager.Reset(); (call); {auto v = InfoManager.GetMessages(); if(!v.empty()) {throw GraphicsExceptionInfo( __LINE__,__FILE__,InfoManager.GetMessages());}}}
//
//#else
//#define GRAPHICS_EXCEPTION(hr) GraphicsException( __LINE__,__FILE__,(hr))
//#define GRAPHICS_ASSERT(hrcall) GRAPHICS_ASSERT_NOINFO(hrcall)
//#define GRAPHICS_DEVICE_REMOVED_EXCEPTION(hr) DeviceRemovedException( __LINE__,__FILE__,(hr))
//#define GRAPHICS_INFO_ONLY(call) call;
//#endif