#ifndef __DECODEDXTC_HPP__
#define __DECODEDXTC_HPP__

#include <stddef.h>
#include <stdint.h>

void DecodeDxt1( const uint64_t* src, uint32_t* dst, size_t width, size_t height );
void DecodeDxt5( const uint64_t* src, uint32_t* dst, size_t width, size_t height );
void DecodeBc4( const uint64_t* src, uint32_t* dst, size_t width, size_t height );
void DecodeBc5( const uint64_t* src, uint32_t* dst, size_t width, size_t height );

#endif
