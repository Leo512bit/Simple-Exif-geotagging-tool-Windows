//© 2026 Leonard Matthew Teyssier BSD-4 Clause License
#ifndef UNICODE
#define UNICODE
#endif

#define COBJMACROS
#pragma comment(lib, "windowscodecs.lib")


#include <windows.h>

#include <wincodec.h>
#include <wincodecsdk.h>

//#include <ocidl.h>

// Uncomment the following line to use TxF (Transactional NTFS) support rather than tradional copy/rename
// Disabled by default due to being NTFS only.
//#define TXF

#ifdef TXF

#pragma comment(lib, "KtmW32.lib")
#pragma comment(lib, "shlwapi.lib") // Required for SHCreateMemStream
#include <ktmw32.h>
#include <shlwapi.h>
#include <objidl.h>

#endif

#include "main.h"
#include "wic_geotag.h"

/* ------------------------------------------------------------------------ -
 CUSTOM C-BASED TRANSACTED ISTREAM IMPLEMENTATION
 -------------------------------------------------------------------------*/
#ifdef TXF
typedef struct {
	const IStreamVtbl* lpVtbl;
	LONG refCount;
	HANDLE hTransactedFile;
} TxFStream;

// Forward declarations for the VTable 

HRESULT STDMETHODCALLTYPE TxF_QueryInterface(IStream* This, REFIID riid, void** ppvObject);
ULONG STDMETHODCALLTYPE TxF_AddRef(IStream* This);
ULONG STDMETHODCALLTYPE TxF_Release(IStream* This);
HRESULT STDMETHODCALLTYPE TxF_Read(IStream* This, void* pv, ULONG cb, ULONG* pcbRead);
HRESULT STDMETHODCALLTYPE TxF_Write(IStream* This, const void* pv, ULONG cb, ULONG* pcbWritten);
HRESULT STDMETHODCALLTYPE TxF_Seek(IStream* This, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition);
HRESULT STDMETHODCALLTYPE TxF_SetSize(IStream* This, ULARGE_INTEGER libNewSize);
HRESULT STDMETHODCALLTYPE TxF_CopyTo(IStream* This, IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten);
HRESULT STDMETHODCALLTYPE TxF_Commit(IStream* This, DWORD grfCommitFlags);
HRESULT STDMETHODCALLTYPE TxF_Revert(IStream* This);
HRESULT STDMETHODCALLTYPE TxF_LockRegion(IStream* This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
HRESULT STDMETHODCALLTYPE TxF_UnlockRegion(IStream* This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
HRESULT STDMETHODCALLTYPE TxF_Stat(IStream* This, STATSTG* pstatstg, DWORD grfStatFlag);
HRESULT STDMETHODCALLTYPE TxF_Clone(IStream* This, IStream** ppstm);

static IStreamVtbl TxFStream_Vtbl = {
	TxF_QueryInterface, TxF_AddRef, TxF_Release, TxF_Read,
	TxF_Write, TxF_Seek, TxF_SetSize, TxF_CopyTo, TxF_Commit,
	TxF_Revert, TxF_LockRegion, TxF_UnlockRegion, TxF_Stat, TxF_Clone
};

HRESULT STDMETHODCALLTYPE TxF_QueryInterface(IStream* This, REFIID riid, void** ppvObject) {
	if (!ppvObject) return E_POINTER;
	*ppvObject = NULL;
	if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IStream) || IsEqualIID(riid, &IID_ISequentialStream)) {
		*ppvObject = This;
		TxF_AddRef(This);
		return S_OK;
	}
	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE TxF_AddRef(IStream* This) {
	TxFStream* pTxF = (TxFStream*)This;
	return InterlockedIncrement(&pTxF->refCount);
}

ULONG STDMETHODCALLTYPE TxF_Release(IStream* This) {
	TxFStream* pTxF = (TxFStream*)This;
	LONG count = InterlockedDecrement(&pTxF->refCount);
	if (count == 0) {
		if (pTxF->hTransactedFile && pTxF->hTransactedFile != INVALID_HANDLE_VALUE) {
			CloseHandle(pTxF->hTransactedFile);
		}
		HeapFree(GetProcessHeap(), 0, pTxF);
	}
	return count;
}

HRESULT STDMETHODCALLTYPE TxF_Read(IStream* This, void* pv, ULONG cb, ULONG* pcbRead) {
	TxFStream* pTxF = (TxFStream*)This;
	DWORD bytesRead = 0;
	if (ReadFile(pTxF->hTransactedFile, pv, cb, &bytesRead, NULL)) {
		if (pcbRead) *pcbRead = bytesRead;
		return S_OK;
	}
	return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT STDMETHODCALLTYPE TxF_Write(IStream* This, const void* pv, ULONG cb, ULONG* pcbWritten) {
	TxFStream* pTxF = (TxFStream*)This;
	DWORD bytesWritten = 0;
	if (WriteFile(pTxF->hTransactedFile, pv, cb, &bytesWritten, NULL)) {
		if (pcbWritten) *pcbWritten = bytesWritten;
		return S_OK;
	}
	return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT STDMETHODCALLTYPE TxF_Seek(IStream* This, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) {
	TxFStream* pTxF = (TxFStream*)This;
	LARGE_INTEGER newPos;
	DWORD moveMethod;

	switch (dwOrigin) {
	case STREAM_SEEK_SET: moveMethod = FILE_BEGIN; break;
	case STREAM_SEEK_CUR: moveMethod = FILE_CURRENT; break;
	case STREAM_SEEK_END: moveMethod = FILE_END; break;
	default: return STG_E_INVALIDFUNCTION;
	}

	if (SetFilePointerEx(pTxF->hTransactedFile, dlibMove, &newPos, moveMethod)) {
		if (plibNewPosition) plibNewPosition->QuadPart = newPos.QuadPart;
		return S_OK;
	}
	return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT STDMETHODCALLTYPE TxF_SetSize(IStream* This, ULARGE_INTEGER libNewSize) {
	TxFStream* pTxF = (TxFStream*)This;
	LARGE_INTEGER newPos;
	newPos.QuadPart = libNewSize.QuadPart;
	if (SetFilePointerEx(pTxF->hTransactedFile, newPos, NULL, FILE_BEGIN)) {
		if (SetEndOfFile(pTxF->hTransactedFile)) return S_OK;
	}
	return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT STDMETHODCALLTYPE TxF_Stat(IStream* This, STATSTG* pstatstg, DWORD grfStatFlag) {
	if (!pstatstg) return E_POINTER;
	ZeroMemory(pstatstg, sizeof(STATSTG));
	TxFStream* pTxF = (TxFStream*)This;
	LARGE_INTEGER fileSize;
	if (GetFileSizeEx(pTxF->hTransactedFile, &fileSize)) {
		pstatstg->cbSize = *(ULARGE_INTEGER*)&fileSize;
		pstatstg->type = STGTY_STREAM;
		return S_OK;
	}
	return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT STDMETHODCALLTYPE TxF_CopyTo(IStream* This, IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE TxF_Commit(IStream* This, DWORD grfCommitFlags) { return FlushFileBuffers(((TxFStream*)This)->hTransactedFile) ? S_OK : HRESULT_FROM_WIN32(GetLastError()); }
HRESULT STDMETHODCALLTYPE TxF_Revert(IStream* This) { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE TxF_LockRegion(IStream* This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE TxF_UnlockRegion(IStream* This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE TxF_Clone(IStream* This, IStream** ppstm) { return E_NOTIMPL; }

IStream* CreateTransactedStream(HANDLE hFile) {
	TxFStream* pStream = (TxFStream*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TxFStream));
	if (pStream) {
		pStream->lpVtbl = &TxFStream_Vtbl;
		pStream->refCount = 1;
		pStream->hTransactedFile = hFile;
		return (IStream*)pStream;
	}
	return NULL;
}
#endif
// -------------------------------------------------------------------------
// EXIF RATIONAL PARSER
// -------------------------------------------------------------------------

static ULONGLONG ParseToExifRational(HWND hEdit, int precisionMultiplier) {
	wchar_t szBuffer[32] = { 0 };
	GetWindowTextW(hEdit, szBuffer, 32);

	const wchar_t* pszStr = szBuffer;
	unsigned __int64 wholePart = 0;
	unsigned __int64 fracPart = 0;
	unsigned __int64 fracDivisor = 1;
	int decimalFound = 0;

	// Skip any leading whitespace
	while (*pszStr == L' ' || *pszStr == L'\t') pszStr++;

	// Process characters using pure integer math
	while (*pszStr) {
		if (*pszStr >= L'0' && *pszStr <= L'9') {
			if (decimalFound) {
				fracPart = (fracPart * 10) + (*pszStr - L'0');
				fracDivisor *= 10;
			}
			else wholePart = (wholePart * 10) + (*pszStr - L'0');
		}
		else if (*pszStr == L'.') decimalFound = 1;
		else break;
		pszStr++;
	}

	/* Combine whole and fractional parts into a final scaled integer numerator
	   Formula: (wholePart * precisionMultiplier) + ((fracPart * precisionMultiplier) / fracDivisor)
	   Adding (fracDivisor / 2) creates a pure integer round-to-nearest implementation (+0.5)*/
	unsigned __int64 num = wholePart * precisionMultiplier;
	if (decimalFound && fracDivisor > 1) {
		unsigned __int64 dividend = fracPart * precisionMultiplier;
		num += (dividend + (fracDivisor / 2)) / fracDivisor;
	}

	// EXIF Little-Endian layouts place the Numerator first in memory (low 32-bits),
	// and the Denominator second in memory (high 32-bits).
	unsigned __int64 den = (unsigned __int64)precisionMultiplier;
	return (den << 32) | (num & 0xFFFFFFFFULL);
}

/* ------------------------------------------------------------------------ -
					CORE IN-PLACE WIC INJECTION
 -------------------------------------------------------------------------
 Note: This is actually visually lossless despite in theory WIC not being so, it has been tested.*/

void ApplyGeotag(HWND hwndOwner, const wchar_t* pszFilePath) {
	HRESULT hr = S_OK;


# ifdef TXF
	// 1. Ingest Original File into RAM
	HANDLE hFileIn = CreateFileW(pszFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFileIn == INVALID_HANDLE_VALUE) {
		MessageBoxW(hwndOwner, L"Failed to read source image.", L"I/O Error", MB_OK | MB_ICONERROR);
		return;
	}

	DWORD fileSize = GetFileSize(hFileIn, NULL);
	BYTE* pMemBuffer = (BYTE*)HeapAlloc(GetProcessHeap(), 0, fileSize);
	if (!pMemBuffer) {
		CloseHandle(hFileIn);
		MessageBoxW(hwndOwner, L"Insufficient memory to buffer image.", L"Memory Error", MB_OK | MB_ICONERROR);
		return;
	}

	DWORD bytesRead = 0;
	ReadFile(hFileIn, pMemBuffer, fileSize, &bytesRead, NULL);
	CloseHandle(hFileIn); // Target file is now unlocked and ready for transacted truncation

	// Create an IStream from our RAM buffer for the Decoder
	IStream* piMemStream = SHCreateMemStream(pMemBuffer, bytesRead);
	HeapFree(GetProcessHeap(), 0, pMemBuffer); // SHCreateMemStream copies data, free original heap

	if (!piMemStream) {
		MessageBoxW(hwndOwner, L"Failed to initialize memory stream.", L"Memory Error", MB_OK | MB_ICONERROR);
		return;
	}

	// 2. Establish KTM Boundary
	HANDLE hTransaction = CreateTransaction(NULL, 0, 0, 0, 0, 0, L"In-Place WIC Injection");
	if (hTransaction == INVALID_HANDLE_VALUE) {
		IStream_Release(piMemStream);
		MessageBoxW(hwndOwner, L"Failed to create Kernel Transaction.", L"KTM Error", MB_OK | MB_ICONERROR);
		return;
	}

	// 3. Transacted File Truncation (In-Place Overwrite Preparation)
	// We use TRUNCATE_EXISTING so we overwrite the exact same file. 
	// If the transaction rolls back, NTFS restores the pre-truncation data perfectly.
	HANDLE hTransactedFile = CreateFileTransactedW(pszFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL, hTransaction, NULL, NULL);

	if (hTransactedFile == INVALID_HANDLE_VALUE) {
		CloseHandle(hTransaction);
		IStream_Release(piMemStream);
		MessageBoxW(hwndOwner, L"Failed to acquire transacted lock for in-place overwrite.", L"KTM Error", MB_OK | MB_ICONERROR);
		return;
	}

	IStream* piTransactedStream = CreateTransactedStream(hTransactedFile);

	// 4. Initialize WIC COM Objects
	IWICImagingFactory* piFactory = NULL;
	IWICBitmapDecoder* piDecoder = NULL;
	IWICBitmapEncoder* piEncoder = NULL;

	hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&piFactory);

	// Decoder reads from RAM
	if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateDecoderFromStream(piFactory, piMemStream, NULL, WICDecodeMetadataCacheOnDemand, &piDecoder);

	// Encoder pushes to our transacted disk stream
	if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateEncoder(piFactory, &GUID_ContainerFormatJpeg, NULL, &piEncoder);
	if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Initialize(piEncoder, piTransactedStream, WICBitmapEncoderNoCache);
#else

	IWICImagingFactory* piFactory = NULL;
	IWICBitmapDecoder* piDecoder = NULL;
	IWICStream* piFileStream = NULL;
	IWICBitmapEncoder* piEncoder = NULL;
	IWICMetadataBlockWriter* piBlockWriter = NULL;
	IWICMetadataBlockReader* piBlockReader = NULL;

	// Generate a temporary file path to stream the write process safely
	wchar_t szTempPath[MAX_PATH];
	wchar_t szTempFile[MAX_PATH];
	GetTempPathW(MAX_PATH, szTempPath);
	GetTempFileNameW(szTempPath, L"GEO", 0, szTempFile);

	// 1. Create Imaging Factory
	hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&piFactory);

	// 2. Load Source File Decoder
	if (SUCCEEDED(hr)) {
		hr = IWICImagingFactory_CreateDecoderFromFilename(piFactory, pszFilePath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &piDecoder);
	}

	// 3. Create and Initialize Destination Write Stream
	if (SUCCEEDED(hr)) {
		hr = IWICImagingFactory_CreateStream(piFactory, &piFileStream);
	}
	if (SUCCEEDED(hr)) {
		hr = IWICStream_InitializeFromFilename(piFileStream, szTempFile, GENERIC_WRITE);
	}

	// 4. Create and Initialize Destination Encoder
	if (SUCCEEDED(hr)) {
		hr = IWICImagingFactory_CreateEncoder(piFactory, &GUID_ContainerFormatJpeg, NULL, &piEncoder);
	}
	if (SUCCEEDED(hr)) {
		hr = IWICBitmapEncoder_Initialize(piEncoder, (IStream*)piFileStream, WICBitmapEncoderNoCache);
#endif

		UINT frameCount = 0;
		if (SUCCEEDED(hr)) hr = IWICBitmapDecoder_GetFrameCount(piDecoder, &frameCount);

		for (UINT i = 0; i < frameCount && SUCCEEDED(hr); i++) {
			IWICBitmapFrameDecode* piFrameDecode = NULL;
			IWICBitmapFrameEncode* piFrameEncode = NULL;
			IWICMetadataQueryWriter* piFrameQWriter = NULL;
#ifdef TXF
			IWICMetadataBlockReader* piBlockReader = NULL;
			IWICMetadataBlockWriter* piBlockWriter = NULL;
#endif
			UINT width = 0, height = 0;
			double dpiX = 0.0, dpiY = 0.0;
			WICPixelFormatGUID pixelFormat = { 0 };

			if (SUCCEEDED(hr)) hr = IWICBitmapDecoder_GetFrame(piDecoder, i, &piFrameDecode);
			if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_CreateNewFrame(piEncoder, &piFrameEncode, NULL);
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Initialize(piFrameEncode, NULL);

			// Mirror Frame Geometry
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameDecode_GetSize(piFrameDecode, &width, &height);
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetSize(piFrameEncode, width, height);
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameDecode_GetResolution(piFrameDecode, &dpiX, &dpiY);
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetResolution(piFrameEncode, dpiX, dpiY);
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameDecode_GetPixelFormat(piFrameDecode, &pixelFormat);
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetPixelFormat(piFrameEncode, &pixelFormat);

			// Clone Structural Metadata
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameDecode_QueryInterface(piFrameDecode, &IID_IWICMetadataBlockReader, (void**)&piBlockReader);
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_QueryInterface(piFrameEncode, &IID_IWICMetadataBlockWriter, (void**)&piBlockWriter);
			if (SUCCEEDED(hr)) hr = IWICMetadataBlockWriter_InitializeFromBlockReader(piBlockWriter, piBlockReader);

			if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_GetMetadataQueryWriter(piFrameEncode, &piFrameQWriter);

			if (SUCCEEDED(hr)) {
				PROPVARIANT subIfd;
				PropVariantInit(&subIfd);
				subIfd.vt = VT_UNKNOWN;
				IWICMetadataQueryWriter_SetMetadataByName(piFrameQWriter, L"/app1/ifd/gps/ {}", &subIfd);
			}

			// Inject Latitude
			if (SUCCEEDED(hr)) {
				wchar_t szDir[4] = { 0 };
				GetWindowTextW(hLatDir, szDir, 4);

#ifndef TXF
				wchar_t* pszCleanDir = L"N";
				if (szDir[0] == L'S' || szDir[0] == L's') pszCleanDir = L"S";
#endif

				PROPVARIANT latRef;
				PropVariantInit(&latRef);
				latRef.vt = VT_LPWSTR;
#ifdef TXF
				latRef.pwszVal = (szDir[0] == L'S' || szDir[0] == L's') ? L"S" : L"N";
#else
				latRef.pwszVal = pszCleanDir;
#endif
				hr = IWICMetadataQueryWriter_SetMetadataByName(piFrameQWriter, L"/app1/ifd/gps/ {ushort=1}", &latRef);
			}
			if (SUCCEEDED(hr)) {
				ULONGLONG latValues[3];
				latValues[0] = ParseToExifRational(hLatDeg, 1);
				latValues[1] = ParseToExifRational(hLatMin, 1);
				latValues[2] = ParseToExifRational(hLatSec, 1000);

				PROPVARIANT latData;
				PropVariantInit(&latData);
				latData.vt = VT_VECTOR | VT_UI8;
				latData.cauh.pElems = (ULARGE_INTEGER*)latValues;
				latData.cauh.cElems = 3;
				hr = IWICMetadataQueryWriter_SetMetadataByName(piFrameQWriter, L"/app1/ifd/gps/ {ushort=2}", &latData);
				latData.vt = VT_EMPTY;
			}

			// Inject Longitude
			if (SUCCEEDED(hr)) {
				wchar_t szDir[4] = { 0 };
				GetWindowTextW(hLonDir, szDir, 4);
#ifndef TXF
				wchar_t* pszCleanDir = L"E";
				if (szDir[0] == L'W' || szDir[0] == L'w') pszCleanDir = L"W";
#endif
				PROPVARIANT lonRef;
				PropVariantInit(&lonRef);
				lonRef.vt = VT_LPWSTR;
#ifdef TXF
				lonRef.pwszVal = (szDir[0] == L'W' || szDir[0] == L'w') ? L"W" : L"E";
#else
				lonRef.pwszVal = pszCleanDir;
#endif
				hr = IWICMetadataQueryWriter_SetMetadataByName(piFrameQWriter, L"/app1/ifd/gps/ {ushort=3}", &lonRef);
			}
			if (SUCCEEDED(hr)) {
				ULONGLONG lonValues[3];
				lonValues[0] = ParseToExifRational(hLonDeg, 1);
				lonValues[1] = ParseToExifRational(hLonMin, 1);
				lonValues[2] = ParseToExifRational(hLonSec, 1000);

				PROPVARIANT lonData;
				PropVariantInit(&lonData);
				lonData.vt = VT_VECTOR | VT_UI8;
				lonData.cauh.pElems = (ULARGE_INTEGER*)lonValues;
				lonData.cauh.cElems = 3;
				hr = IWICMetadataQueryWriter_SetMetadataByName(piFrameQWriter, L"/app1/ifd/gps/ {ushort=4}", &lonData);
				lonData.vt = VT_EMPTY;
			}
			// Finalize loss-less frame copy pipeline
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_WriteSource(piFrameEncode, (IWICBitmapSource*)piFrameDecode, NULL);
			if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Commit(piFrameEncode);

#ifdef TXF
			if (piBlockReader) IWICMetadataBlockReader_Release(piBlockReader);
			if (piBlockWriter) IWICMetadataBlockWriter_Release(piBlockWriter);
#endif

			if (piFrameDecode) IWICBitmapFrameDecode_Release(piFrameDecode);
			if (piFrameEncode) IWICBitmapFrameEncode_Release(piFrameEncode);
			if (piFrameQWriter) IWICMetadataQueryWriter_Release(piFrameQWriter);
		}

		// Commit File Architectures
		if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Commit(piEncoder);

		// 5. Cleanup Handles
#ifndef TXF
		if (SUCCEEDED(hr)) hr = IWICStream_Commit(piFileStream, STGC_DEFAULT);
		if (piBlockReader) IWICMetadataBlockReader_Release(piBlockReader);
		if (piBlockWriter) IWICMetadataBlockWriter_Release(piBlockWriter);
#endif
		if (piEncoder) IWICBitmapEncoder_Release(piEncoder);
#ifndef TXF
		if (piFileStream) IWICStream_Release(piFileStream);
#endif
		if (piDecoder) IWICBitmapDecoder_Release(piDecoder);
		if (piFactory) IWICImagingFactory_Release(piFactory);
#ifdef TXF
		if (piMemStream) IStream_Release(piMemStream);

		// This invokes TxF_Release, dropping refCount to 0, and calling CloseHandle(hTransactedFile)
		if (piTransactedStream) IStream_Release(piTransactedStream);

		// 6. Transacted Execution
		if (SUCCEEDED(hr)) {
			if (CommitTransaction(hTransaction)) {
				MessageBoxW(hwndOwner, L"Geotags successfully injected in-place via TxF!", L"Success", MB_OK | MB_ICONINFORMATION);
			}
			else {
				DWORD dwCommitError = GetLastError();
				RollbackTransaction(hTransaction);
				wchar_t szErrorMsg[256];
				wsprintfW(szErrorMsg, L"TxF Commit failed. In-place data safely reverted.\nOS Error Code: %lu", dwCommitError);
				MessageBoxW(hwndOwner, szErrorMsg, L"Transaction Error", MB_OK | MB_ICONERROR);
			}
		}
		else {
			RollbackTransaction(hTransaction);
			wchar_t szWicError[128];
			wsprintfW(szWicError, L"WIC serialization failed. File reverted to original state.\nHRESULT: 0x%08X", hr);
			MessageBoxW(hwndOwner, szWicError, L"WIC Failure", MB_OK | MB_ICONERROR);
		}

		CloseHandle(hTransaction);
}
#else
	}
	if (SUCCEEDED(hr)) {
		if (ReplaceFileW(pszFilePath, szTempFile, NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
			MessageBoxW(hwndOwner, L"Geotags successfully embedded directly into image metadata!", L"Success", MB_OK | MB_ICONINFORMATION);
		}
		else {
			DWORD dwError = GetLastError();
			wchar_t szErrorMsg[256];

			if (dwError == ERROR_ACCESS_DENIED) {
				wsprintfW(szErrorMsg, L"Access Denied (Error 5).\n\nThe file is either Read-Only, locked by another program, or tucked inside a protected folder.");
			}
			else {
				wsprintfW(szErrorMsg, L"Windows CopyFileW failed with OS Error Code: %lu", dwError);
			}

			MessageBoxW(hwndOwner, szErrorMsg, L"Write Error", MB_OK | MB_ICONERROR);
		}
	}
	else {
		wchar_t szWicError[128];
		wsprintfW(szWicError, L"An internal error occurred writing WIC EXIF blocks.\nHRESULT: 0x%08X", hr);
		MessageBoxW(hwndOwner, szWicError, L"WIC Failure", MB_OK | MB_ICONERROR);
	}

	DeleteFileW(szTempFile);
}
#endif
