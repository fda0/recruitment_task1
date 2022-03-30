#pragma comment(lib, "User32.lib") // basic windows functionality
#pragma comment(lib, "Gdi32.lib") // gdi graphics
#pragma comment(lib, "UxTheme.lib") // for gdi backbuffer

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include "uxtheme.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#include "inttypes.h"
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef s8 b8; // booleans
typedef s16 b16;
typedef s64 b64;
typedef s32 b32;



#define debug_break() do{if(IsDebuggerPresent()) {fflush(stdout); __debugbreak();}}while(0)
// Usually I enable my asserts for non-shipping builds only
#define assert(Expression) do{ if(!(Expression)) { debug_break(); *((s32 volatile*)0) = 1; ExitProcess(1); }}while(0)

#define pick_smaller(a, b) (((a) > (b)) ? (b) : (a))
#define pick_bigger(a, b) (((a) > (b)) ? (a) : (b))
#define array_count(a) ((sizeof(a))/(sizeof(*a)))




////////////////////////////////
union v3
{
    struct {f32 x, y, z;};
    struct {f32 r, g, b;};
    f32 e[3];
};

static v3 operator*(f32 a, v3 b)
{
    v3 result = {
        a * b.x,
        a * b.y,
        a * b.z,
    };
    return result;
}

static v3 operator*(v3 a, f32 b)
{
    v3 result = b * a;
    return result;
}

static v3 operator-(v3 a)
{
    v3 result = {
        -a.x,
        -a.y,
        -a.z,
    };
    return result;
}

static v3 operator+(v3 a, v3 b)
{
    v3 result = {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
    };
    return result;
}

static v3 operator-(v3 a, v3 b)
{
    v3 result = {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z,
    };
    return result;
}

static v3 operator*=(v3 &a, f32 b)
{ 
    a = b * a;
    return a;
}

static v3 operator+=(v3 &a, v3 b)
{
    a = a + b;
    return a;   
}

static v3 operator-=(v3 &a, v3 b)
{
    a = a - b;
    return a;
}



static s32 clamp(s32 min, s32 value, s32 max)
{
    s32 result = value;
    if (result < min) { result = min; }
    if (result > max) { result = max; }
    return result;
}
static f32 clamp(f32 min, f32 value, f32 max)
{
    f32 result = value;
    if (result < min) { result = min; }
    if (result > max) { result = max; }
    return result;
}
static f32 clamp01(f32 value)
{
    return clamp(0.0f, value, 1.0f);
}
static v3 clamp01(v3 value)
{
    v3 result = {
        clamp01(value.x),
        clamp01(value.y),
        clamp01(value.z),
    };
    return result;
}




////////////////////////////////
static s64 time_perf()
{
    LARGE_INTEGER large;
    QueryPerformanceCounter(&large);
    s64 result = large.QuadPart;
    return result;
}

static f32 time_elapsed(s64 recent, s64 old)
{
    LARGE_INTEGER perfomance_freq;
    QueryPerformanceFrequency(&perfomance_freq);
    f32 inv_freq = 1.f / (f32)perfomance_freq.QuadPart;
    
    s64 delta = recent - old;
    f32 result = ((f32)delta * inv_freq);
    return result;
}




////////////////////////////////
struct Gdi_Buffer
{
    u32 *memory;
    u32 width, height;
    BITMAPINFO info;
};

static void display_gdi_buffer(HWND window, HDC device_context, Gdi_Buffer *buffer, char *text)
{
    RECT client_rect;
    GetClientRect(window, &client_rect);
    u32 client_width = client_rect.right;
    u32 client_height = client_rect.bottom;
    
    if (client_width && client_height)
    {
        // keep the aspect ratio
        f32 ratio_width = (f32)buffer->width / (f32)client_width;
        f32 ratio_height = (f32)buffer->height / (f32)client_height;
        if (ratio_width > ratio_height)
        {
            ratio_height /= ratio_width;
            ratio_width = 1.f;
        }
        else
        {
            ratio_width /= ratio_height;
            ratio_height = 1.f;
        }
        
        u32 target_width = (u32)(client_width*ratio_width);
        u32 target_height = (u32)(client_height*ratio_height);
        
        // buffered paint to back buffer because otherwise GDI is flickering
        HDC buffered_context = {};
        HPAINTBUFFER paint_buffer = BeginBufferedPaint(device_context, &client_rect,
                                                       BPBF_COMPATIBLEBITMAP,
                                                       nullptr, &buffered_context);
        
        if (target_width != client_width ||
            target_height != client_height)
        {
            PatBlt(buffered_context, 0, 0, client_width, client_height, BLACKNESS);
        }
        
        u32 offset_x = (client_width - target_width) / 2;
        u32 offset_y = (client_height - target_height) / 2;
        
        // this function is actually really terrible at resizing images - especially at shrinking
        StretchDIBits(buffered_context,
                      offset_x, offset_y, target_width, target_height,
                      0, 0, buffer->width, buffer->height,
                      buffer->memory, &buffer->info, DIB_RGB_COLORS, SRCCOPY);
        
        DrawText(buffered_context, text, -1, &client_rect, DT_CENTER | DT_WORDBREAK);
        
        EndBufferedPaint(paint_buffer, true);
    }
}


