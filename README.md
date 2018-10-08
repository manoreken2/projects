Manoretimedia Kenkyuujyo project files
==============

How to build
--------------

Use cmake-gui to create Visual studio solution and projects.
Open solution file using Visual studio and build. right click project and select set as startup projects and run.


DngHeaderDump project
--------------

DngHeaderDump is a console app to read and show all IFD header info of the specified DNG file.

It shows your camera name, camera serial number, image number, lens name, exposure settings, battery level, GPS location, GPS time and so on.

DNG (Digital Negative) file can be created from digital camera Raw files (CR2, NEF, ARW ...), by converting to DNG file using free Adobe DNG Converter.


ImageToDng project
--------------

Windows program to convert image to 8bit RAW CFA(Color Filter Array) DNG format.

Conversion is lossy process: Bayer pattern is applied to the image, green channel resolution is reduced by √2 x, red and blue channel resolution is reduced by 2x. Please refer this document: https://en.wikipedia.org/wiki/Bayer_filter

Also I omitted low-pass prefilter that should be applied before applying Bayer pattern, so aliasing artifact may appear.

To show this image, it is necessary to demosaic it to recover RGB image.
Dng image can be opened by RAW image development software such as RawTherapee.

following Bayer patterns are available:
  * RG/GB
  * BG/GR
  * GR/BG
  * GB/RG

Bugs: 
  * Thumbnail image is wrong.


BMRawYuv420p10ToDng project
--------------

Reads Blackmagic Micro Studio Camera 4k 12bit raw image data encoded in yuv420p10le file and writes it as 12bit Raw DNG file. yuv420p10le file can be created using ffmpeg.

  
  