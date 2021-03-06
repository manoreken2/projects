HDR10Capture project : HDR10 Image Viewer / video Capture app
--------------

Enable HDR on Windows Display Settings to display images with HDR10. HDR10 PQ Image is displayed on HDR10 monitor connected to Windows PC. 

Image displaying functionality: supports 24bit/32bit/48bit BMP, 24bit/48bit PNG and Half float OpenEXR files. Drop image file to App to display it.

Video capture functionality: App works with DeckLink devices. Tested on DeckLink Mini recorder 4K. Captured video is saved as Uncompressed AVI. Screenshot can be saved as OpenEXR image.

### About AVI file output of HDR10Capture

Tested input video format is RGB 10bit r210, RGB 12bit R12B, and YUV422 10bit v210.

  * RGB video is saved as r210 AVI.
  * YUV video is saved as v210 AVI.

Output AVI file does not have index chunk and not completed. Mencoder can fix the index. Example:

> mencoder output.avi -ovc copy -oac copy -o fixed.avi

r210 or v210 AVI can be converted to OpenEXR image sequence using AviToExrSequence.

### How to build

Decklink SDK 11.7 is necessary to build.
Copy Blackmagic DeckLink SDK 11.7\Win\include\* to MLDecklinkCapture\Decklink directory.
Open MLDX12VideoCapture\MLDX12VideoCapture.sln using Visual Studio 2019 and build.

AviToExrSequence project
--------------

Read Uncompressed RGB 10bit "r210" AVI, extract HDR10 image and write OpenEXR image sequence.

### How to build

Open AviToExrSequence.sln using Visual Studio 2019 and build.


HDR10Viewer2 project
--------------

Sends HDR10 image to HDR10 display using DirectX12 API.

### How to build

Open HDR10Viewer2.sln with Visual Studio 2019 and build.

  * Note 1: The following screenshot image is converted to SDR and brighter part is saturated to white.
  * Note 2: The OpenEXR image is generated with Blender, the scene file is "Class room By Christophe Seux" https://www.blender.org/download/demo-files/#cycles
  ![ScreenShot](HDR10Viewer2/HDR10Viewer2_100_Screenshot.png)

MLDX12VideoCapture project
--------------

Video capture program using DirectX12 and Decklink Mini recorder 4K.

It can preview Raw SDI stream of Blackmagic Micro Studio Camera 4K.
Please use BMRawAVIv210ToDng to convert from recorded Raw SDI yuv v210 AVI file to Cinema DNG.

How to build

Decklink SDK 10.11.4 is necessary to build.
Copy Blackmagic DeckLink SDK 10.11.4\Win\include\* to MLDecklinkCapture\Decklink directory.
Open MLDX12VideoCapture\MLDX12VideoCapture.sln using Visual Studio 2019 and build.

### About output AVI file of MLDX12VideoCapture 

Currently only YUV 10bit v210 format can be recorded.

Output AVI file does not have index chunk and not completed. Mencoder can fix the index. Example:

> mencoder -idx output.avi -ovc copy -oac copy -o fixed.avi

  ![ScreenShot](MLDX12VideoCapture/ScreenShot.png)
  
  
How to build ImageToDng, DngHeaderDump etc.
--------------

Use cmake-gui to create Visual Studio solution and projects.
Open solution file using Visual Studio and build. right click project and select set as startup projects and run.


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


DngHeaderDump project
--------------

DngHeaderDump is a console app to read and show all IFD header info of the specified DNG file.

It shows your camera name, camera serial number, image number, lens name, exposure settings, battery level, GPS location, GPS time and so on.

DNG (Digital Negative) file can be created from digital camera Raw files (CR2, NEF, ARW ...), by converting to DNG file using free Adobe DNG Converter.


BMRawYuv422p10ToDng project
--------------

Reads Blackmagic Micro Studio Camera 4k 12bit raw image data encoded in yuv422p10le file and writes it as 12bit Raw DNG file.

yuv422p10le file can be created using ffmpeg.


