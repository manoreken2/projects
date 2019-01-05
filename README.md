Manoretimedia Kenkyuujyo project files
==============

How to build
--------------

Use cmake-gui to create Visual Studio solution and projects.
Open solution file using Visual Studio and build. right click project and select set as startup projects and run.


DngHeaderDump project
--------------

DngHeaderDump is a console app to read and show all IFD header info of the specified DNG file.

It shows your camera name, camera serial number, image number, lens name, exposure settings, battery level, GPS location, GPS time and so on.

DNG (Digital Negative) file can be created from digital camera Raw files (CR2, NEF, ARW ...), by converting to DNG file using free Adobe DNG Converter.


ImageToDng project
--------------

Windows program to convert image to 8bit RAW CFA(Color Filter Array) DNG format.

Conversion is lossy process: Bayer pattern is applied to the image, green channel resolution is reduced by √2 x, red and blue channel resolution is reduced by 2x. Please refer this document: https://en.wikipedia.org/wiki/Bayer_filter

Also low-pass prefilter that should be applied before applying Bayer pattern is omitted, so aliasing artifact may appear.

To show this image, it is necessary to demosaic it to recover RGB image.
Dng image can be opened by RAW image development software such as RawTherapee.

Following Bayer patterns are available:
  * RG/GB
  * BG/GR
  * GR/BG
  * GB/RG

Bugs: 
  * Thumbnail image is wrong.


BMRawYuv422p10ToDng project
--------------

Reads Blackmagic Micro Studio Camera 4k 12bit raw image data encoded in yuv422p10le file and writes it as 12bit Raw DNG file.

yuv422p10le file can be created using ffmpeg.


MLDX12VideoCapture project
--------------

Video capture program using DirectX12, Decklink Mini recorder 4K and Decklink SDK 10.11.4 

  
  