static Gdi_Buffer create_gdi_buffer(u32 width, u32 height, u32 *memory)
{
    Gdi_Buffer buffer = {};
    buffer.memory = memory;
    buffer.width = width;
    buffer.height = height;
    
    buffer.info.bmiHeader.biSize = sizeof(buffer.info.bmiHeader);
    buffer.info.bmiHeader.biWidth = width;
    buffer.info.bmiHeader.biHeight = height;
    buffer.info.bmiHeader.biPlanes = 1;
    buffer.info.bmiHeader.biBitCount = 32;
    buffer.info.bmiHeader.biCompression = BI_RGB;
    return buffer;
}











////////////////////////////////
union Color_Hsl
{
    struct {f32 h, s, l;};
    v3 vec;
};

union Color_Hsv
{
    struct {f32 h, s, v;};
    v3 vec;
};


static Color_Hsv hsv_from_rgb(v3 color)
{
    Color_Hsv result = {};
    f32 cmax = pick_bigger(pick_bigger(color.r, color.g), color.b);
    f32 cmin = pick_smaller(pick_smaller(color.r, color.g), color.b);
    f32 cdelta = cmax - cmin;
    
    if (cdelta > 0)
    {
        if (cmax == color.r) {
            result.h = (f32)fmod(((color.g - color.b) / cdelta), 6.f); // todo: replace clib fmod & fabs?
        }
        else if (cmax == color.g) {
            result.h = ((color.b - color.r) / cdelta) + 2;
        }
        else { // blue
            result.h = ((color.r - color.g) / cdelta) + 4;
        }
        
        result.h /= 6.f;
        if (result.h < 0) { result.h += 1.f; }
        if (cmax > 0) { result.s = cdelta / cmax; }
    }
    
    result.v = cmax;
    return result;
}

static v3 rgb_from_hsv(Color_Hsv in)
{
    v3 res = {};
    
    in.h *= 6.f;
    s32 h_int = (s32)in.h;
    
    f32 h_reminder = in.h - (f32)h_int;
    
    f32 p = in.v * (1.f - in.s);
    f32 q = in.v * (1.f - in.s * h_reminder);
    f32 t = in.v * (1.f - in.s * (1.f - h_reminder));
    
    switch (h_int)
    {
        case 0: res = {in.v,  t,     p}; break;
        case 1: res = {q,     in.v,  p}; break;
        case 2: res = {p,     in.v,  t}; break;
        case 3: res = {p,     q,     in.v}; break;
        case 4: res = {t,     p,     in.v}; break;
        case 5: res = {in.v,  p,     q}; break;
    }
    
    return res;
}

static Color_Hsl hsl_from_rgb(v3 in)
{
    f32 max = pick_bigger(pick_bigger(in.r, in.g), in.b);
    f32 min = pick_smaller(pick_smaller(in.r, in.g), in.b);
    
    Color_Hsl res = {};
    res.l = (max + min) * 0.5f;
    
    if (max != min)
    {
        f32 delta = max - min;
        
        if (res.l > 0.5f) {
            res.s = delta / (2 - max - min);
        } else{
            res.s = delta / (max + min);
        }
        
        
        if (max == in.r)
        {
            res.h = (in.g - in.b) / delta;
            
            if (in.g < in.b) {
                res.h += 6.f;
            }
        }
        else if (max == in.g)
        {
            res.h = ((in.b - in.r) / delta) + 2.f;
        }
        else // blue
        {
            res.h = ((in.r - in.g) / delta) + 4.f;
        }
        
        res.h /= 6;
    }
    
    return res;
}


static v3 rgb_from_hsl(Color_Hsl color)
{
    f32 hue6 = color.h * 6.f;
    f32 c = (f32)(1 - fabs(2 * color.l - 1)) * color.s; // chroma 
    f32 x = c * (1 - (f32)fabs(fmod(hue6, 2.f) - 1.f)); // second largest color component
    f32 m = color.l - (c / 2);
    
    v3 res = {};
    switch ((int)hue6)
    {
        case 0: res = {c, x, 0}; break;
        case 1: res = {x, c, 0}; break;
        case 2: res = {0, c, x}; break;
        case 3: res = {0, x, c}; break;
        case 4: res = {x, 0, c}; break;
        case 5: res = {c, 0, x}; break;
    }
    
    res.r += m;
    res.g += m;
    res.b += m;
    return res;
}
