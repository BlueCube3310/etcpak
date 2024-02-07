#ifndef __DECODERGB_HPP__
#define __DECODERGB_HPP__

#include <stddef.h>
#include <stdint.h>

void DecodeR( const uint64_t* src, uint32_t* dst, size_t width, size_t height );
void DecodeRG( const uint64_t* src, uint32_t* dst, size_t width, size_t height );
void DecodeRGB( const uint64_t* src, uint32_t* dst, size_t width, size_t height );
void DecodeRGBA( const uint64_t* src, uint32_t* dst, size_t width, size_t height );

#endif
