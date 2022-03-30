/*
  Task 1:
  I started with some exploration of different techniques for image saturation.
  I implemented converting to HSV (1) & HSL (2) color spaces and back - and changing the saturation there.
  Then I found an approach on the internet where you calculate relative luminance of a color (3).
  Then you simply multiply the difference between a gray scale version and color version by the saturation value.
  My understanding is that this should be done in linear color space, (4) so I added this version as well.

  The linear luminance version and HSL version give similar results.
  I implemented all-in-one convert_image function using technique (3) for saturation.
  It is the least correct one but it was the fastest, looks mostly OK and was the easiest to implement for my AVX version :D

  convert_image is implemented both as normal C++ code and has an analogous version in manually written AVX intrinsics.
  I picked AVX because it is the newest SIMD extension that my CPU supports (Ivy Bridge).
  AVX is a little weird because it doesn't support 256 bit wide integer operations so in reality it is a mix of AVX and SSE.
  
  
  Controls:
  C - convert_image()
  B - benchmark - calls convert_image() 245 times
  R - reload image.png from disk
  [ - decrease saturation variable by 0.1
  ] - increase saturation variable by 0.1
  BACKSPACE - reset saturation variable to 1.0
  1 - swap Red and Blue bytes
  2 - flip image vertically
  3 - saturate image in HSV space
  4 - saturate image in HSL space
  5 - saturate image using relative luminance
  6 - saturate image using relative luminance in linear color space
*/

// Benchmark in release mode (245 iterations); lowest time:
// clang without manual AVX: 1.562ms
//  msvc without manual AVX: 3.970ms
// clang with AVX: 0.724ms
//  msvc with AVX: 0.757ms




#include "shared.h"

#define Use_Avx 1

#if Use_Avx
#  if _MSC_VER
#  include <immintrin.h>
#  else
#  include <x86intrin.h>
#  endif
#endif




