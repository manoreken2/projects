Manoretimedia Kenkyuujyo project files
==============

ImageToDng
--------------

ImageToDng is Windows program to convert image to 8bit RAW DNG format.

Conversion is lossy process: Bayer pattern is applied to image, Green channel resolution is reduced by √2 x, Red and Blue channel resolution is reduced by 2x.

And I omitted low-pass prefilter that should be applied before applying Bayer pattern, so aliasing artifact may appear.

Dng image can be opened by RAW image development software such as RawTherapee.

Bugs: 
  * Thumbnail image is wrong.
