//© 2026 Leonard Matthew Teyssier BSD-4 Clause License

#ifndef UNICODE
#define UNICODE
#endif

#define COBJMACROS
#pragma comment(lib, "windowscodecs.lib")

#include <windows.h>

//#define memset(dest, c, count) RtlZeroMemory(dest, count)

#include <wincodec.h>
#include <wincodecsdk.h>

#include "main.h"
#include "wic_geotag.h"


/*
// Explicitly define the 3 missing WIC GUIDs to bypass all header quirks
const GUID IID_IWICImagingFactory = { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0x47, 0x0d } };
//const GUID IID_IWICMetadataBlockReader = { 0xfe442a19, 0xac9d, 0x4584, { 0xac, 0xb8, 0xd2, 0xbd, 0xc9, 0xfa, 0xfe, 0x11 } };
const GUID IID_IWICMetadataBlockReader = { 0xfeaa2a8d, 0xb3f3, 0x43e4, { 0xb2, 0x5c, 0xd1, 0xde, 0x99, 0x0a, 0x1a, 0xe1 } };
const GUID IID_IWICMetadataBlockWriter = { 0x08107dd0, 0xba82, 0x47d4, { 0xac, 0x31, 0x00, 0xbc, 0x10, 0x24, 0x0e, 0x88 } };
*/


static ULONGLONG ParseToExifRational(HWND hEdit, int precisionMultiplier) {
	wchar_t szBuffer[32] = { 0 };
	GetWindowTextW(hEdit, szBuffer, 32);

	const wchar_t* pszStr = szBuffer;
	unsigned __int64 wholePart = 0;
	unsigned __int64 fracPart = 0;
	unsigned __int64 fracDivisor = 1;
	int decimalFound = 0;

	// Skip any leading whitespace
	while (*pszStr == L' ' || *pszStr == L'\t') {
		pszStr++;
	}

	// Process characters using pure integer math
	while (*pszStr) {
		if (*pszStr >= L'0' && *pszStr <= L'9') {
			if (decimalFound) {
				// Keep tracking digits after the decimal point as integers
				fracPart = (fracPart * 10) + (*pszStr - L'0');
				fracDivisor *= 10;
			}
			else {
				wholePart = (wholePart * 10) + (*pszStr - L'0');
			}
		}
		else if (*pszStr == L'.') {
			decimalFound = 1;
		}
		else {
			break;
		}
		pszStr++;
	}

	// Combine whole and fractional parts into a final scaled integer numerator
	// Formula: (wholePart * precisionMultiplier) + ((fracPart * precisionMultiplier) / fracDivisor)
	// Adding (fracDivisor / 2) creates a pure integer round-to-nearest implementation (+0.5)
	unsigned __int64 num = wholePart * precisionMultiplier;
	if (decimalFound && fracDivisor > 1) {
		unsigned __int64 dividend = fracPart * precisionMultiplier;
		num += (dividend + (fracDivisor / 2)) / fracDivisor;
	}

	unsigned __int64 den = (unsigned __int64)precisionMultiplier;

	// EXIF Little-Endian layouts place the Numerator first in memory (low 32-bits),
	// and the Denominator second in memory (high 32-bits).
	return (den << 32) | (num & 0xFFFFFFFFULL);
}

