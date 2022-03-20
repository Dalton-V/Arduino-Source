/*  Architecture
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_Kernels_Arch_H
#define PokemonAutomation_Kernels_Arch_H

//  Manual ISA Override
//#define PA_Arch_x64_AVX512GF
//#define PA_Arch_x64_AVX512
//#define PA_Arch_x64_AVX2
#define PA_Arch_x64_SSE42


//  Environment implied ISAs.
#if (defined _WIN64) || (defined __x86_64)
#define PA_Arch_x64
#endif


//  ISA Strings
#if 0
#elif defined PA_Arch_x64_AVX512GF
#define PA_ARCH_STRING  "x64-AVX512GF"
#elif defined PA_Arch_x64_AVX512
#define PA_ARCH_STRING  "x64-AVX512"
#elif defined PA_Arch_x64_AVX2
#define PA_ARCH_STRING  "x64-AVX2"
#elif defined PA_Arch_x64_SSE42
#define PA_ARCH_STRING  "x64-SSE4.2"
#elif defined PA_Arch_x64_SSE41
#define PA_ARCH_STRING  "x64-SSE4.1"
#elif defined PA_Arch_x64
#define PA_ARCH_STRING  "x64"
#endif




//  Implied ISAs.

#ifdef PA_Arch_x64_AVX512GF
#define PA_Arch_x64_AVX512
#endif

#ifdef PA_Arch_x64_AVX512
#define PA_Arch_x64_AVX2
#endif

#ifdef PA_Arch_x64_AVX2
#define PA_Arch_x64_SSE42
#endif

#ifdef PA_Arch_x64_SSE42
#define PA_Arch_x64_SSE41
#endif

#ifdef PA_Arch_x64_SSE41
#define PA_Arch_x64
#endif




namespace PokemonAutomation{
namespace Kernels{




}
}


#endif
