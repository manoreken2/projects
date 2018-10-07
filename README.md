Manoretimedia Kenkyuujyo project files
==============


DngHeaderDump
--------------

DngHeaderDump is a console app to read and show all IFD header info of the specified DNG file.

It shows your camera name, camera serial number, image number, lens name, exposure settings, battery level, GPS location, GPS time and so on.

DNG (Digital Negative) file can be created from digital camera Raw files (CR2, NEF, ARW ...), by converting to DNG file using free Adobe DNG Converter.


ImageToDng
--------------

ImageToDng is Windows program to convert image to 8bit RAW CFA(Color Filter Array) DNG format.

Conversion is lossy process: Bayer pattern is applied to the image, green channel resolution is reduced by √2 x, red and blue channel resolution is reduced by 2x.

Also I omitted low-pass prefilter that should be applied before applying Bayer pattern, so aliasing artifact may appear.

To show this image, it is necessary to demosaic it to recover RGB image.
Dng image can be opened by RAW image development software such as RawTherapee.

following Bayer patterns are available:
  * RG/GB
  
| R | G | R | G | … |
| - | - | - | - | - |
| G | B | G | B | … |
| - | - | - | - | - |
| R | G | R | G | … |
| - | - | - | - | - |
| G | B | G | B | … |
| - | - | - | - | - |
| … | … | … | … | … |

  * BG/GR
  
| B | G | B | G | … |
| - | - | - | - | - |
| G | R | G | R | … |
| - | - | - | - | - |
| B | G | B | G | … |
| - | - | - | - | - |
| G | R | G | R | … |
| - | - | - | - | - |
| … | … | … | … | … |

  * GR/BG
  
| G | R | G | R | … |
| - | - | - | - | - |
| B | G | B | G | … |
| - | - | - | - | - |
| G | R | G | R | … |
| - | - | - | - | - |
| B | G | B | G | … |
| - | - | - | - | - |
| … | … | … | … | … |

  * GB/RG
  
| G | B | G | B | … |
| - | - | - | - | - |
| R | G | R | G | … |
| - | - | - | - | - |
| G | B | G | B | … |
| - | - | - | - | - |
| R | G | R | G | … |
| - | - | - | - | - |
| … | … | … | … | … |

Bugs: 
  * Thumbnail image is wrong.


BMRawYuv420p10ToDngImageToDng
--------------

Reads Blackmagic Micro Studio Camera 4k 12bit raw image data encoded in YUV420p10 file and writes it as 12bit Raw DNG file.

  
  