void ApplyGeotag(HWND hwndOwner, const wchar_t* pszFilePath) {
	HRESULT hr = S_OK;

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
	}

	// 5. Read Frame Count and Process
	UINT frameCount = 0;
	if (SUCCEEDED(hr)) {
		hr = IWICBitmapDecoder_GetFrameCount(piDecoder, &frameCount);
	}

	for (UINT i = 0; i < frameCount && SUCCEEDED(hr); i++) {
		IWICBitmapFrameDecode* piFrameDecode = NULL;
		IWICBitmapFrameEncode* piFrameEncode = NULL;
		IWICMetadataQueryWriter* piFrameQWriter = NULL;

		UINT width = 0, height = 0;
		double dpiX = 0.0, dpiY = 0.0;
		WICPixelFormatGUID pixelFormat = { 0 };

		if (SUCCEEDED(hr)) hr = IWICBitmapDecoder_GetFrame(piDecoder, i, &piFrameDecode);
		if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_CreateNewFrame(piEncoder, &piFrameEncode, NULL);
		if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Initialize(piFrameEncode, NULL);

		// Copy Properties
		if (SUCCEEDED(hr)) hr = IWICBitmapFrameDecode_GetSize(piFrameDecode, &width, &height);
		if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetSize(piFrameEncode, width, height);
		if (SUCCEEDED(hr)) hr = IWICBitmapFrameDecode_GetResolution(piFrameDecode, &dpiX, &dpiY);
		if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetResolution(piFrameEncode, dpiX, dpiY);
		if (SUCCEEDED(hr)) hr = IWICBitmapFrameDecode_GetPixelFormat(piFrameDecode, &pixelFormat);
		if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetPixelFormat(piFrameEncode, &pixelFormat);

		// Clone Existing Structural Metadata Block Blocks
		if (SUCCEEDED(hr)) hr = IWICBitmapFrameDecode_QueryInterface(piFrameDecode, &IID_IWICMetadataBlockReader, (void**)&piBlockReader);
		if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_QueryInterface(piFrameEncode, &IID_IWICMetadataBlockWriter, (void**)&piBlockWriter);
		if (SUCCEEDED(hr)) hr = IWICMetadataBlockWriter_InitializeFromBlockReader(piBlockWriter, piBlockReader);

		// Access Frame Query Writer for GPS Injection
		if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_GetMetadataQueryWriter(piFrameEncode, &piFrameQWriter);

		// Initialize GPS Sub-IFD block if absent
		if (SUCCEEDED(hr)) {
			PROPVARIANT subIfd;
			PropVariantInit(&subIfd);
			subIfd.vt = VT_UNKNOWN;
			IWICMetadataQueryWriter_SetMetadataByName(piFrameQWriter, L"/app1/ifd/gps/ {}", &subIfd);
		}

		// --- WRITE LATITUDE ---
		if (SUCCEEDED(hr)) {
			wchar_t szDir[4] = { 0 };
			GetWindowTextW(hLatDir, szDir, 4);

			wchar_t* pszCleanDir = L"N";
			if (szDir[0] == L'S' || szDir[0] == L's') pszCleanDir = L"S";

			PROPVARIANT latRef;
			PropVariantInit(&latRef);
			latRef.vt = VT_LPWSTR;
			latRef.pwszVal = pszCleanDir;
			hr = IWICMetadataQueryWriter_SetMetadataByName(piFrameQWriter, L"/app1/ifd/gps/ {ushort=1}", &latRef);
		}
		if (SUCCEEDED(hr)) {
			ULONGLONG latValues[3];
			latValues[0] = ParseToExifRational(hLatDeg, 1);    // Degrees
			latValues[1] = ParseToExifRational(hLatMin, 1);    // Minutes
			latValues[2] = ParseToExifRational(hLatSec, 1000); // Seconds (3 decimal places)

			PROPVARIANT latData;
			PropVariantInit(&latData);
			latData.vt = VT_VECTOR | VT_UI8;
			latData.cauh.pElems = (ULARGE_INTEGER*)latValues;
			latData.cauh.cElems = 3;

			hr = IWICMetadataQueryWriter_SetMetadataByName(piFrameQWriter, L"/app1/ifd/gps/ {ushort=2}", &latData);
			latData.vt = VT_EMPTY;
		}

		// --- WRITE LONGITUDE ---
		if (SUCCEEDED(hr)) {
			wchar_t szDir[4] = { 0 };
			GetWindowTextW(hLonDir, szDir, 4);

			wchar_t* pszCleanDir = L"E";
			if (szDir[0] == L'W' || szDir[0] == L'w') pszCleanDir = L"W";

			PROPVARIANT lonRef;
			PropVariantInit(&lonRef);
			lonRef.vt = VT_LPWSTR;
			lonRef.pwszVal = pszCleanDir;
			hr = IWICMetadataQueryWriter_SetMetadataByName(piFrameQWriter, L"/app1/ifd/gps/ {ushort=3}", &lonRef);
		}
		if (SUCCEEDED(hr)) {
			ULONGLONG lonValues[3];
			lonValues[0] = ParseToExifRational(hLonDeg, 1);    // Degrees
			lonValues[1] = ParseToExifRational(hLonMin, 1);    // Minutes
			lonValues[2] = ParseToExifRational(hLonSec, 1000); // Seconds (3 decimal places)

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

		if (piFrameDecode) IWICBitmapFrameDecode_Release(piFrameDecode);
		if (piFrameEncode) IWICBitmapFrameEncode_Release(piFrameEncode);
		if (piFrameQWriter) IWICMetadataQueryWriter_Release(piFrameQWriter);
	}

	// 6. Commit File Architectures
	if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Commit(piEncoder);
	if (SUCCEEDED(hr)) hr = IWICStream_Commit(piFileStream, STGC_DEFAULT);

	// 7. Cleanup Handles
	if (piBlockReader) IWICMetadataBlockReader_Release(piBlockReader);
	if (piBlockWriter) IWICMetadataBlockWriter_Release(piBlockWriter);
	if (piEncoder) IWICBitmapEncoder_Release(piEncoder);
	if (piFileStream) IWICStream_Release(piFileStream);
	if (piDecoder) IWICBitmapDecoder_Release(piDecoder);
	if (piFactory) IWICImagingFactory_Release(piFactory);

	// 8. Swap files or report failure UI feedback
	if (SUCCEEDED(hr)) {
		if (CopyFileW(szTempFile, pszFilePath, FALSE)) {
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