__forceinline static u32 convert_pixel(u32 value, f32 saturation)
{
    v3 source = {
        (f32)((value >> 16) & 0xFF),
        (f32)((value >> 8)  & 0xFF),
        (f32)(value         & 0xFF),
    };
    
    f32 luminance = (source.r * 0.2126f +
                     source.g * 0.7152f +
                     source.b * 0.0722f);
    v3 gray = {luminance, luminance, luminance};
    v3 diff = source - gray;
    diff *= saturation;
    
    v3 out = (gray + diff);
    out.x = clamp(0.f, out.x, 255.f);
    out.y = clamp(0.f, out.y, 255.f);
    out.z = clamp(0.f, out.z, 255.f);
    
    u32 r255 = (u32)out.r;
    u32 g255 = (u32)out.g;
    u32 b255 = (u32)out.b;
    
    u32 result = ((value & 0xFF00'0000) |
                  (b255 << 16) |
                  (g255 << 8) |
                  r255);
    return result;
}


static void convert_image(u32 width, u32 height, u32 *memory, float saturation)
{
    u32 half_height = height / 2;
    
#if Use_Avx
    __m128i imask_FF = _mm_set1_epi32(0xFF);
    __m128i imask_full = _mm_set1_epi32(~0);
    __m128i imask_0 = _mm_set1_epi32(0);
    
    __m256 coef_r = _mm256_set1_ps(0.2126f);
    __m256 coef_g = _mm256_set1_ps(0.7152f);
    __m256 coef_b = _mm256_set1_ps(0.0722f);
    __m256 saturation_wide = _mm256_set1_ps(saturation);
    __m256 value0 = _mm256_set1_ps(0.f);
    __m256 value255 = _mm256_set1_ps(255.f);
    
    
    __m128i row_ending_clip_masks[] = {
        _mm_srli_si128(imask_full, 0*4),
        _mm_srli_si128(imask_full, 3*4),
        _mm_srli_si128(imask_full, 2*4),
        _mm_srli_si128(imask_full, 1*4),
    };
    
    u32 width_ending = width & 3;
    __m128i row_end_clip_mask = row_ending_clip_masks[width_ending];
    __m128i inv_row_end_clip_mask = _mm_xor_si128(row_end_clip_mask, imask_full);
    
    
    for (u64 y = 0; y < half_height; y += 1)
    {
        u32 *row = memory + y*width;
        u32 *opposite_row = memory + (height - y - 1)*width;
        __m128i end_clip_mask = imask_full;
        __m128i inv_end_clip_mask = imask_0;
        
        for (u64 x = 0; x < width; x += 4)
        {
            if (x+4 > width) {
                end_clip_mask = row_end_clip_mask;
                inv_end_clip_mask = inv_row_end_clip_mask;
            }
            
            
            // Loading data
            __m128i *top_address = (__m128i*)&row[x];
            __m128i *bot_address = (__m128i*)&opposite_row[x];
            __m128i top_input = _mm_loadu_si128(top_address);
            __m128i bot_input = _mm_loadu_si128(bot_address);
            
            
            // Unpacking from u32 colors to 3 floats per color; Also Red and Blue is swapped.
            __m128i top_r = _mm_and_si128(_mm_srli_epi32(top_input, 16), imask_FF);
            __m128i bot_r = _mm_and_si128(_mm_srli_epi32(bot_input, 16), imask_FF);
            __m256 r = _mm256_cvtepi32_ps(_mm256_loadu2_m128i(&top_r, &bot_r));
            
            __m128i top_g = _mm_and_si128(_mm_srli_epi32(top_input, 8), imask_FF);
            __m128i bot_g = _mm_and_si128(_mm_srli_epi32(bot_input, 8), imask_FF);
            __m256 g = _mm256_cvtepi32_ps(_mm256_loadu2_m128i(&top_g, &bot_g));
            
            __m128i top_b = _mm_and_si128(top_input, imask_FF);
            __m128i bot_b = _mm_and_si128(bot_input, imask_FF);
            __m256 b = _mm256_cvtepi32_ps(_mm256_loadu2_m128i(&top_b, &bot_b));
            
            // Data is now transfered to three 256bit variables - each variable holds 8 floats.
            // {top_r0, top_r1, top_r2, top_r3, bot_r0, bot_r1, bot_r2, bot_r3}
            // {top_g0, top_g1, top_g2, top_g3, bot_g0, bot_g1, bot_g2, bot_g3}
            // {top_b0, top_b1, top_b2, top_b3, bot_b0, bot_b1, bot_b2, bot_b3}
            
            
            // Calculate saturation
            __m256 luminance = _mm256_add_ps(_mm256_mul_ps(r, coef_r), _mm256_mul_ps(g, coef_g));
            luminance = _mm256_add_ps(luminance, _mm256_mul_ps(b, coef_b));
            
            __m256 diff_r = _mm256_mul_ps(_mm256_sub_ps(r, luminance), saturation_wide);
            __m256 diff_g = _mm256_mul_ps(_mm256_sub_ps(g, luminance), saturation_wide);
            __m256 diff_b = _mm256_mul_ps(_mm256_sub_ps(b, luminance), saturation_wide);
            
            r = _mm256_add_ps(luminance, diff_r);
            g = _mm256_add_ps(luminance, diff_g);
            b = _mm256_add_ps(luminance, diff_b);
            
            r = _mm256_max_ps(_mm256_min_ps(r, value255), value0);
            g = _mm256_max_ps(_mm256_min_ps(g, value255), value0);
            b = _mm256_max_ps(_mm256_min_ps(b, value255), value0);
            
            
            // Going back to u32 color
            __m256i ri = _mm256_cvttps_epi32(r);
            __m256i gi = _mm256_cvttps_epi32(g);
            __m256i bi = _mm256_cvttps_epi32(b);
            
            __m128i bot_ri = _mm256_extractf128_si256(ri, 0);
            __m128i top_ri = _mm256_extractf128_si256(ri, 1);
            __m128i bot_gi = _mm256_extractf128_si256(gi, 0);
            __m128i top_gi = _mm256_extractf128_si256(gi, 1);
            __m128i bot_bi = _mm256_extractf128_si256(bi, 0);
            __m128i top_bi = _mm256_extractf128_si256(bi, 1);
            
            bot_gi = _mm_slli_epi32(bot_gi, 8);
            top_gi = _mm_slli_epi32(top_gi, 8);
            bot_bi = _mm_slli_epi32(bot_bi, 16);
            top_bi = _mm_slli_epi32(top_bi, 16);
            
            __m128i top_output = _mm_or_si128(_mm_or_si128(top_ri, top_gi), top_bi);
            __m128i bot_output = _mm_or_si128(_mm_or_si128(bot_ri, bot_gi), bot_bi);
            
            
            // Masking off the changes at the end of the row for widths that are non-divisible by 4
            top_output = _mm_and_si128(top_output, end_clip_mask);
            bot_output = _mm_and_si128(bot_output, end_clip_mask);
            top_input = _mm_and_si128(top_input, inv_end_clip_mask);
            bot_input = _mm_and_si128(bot_input, inv_end_clip_mask);
            top_output = _mm_or_si128(top_output, bot_input);
            bot_output = _mm_or_si128(bot_output, top_input);
            
            
            // Store the data back + swap top and bottom rows
            _mm_storeu_si128(bot_address, top_output);
            _mm_storeu_si128(top_address, bot_output);
        }
    }
#else
    for (u64 y = 0; y < half_height; y += 1)
    {
        u32 *row = memory + y*width;
        u32 *opposite_row = memory + (height - y - 1)*width;
        
        for (u64 x = 0; x < width; x += 1)
        {
            u32 row_value = convert_pixel(row[x], saturation);
            u32 opposite_value = convert_pixel(opposite_row[x], saturation);
            
            opposite_row[x] = row_value;
            row[x] = opposite_value;
        }
    }
#endif
    
    
    // Run normal C++ version for one row in the middle - for odd heights
    if (height & 1)
    {
        u32 *row = memory + half_height*width;
        
        for (u64 x = 0; x < width; x += 1)
        {
            row[x] = convert_pixel(row[x], saturation);
        }
    }
}





struct App_State
{
    HWND window;
    Gdi_Buffer buffer;
    float saturation;
    
    char benchmark_text[512];
};
static App_State app_state;




#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ASSERT(x) assert(x)

// Add padding at the end of the buffer for images with width non-divisible by 4
// so we don't touch memory beyond the end of the buffer with SSE instructions
#define align_bin_to(Value, AlignToPow2Number) ((Value + (AlignToPow2Number-1)) & ~(AlignToPow2Number-1))
#define STBI_MALLOC(sz)           malloc(align_bin_to(sz, 16))
#define STBI_REALLOC(p,newsz)     realloc(p, align_bin_to(newsz, 16))
#define STBI_FREE(p)              free(p)
#include "stb_image.h"






static void image_swap_bytes_between_rgba_and_bgra(u32 width, u32 height, u32 *memory)
{
    for (u64 y = 0; y < height; y += 1)
    {
        u32 *row = memory + y*width;
            
        for (u64 x = 0; x < width; x += 1)
        {
            u32 value = row[x];
            row[x] = (((value & 0xFF'00'FF'00)      ) |
                      ((value & 0x00'00'00'FF) << 16) |
                      ((value & 0x00'FF'00'00) >> 16));
        }
    }
}

static void image_flip_vertically(u32 width, u32 height, u32 *memory)
{
    u32 half_height = height / 2;
    
    for (u64 y = 0; y < half_height; y += 1)
    {
        u32 *row = memory + y*width;
        u32 *opposite_row = memory + (height - y - 1)*width;
        
        for (u64 x = 0; x < width; x += 1)
        {
            u32 temp = row[x];
            row[x] = opposite_row[x];
            opposite_row[x] = temp;
        }
    }
}





static f32 color_linear_to_srgb(f32 l)
{
    f32 s;
    if (l > 0.0031308f)
    {
        s = 1.055f*powf(l, 1.f/2.4f) - 0.055f;
    }
    else
    {
        s = l*12.92f;
    }
    return s;
}

static f32 color_srgb_to_linear(f32 s)
{
    f32 l;
    if (s > 0.04045f)
    {
        l = powf(((s+0.055f) / 1.055f), 2.4f);
    }
    else
    {
        l = s / 12.92f;
    }
    return l;
}


enum Saturation_Type
{
    Saturation_Hsv,
    Saturation_Hsl,
    Saturation_Luminance_Srgb,
    Saturation_Luminance_Linear,
};

static void image_saturate(u32 width, u32 height, u32 *memory,
                           f32 saturation, Saturation_Type saturation_type)
{
    for (u64 y = 0; y < height; y += 1)
    {
        u32 *row = memory + y*width;
        
        for (u64 x = 0; x < width; x += 1)
        {
            u32 value = row[x];
            
            // this is assuming that our image is in RGBA (R in bottom bits) format now
            v3 source = {
                (f32)(value         & 0xFF) / 255.f,
                (f32)((value >> 8)  & 0xFF) / 255.f,
                (f32)((value >> 16) & 0xFF) / 255.f,
            };
            
            v3 out = {};
            
            switch (saturation_type)
            {
                case Saturation_Hsv:
                {
                    Color_Hsv hsv = hsv_from_rgb(source);
                    hsv.s *= saturation;
                    out = rgb_from_hsv(hsv);
                } break;
                
                case Saturation_Hsl:
                {
                    Color_Hsl hsl = hsl_from_rgb(source);
                    hsl.s *= saturation;
                    out = rgb_from_hsl(hsl);
                } break;
                
                case Saturation_Luminance_Srgb:
                {
                    f32 luminance = (source.r * 0.2126f +
                                     source.g * 0.7152f +
                                     source.b * 0.0722f);
                    v3 gray = {luminance, luminance, luminance};
                    v3 diff = source - gray;
                    diff *= saturation;
                    
                    out = (gray + diff);
                } break;
                
                case Saturation_Luminance_Linear:
                {
                    source.r = color_srgb_to_linear(source.r);
                    source.g = color_srgb_to_linear(source.g);
                    source.b = color_srgb_to_linear(source.b);
                    
                    f32 luminance = (source.r * 0.2126f +
                                     source.g * 0.7152f +
                                     source.b * 0.0722f);
                    v3 gray = {luminance, luminance, luminance};
                    v3 diff = source - gray;
                    diff *= saturation;
                    
                    out = (gray + diff);
                    out.r = color_linear_to_srgb(out.r);
                    out.g = color_linear_to_srgb(out.g);
                    out.b = color_linear_to_srgb(out.b);
                } break;
            };
            
            
            out = clamp01(out);
            row[x] = ((value & 0xFF00'0000) |
                      (u32)(out.z * 255.f) << 16 |
                      (u32)(out.y * 255.f) << 8 |
                      (u32)(out.x * 255.f));
        }
    }
}






static s32 win32_throw_message(char *message)
{
    MSGBOXPARAMSA params = {};
    params.cbSize = sizeof(params);
    params.lpszText = message;
    params.dwStyle = MB_ICONERROR | MB_TASKMODAL;
    params.dwStyle |= MB_OK;
    
    s32 message_res = MessageBoxIndirectA(&params);
    return message_res;
}





static void reload_default_image()
{
    if (app_state.buffer.memory)
    {
        stbi_image_free(app_state.buffer.memory);
        app_state.buffer.memory = nullptr;
    }
    
    
    char *image_path = "image.png";
    int image_width = 0;
    int image_height = 0;
    int components = 0;
    
    u32 *image_data = (u32*)stbi_load(image_path, &image_width, &image_height, &components, 4);
    if (!image_data)
    {
        win32_throw_message("Please put image.png into current working directory");
        ExitProcess(0);
    }
    
    app_state.buffer = create_gdi_buffer(image_width, image_height, image_data);
    
    // stb_image.h format -> gdi format
    image_swap_bytes_between_rgba_and_bgra(image_width, image_height, image_data);
    image_flip_vertically(image_width, image_height, image_data);
}




static LRESULT win32_window_procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (message)
    {
        case WM_QUIT:
        case WM_CLOSE:
        {
            ExitProcess(0);
        } break;
        
        case WM_KEYDOWN:
        {
            Gdi_Buffer *buffer = &app_state.buffer;
            
            u64 vk_code = wParam;
            switch (vk_code)
            {
                case 'C':
                {
                    convert_image(buffer->width, buffer->height, buffer->memory,
                                  app_state.saturation);
                } break;
                
                case 'B':
                {
                    // simple benchmark
                    f32 lowest_time = 10000.f;
                    f32 total_time = 0;
                    int loop_count = 245;
                    
                    s64 last = time_perf();
                    for (int i = 0; i < loop_count; i += 1)
                    {
                        convert_image(buffer->width, buffer->height, buffer->memory,
                                      app_state.saturation);
                        
                        s64 now = time_perf();
                        f32 elapsed = time_elapsed(now, last);
                        last = now;
                        
                        if (elapsed < lowest_time) {
                            lowest_time = elapsed;
                        }
                        
                        total_time += elapsed;
                    }
                    
                    
                    f32 average_time = total_time / (f32)loop_count;
                    
                    snprintf(app_state.benchmark_text, sizeof(app_state.benchmark_text),
                             "Lowest: %.3fms\nAverage: %.3fms\nTotal: %.3fms\n",
                             lowest_time*1000.f, average_time*1000.f, total_time*1000.f);
                    OutputDebugStringA(app_state.benchmark_text);
                } break;
                
                case 'R':
                {
                    reload_default_image();
                    app_state.benchmark_text[0] = 0;
                } break;
                
                case VK_OEM_4: // [
                {
                    app_state.saturation -= 0.1f;
                    if (app_state.saturation < 0.f)
                    {
                        app_state.saturation = 0.f;
                    }
                } break;
                
                case VK_OEM_6: // ]
                {
                    app_state.saturation += 0.1f;
                } break;
                
                case VK_BACK: // backspace
                {
                    app_state.saturation = 1.f;
                } break;
                
                case '1': {
                    image_swap_bytes_between_rgba_and_bgra(buffer->width, buffer->height, buffer->memory);
                } break;
                
                case '2': {
                    image_flip_vertically(buffer->width, buffer->height, buffer->memory);
                } break;
                
                case '3': {
                    image_saturate(buffer->width, buffer->height, buffer->memory,
                                   app_state.saturation, Saturation_Hsv);
                } break;
                
                case '4': {
                    image_saturate(buffer->width, buffer->height, buffer->memory,
                                   app_state.saturation, Saturation_Hsl);
                } break;
                
                case '5': {
                    image_saturate(buffer->width, buffer->height, buffer->memory,
                                   app_state.saturation, Saturation_Luminance_Srgb);
                } break;
                
                case '6': {
                    image_saturate(buffer->width, buffer->height, buffer->memory,
                                   app_state.saturation, Saturation_Luminance_Linear);
                } break;
            }
        } break;
        
        default:
        {
            result = DefWindowProcW(window, message, wParam, lParam);
        } break;
    }
    return result;
}





static b32 debug_equals(f32 a, f32 b)
{
    f32 epsilon = 0.01f; // for float numerical precision + my test input data wasn't too precise either
    b32 result = (a - epsilon <= b && a + epsilon >= b);
    return result;
}

static b32 debug_equals(v3 a, v3 b)
{
    b32 result = (debug_equals(a.x, b.x) &&
                  debug_equals(a.y, b.y) &&
                  debug_equals(a.z, b.z));
    return result;
}

static void debug_conversion_tests()
{
    // I think that tests are useful for this kind of code in general
    // but besides that - I made some mistakes and needed to find & debug them
    {
        Color_Hsl hsl_values[] =
        {
            {0,0,0}, {0.5f,0,0}, {0,0.5f,0}, {0,0,0.5f},
            {0,0.5f,0.5f}, {0.5f,0.5f,0.9f},
            {0.475f, 0.66f, 0.77f},
        };
        v3 rgb_values[] =
        {
            {0,0,0}, {0,0,0}, {0,0,0}, {0.5f,0.5f,0.5f},
            {0.75f,0.25f,0.25f}, {0.8509f,0.94901f,0.94901f},
            {0.619607f, 0.921568f, 0.874509f},
        };
        static_assert(array_count(hsl_values) == array_count(rgb_values), "Expected the same array counts");
        
        
        for (u64 i = 0; i < array_count(hsl_values); i += 1)
        {
            v3 result = rgb_from_hsl(hsl_values[i]);
            assert(debug_equals(result, rgb_values[i]));
        }
        
        for (u64 i = 0; i < array_count(hsl_values); i += 1)
        {
            Color_Hsl expected = hsl_values[i];
            
            Color_Hsl result = hsl_from_rgb(rgb_values[i]);
            assert(debug_equals(result.vec, expected.vec) ||
                   (expected.l == 0.f && result.l == 0.f));
        }
    }
    
    
    {
        Color_Hsv hsv_values[] =
        {
            {0,0,0}, {0.5f,0,0}, {0,0.5f,0}, {0,0,0.5f},
            {0,0.5f,0.5f}, {0.5f,0.5f,0.9f},
            {0.475f, 0.66f, 0.77f},
        };
        v3 rgb_values[] =
        {
            {0,0,0}, {0,0,0}, {0,0,0}, {0.5f,0.5f,0.5f},
            {0.5f,0.25f,0.25f}, {0.45098f,0.90196f,0.90196f},
            {0.262745f, 0.768627f, 0.694118f},
        };
        static_assert(array_count(hsv_values) == array_count(rgb_values), "Expected the same array counts");
        
        
        for (u64 i = 0; i < array_count(hsv_values); i += 1)
        {
            v3 result = rgb_from_hsv(hsv_values[i]);
            assert(debug_equals(result, rgb_values[i]));
        }
        
        for (u64 i = 0; i < array_count(hsv_values); i += 1)
        {
            Color_Hsv expected = hsv_values[i];
            
            Color_Hsv result = hsv_from_rgb(rgb_values[i]);
            assert(debug_equals(result.vec, expected.vec) ||
                   (expected.v == 0.f && result.v == 0.f));
        }
    }
}





int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    debug_conversion_tests();
    
    reload_default_image();
    app_state.saturation = 1.f;
    
    
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &win32_window_procedure;
    window_class.hInstance = hInstance;
    window_class.hCursor = LoadCursorA(0, IDC_ARROW);
    window_class.lpszClassName = L"ImageConverterClass";
    RegisterClassExW(&window_class);
    
    app_state.window = CreateWindowExW(0, window_class.lpszClassName, L"Image converter",
                                    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                    1000, 900, 0, 0, hInstance, nullptr);
    
    ShowWindow(app_state.window, SW_SHOW);
    
    
    
    for(;;)
    {
        char text_buffer[256];
        snprintf(text_buffer, sizeof(text_buffer), "saturation: %.2f\n%s",
                 app_state.saturation, app_state.benchmark_text);
        
        HDC device_context = GetDC(app_state.window);
        display_gdi_buffer(app_state.window, device_context, &app_state.buffer, text_buffer);
        ReleaseDC(app_state.window, device_context);
        
        {
            MSG msg;
            GetMessageW(&msg, 0, 0, 0);
            do
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            } while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE));
        }
    }
}
