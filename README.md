# Simple-Exif-geotagging-tool
A simple native Win32 Exif geotagging tool written in C and using WIC as the backend.
<img width="570" height="195" alt="image" src="https://github.com/user-attachments/assets/2816d41a-9baa-4f5e-ade4-836c94552018" />

## Features:
- Smart Clipboard. Allows you to paste in 78°51'46.6"N 86°32'49.1"E or even 78°51'46.6"N, 86°32'49.1"E without having to perform special clipboard manipulation
- Uses the Windows Imaging Component for reliability.
- Atomic I/O for safety.
- Entirely native. Does not use the C runtime except for WIC.
- Completely high DPI aware.
- Drag and drop support via COM to ensure drag and drop from any application is supported.
- Drag and drop support in Windows Explorer (draging files on top of the exe).
- Optional Transactional NTFS (TxF) support at compile time (see wic_geotag.c). Disabled by default.

 ## Known issues:
 - Not truly CRT free due to WIC, might be possible with stubs. Might not even be needed as if a system doesn't have the CRT WIC probably wouldn't even work.
 - If you backspace into a box and try to type in the next box it could say that the value is too high. Might fix later. **Easy workaround: Just tab into the next box.**
 - Compiles all the way down to Vista but the WIC backend fails on anything lower than Windows 10. Not sure why.
 - DPI awareness for multi-monitor setups is untested as I don't have such a setup.
 - Technically not lossless but it is visually lossless. This has been confirmed with tests

## License:

Copyright © 2026 Leonard Matthew Teyssier. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
3. All advertising materials mentioning features or use of this software must display the following acknowledgement:
This product includes software developed by Leonard Matthew Teyssier.
4. Neither the name of Leonard Matthew Teyssier nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY LEONARD MATTHEW TEYSSIER "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LEONARD MATTHEW TEYSSIER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#### Why use the BSD 4-clause license?
To keep this program small and light there is no about box, that would probably add an extra 100 lines of code. I feel like the BSD 4-clause provides a stronger requirement for credit than the BSD 3-clause.
##### B-b-bbut it's incompatible with the GPL!
So? This source code is pretty useless beyond maintenance. It is entirely native to Windows and cannot be ran on any POSIX system. Even if it could, this can be trivially rewritten.

## Other information:
This is my first time ever programming anything for windows. I know I probably made some mistakes but I am fairly confident in its quality for a first project. I did use some LLM assistants to help with some of the writing as Microsoft's C documentation is lackluster. I do want to make it clear this is **not** vibecoded slop. It took me several days to write with constant corrections and hand written code.
