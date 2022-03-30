This is a recruitment task. The requirement was to do some specific image manipulation operations.  
It is supposed to flip R & B colors, flip image vertically and change saturation (it should oversaturate or desaturate depending on float saturation value).


I started with some exploration of different techniques for image saturation.  
I implemented converting to HSV (1) & HSL (2) color spaces and back - and changing the saturation there.  
Then I found an approach on the internet where you calculate relative luminance of a color (3).  
Then you simply multiply the difference between a gray scale version and color version by the saturation value.  
My understanding is that this should be done in linear color space, (4) so I added this version as well.  
The linear luminance version and HSL version give similar results.  
I implemented all-in-one convert_image function using technique (3) for saturation.  
It is the least correct one but it was the fastest, looks mostly OK and was the easiest to implement for my AVX version.  
convert_image is implemented both as normal C++ code and has an analogous version in manually written AVX intrinsics.  
I picked AVX because it is the newest SIMD extension that my CPU supports (Ivy Bridge).  
AVX is a little weird because it doesn't support 256 bit wide integer operations so this code is a mix of AVX and SSE.  


Controls:  
- C - convert_image()  
- B - benchmark - calls convert_image() 245 times  
- R - reload image.png from disk  
- [ - decrease saturation variable by 0.1  
- ] - increase saturation variable by 0.1  
- BACKSPACE - reset saturation variable to 1.0  
- 1 - swap Red and Blue bytes  
- 2 - flip image vertically  
- 3 - saturate image in HSV space  
- 4 - saturate image in HSL space  
- 5 - saturate image using relative luminance  
- 6 - saturate image using relative luminance in linear color space  