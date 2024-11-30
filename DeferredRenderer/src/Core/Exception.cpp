#include "Exception.h"
#include <sstream>
#include <iomanip>
#include <dxgidebug.h>

ExceptionBase::ExceptionBase(uint32_t line, const char* file, const std::string& customMessage) noexcept
	: Line(line), File(file), CustomMessage(customMessage)
{}

const char* ExceptionBase::what() const noexcept
{
	std::ostringstream oss;
	oss << GetType() << " | " << GetRawMessage() << std::endl << CustomMessage;
	ErrorMessage = oss.str();
	return ErrorMessage.c_str();
}

const char* ExceptionBase::GetType() const noexcept
{
	return "Exception Base";
}

std::string ExceptionBase::GetRawMessage() const noexcept
{
	std::ostringstream oss;
	oss << "File: " << std::left << std::setw(File.size() + 2) << File << std::endl <<
		"Line:" << std::setw(std::to_string(Line).length() + 1) << std::right << Line << std::endl;
	return oss.str();
}

uint32_t ExceptionBase::GetLine() const noexcept
{
	return Line;
}

const std::string& ExceptionBase::GetFile() const noexcept
{
	return File;
}


std::string ExceptionBase::TranslateErrorCode(HRESULT result) noexcept
{
	char* pMsgBuf = nullptr;
	DWORD nMsgLen = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&pMsgBuf), 0, nullptr);

	if (nMsgLen == 0)
	{
		return "Unidentified error code";
	}
	std::string errorString = pMsgBuf;
	LocalFree(pMsgBuf);
	return errorString;
}

WindowException::WindowException(uint32_t line, const char* file, HRESULT result) noexcept
	: ExceptionBase(line, file), Result(result)
{}

const char* WindowException::what() const noexcept
{
	std::ostringstream oss;
	oss << GetType() <<
		"Error Code: " << GetErrorCode() << std::endl <<
		"Error Code Demangle: " << TranslateErrorCode(Result) << std::endl <<
		GetRawMessage();
	ErrorMessage = oss.str();
	return ErrorMessage.c_str();
}

const char* WindowException::GetType() const noexcept
{
	return "Window Exception";
}

HRESULT WindowException::GetErrorCode() const noexcept
{
	return Result;
}

GraphicsException::GraphicsException(int line, const char* file, HRESULT result, std::vector<std::string> messages) noexcept
	:ExceptionBase(line, file), Result(result)
{
	for (const auto& m : messages)
	{
		Info += m;
		Info.push_back('\n');
	}

	if (!Info.empty())
		Info.pop_back();
}

const char* GraphicsException::what() const noexcept
{
	std::ostringstream oss;
	oss << GetType() << std::endl <<
		"Error Code: 0x" << std::hex << std::uppercase << GetErrorCode() << std::endl <<
		std::dec << " (" << (unsigned long)GetErrorCode() << ")" << std::endl <<
		"Error Code Demangle: " << TranslateErrorCode(Result) << std::endl;
	if (!Info.empty())
	{
		oss << "\n[Error Info]\n" << GetErrorInfo() << std::endl << std::endl;
	}
	oss << GetRawMessage();
	ErrorMessage = oss.str();
	return ErrorMessage.c_str();
}

const char* GraphicsException::GetType() const noexcept
{
	return "Graphics Exception";
}

HRESULT GraphicsException::GetErrorCode() const noexcept
{
	return Result;
}

std::string GraphicsException::GetErrorInfo() const noexcept
{
	return Info;
}


GraphicsExceptionInfo::GraphicsExceptionInfo(int line, const char* file, std::vector<std::string> messages) noexcept
	:ExceptionBase(line, file)
{
	for (const auto& m : messages)
	{
		Info += m;
		Info.push_back('\n');
	}

	if (!Info.empty())
		Info.pop_back();
}

const char* GraphicsExceptionInfo::what() const noexcept
{
	std::ostringstream oss;
	if (!Info.empty())
	{
		oss << "\n[Error Info]\n" << GetErrorInfo() << std::endl << std::endl;
	}
	oss << GetRawMessage();
	ErrorMessage = oss.str();
	return ErrorMessage.c_str();
}

const char* GraphicsExceptionInfo::GetType() const noexcept
{
	return "Graphics Exception (No Info)";
}

std::string GraphicsExceptionInfo::GetErrorInfo() const noexcept
{
	return Info;
}


const char* DeviceRemovedException::GetType() const noexcept
{
	return "Device Removed Exception";
}

const char* NoGraphicsException::GetType() const noexcept
{
	return "No Graphics Exception";
}

DXGIInfoManager::DXGIInfoManager()
{
	using DXGIGetDebugInterface = HRESULT(WINAPI*)(REFIID, void**);

	const auto modDxgiDebug = LoadLibraryEx(L"dxgidebug.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!modDxgiDebug)
		throw WIN_EXCEPTION_LAST_ERROR;

	const auto dxgiGetDebugInterface = reinterpret_cast<DXGIGetDebugInterface>(
		reinterpret_cast<void*>(GetProcAddress(modDxgiDebug, "DXGIGetDebugInterface"))
		);
	if (!dxgiGetDebugInterface)
		throw WIN_EXCEPTION_LAST_ERROR;

	GRAPHICS_ASSERT_NOINFO(dxgiGetDebugInterface(__uuidof(IDXGIInfoQueue), reinterpret_cast<void**>(&InfoQueue)));
}

DXGIInfoManager::~DXGIInfoManager()
{
	if (!InfoQueue)
		InfoQueue->Release();
}

void DXGIInfoManager::Reset() noexcept
{
	Next = InfoQueue->GetNumStoredMessages(DXGI_DEBUG_ALL);
}

std::vector<std::string> DXGIInfoManager::GetMessages() const
{
	std::vector<std::string> messages;
	const auto end = InfoQueue->GetNumStoredMessages(DXGI_DEBUG_ALL);
	for (auto i = Next; i < end; i++)
	{
		SIZE_T messageLength = 0;

		GRAPHICS_ASSERT_NOINFO(InfoQueue->GetMessageW(DXGI_DEBUG_ALL, i, nullptr, &messageLength));

		auto bytes = std::make_unique<byte[]>(messageLength);
		auto message = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(bytes.get());

		GRAPHICS_ASSERT_NOINFO(InfoQueue->GetMessageW(DXGI_DEBUG_ALL, i, message, &messageLength));
		messages.emplace_back(message->pDescription);
	}
	return messages;
}