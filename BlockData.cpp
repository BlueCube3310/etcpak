#include <assert.h>
#include <string.h>

#include "BlockData.hpp"
#include "ColorSpace.hpp"
#include "Debug.hpp"
#include "DecodeRGB.hpp"
#include "DecodeDxtc.hpp"
#include "MipMap.hpp"
#include "mmap.hpp"
#include "ProcessRGB.hpp"
#include "ProcessDxtc.hpp"
#include "TaskDispatch.hpp"

BlockData::BlockData( const char* fn )
    : m_file( fopen( fn, "rb" ) )
{
    assert( m_file );
    fseek( m_file, 0, SEEK_END );
    m_maplen = ftell( m_file );
    fseek( m_file, 0, SEEK_SET );
    m_data = (uint8_t*)mmap( nullptr, m_maplen, PROT_READ, MAP_SHARED, fileno( m_file ), 0 );

    auto data32 = (uint32_t*)m_data;
    if( *data32 == 0x03525650 )
    {
        // PVR
        switch( *(data32+2) )
        {
        case 6:
            m_type = Etc1;
            break;
        case 7:
            m_type = Dxt1;
            break;
        case 11:
            m_type = Dxt5;
            break;
        case 12:
            m_type = Bc4;
            break;
        case 13:
            m_type = Bc5;
            break;
        case 22:
            m_type = Etc2_RGB;
            break;
        case 23:
            m_type = Etc2_RGBA;
            break;
        case 25:
            m_type = Etc2_R11;
            break;
        case 26:
            m_type = Etc2_RG11;
            break;
        default:
            assert( false );
            break;
        }

        m_size.y = *(data32+6);
        m_size.x = *(data32+7);
        m_dataOffset = 52 + *(data32+12);
    }
    else if( *data32 == 0x58544BAB )
    {
        // KTX
        switch( *(data32+7) )
        {
        case 0x9274:
            m_type = Etc2_RGB;
            break;
        case 0x9278:
            m_type = Etc2_RGBA;
            break;
        case 0x9270:
            m_type = Etc2_R11;
            break;
        case 0x9272:
            m_type = Etc2_RG11;
            break;
        default:
            assert( false );
            break;
        }

        m_size.x = *(data32+9);
        m_size.y = *(data32+10);
        m_dataOffset = sizeof( uint32_t ) * 17 + *(data32+15);
    }
    else
    {
        assert( false );
    }
}

static uint8_t* OpenForWriting( const char* fn, size_t len, const v2i& size, FILE** f, int levels, BlockData::Type type )
{
    *f = fopen( fn, "wb+" );
    assert( *f );
    fseek( *f, len - 1, SEEK_SET );
    const char zero = 0;
    fwrite( &zero, 1, 1, *f );
    fseek( *f, 0, SEEK_SET );

    auto ret = (uint8_t*)mmap( nullptr, len, PROT_WRITE, MAP_SHARED, fileno( *f ), 0 );
    auto dst = (uint32_t*)ret;

    *dst++ = 0x03525650;  // version
    *dst++ = 0;           // flags
    switch( type )        // pixelformat[0]
    {
    case BlockData::Etc1:
        *dst++ = 6;
        break;
    case BlockData::Etc2_RGB:
        *dst++ = 22;
        break;
    case BlockData::Etc2_RGBA:
        *dst++ = 23;
        break;
    case BlockData::Etc2_R11:
        *dst++ = 25;
        break;
    case BlockData::Etc2_RG11:
        *dst++ = 26;
        break;
    case BlockData::Dxt1:
        *dst++ = 7;
        break;
    case BlockData::Dxt5:
        *dst++ = 11;
        break;
    case BlockData::Bc4:
        *dst++ = 12;
        break;
    case BlockData::Bc5:
        *dst++ = 13;
        break;
    default:
        assert( false );
        break;
    }
    *dst++ = 0;           // pixelformat[1]
    *dst++ = 0;           // colourspace
    *dst++ = 0;           // channel type
    *dst++ = size.y;      // height
    *dst++ = size.x;      // width
    *dst++ = 1;           // depth
    *dst++ = 1;           // num surfs
    *dst++ = 1;           // num faces
    *dst++ = levels;      // mipmap count
    *dst++ = 0;           // metadata size

    return ret;
}

static int AdjustSizeForMipmaps( const v2i& size, int levels )
{
    int len = 0;
    v2i current = size;
    for( int i=1; i<levels; i++ )
    {
        assert( current.x != 1 || current.y != 1 );
        current.x = std::max( 1, current.x / 2 );
        current.y = std::max( 1, current.y / 2 );
        len += std::max( 4, current.x ) * std::max( 4, current.y ) / 2;
    }
    assert( current.x == 1 && current.y == 1 );
    return len;
}

BlockData::BlockData( const char* fn, const v2i& size, bool mipmap, Type type )
    : m_size( size )
    , m_dataOffset( 52 )
    , m_maplen( m_size.x*m_size.y/2 )
    , m_type( type )
{
    assert( m_size.x%4 == 0 && m_size.y%4 == 0 );

    uint32_t cnt = m_size.x * m_size.y / 16;
    DBGPRINT( cnt << " blocks" );

    int levels = 1;

    if( mipmap )
    {
        levels = NumberOfMipLevels( size );
        DBGPRINT( "Number of mipmaps: " << levels );
        m_maplen += AdjustSizeForMipmaps( size, levels );
    }

    if( type == Etc2_RGBA || type == Dxt5 || type == Bc5 || type == Etc2_RG11 ) m_maplen *= 2;

    m_maplen += m_dataOffset;
    m_data = OpenForWriting( fn, m_maplen, m_size, &m_file, levels, type );
}

BlockData::BlockData( const v2i& size, bool mipmap, Type type )
    : m_size( size )
    , m_dataOffset( 52 )
    , m_file( nullptr )
    , m_maplen( m_size.x*m_size.y/2 )
    , m_type( type )
{
    assert( m_size.x%4 == 0 && m_size.y%4 == 0 );
    if( mipmap )
    {
        const int levels = NumberOfMipLevels( size );
        m_maplen += AdjustSizeForMipmaps( size, levels );
    }

    if( type == Etc2_RGBA || type == Dxt5 || type == Bc5 || type == Etc2_RG11 ) m_maplen *= 2;

    m_maplen += m_dataOffset;
    m_data = new uint8_t[m_maplen];
}

BlockData::~BlockData()
{
    if( m_file )
    {
        munmap( m_data, m_maplen );
        fclose( m_file );
    }
    else
    {
        delete[] m_data;
    }
}

void BlockData::Process( const uint32_t* src, uint32_t blocks, size_t offset, size_t width, Channels type, bool dither, bool useHeuristics )
{
    auto dst = ((uint64_t*)( m_data + m_dataOffset )) + offset;

    if( type == Channels::Alpha )
    {
        if( m_type != Etc1 )
        {
            CompressEtc2Alpha( src, dst, blocks, width, useHeuristics );
        }
        else
        {
            CompressEtc1Alpha( src, dst, blocks, width );
        }
    }
    else
    {
        switch( m_type )
        {
        case Etc1:
            if( dither )
            {
                CompressEtc1RgbDither( src, dst, blocks, width );
            }
            else
            {
                CompressEtc1Rgb( src, dst, blocks, width );
            }
            break;
        case Etc2_RGB:
            CompressEtc2Rgb( src, dst, blocks, width, useHeuristics );
            break;
        case Etc2_R11:
            CompressEacR( src, dst, blocks, width );
            break;
        case Etc2_RG11:
            dst = ((uint64_t*)( m_data + m_dataOffset )) + offset * 2;
            CompressEacRg( src, dst, blocks, width );
            break;
        case Dxt1:
            if( dither )
            {
                CompressDxt1Dither( src, dst, blocks, width );
            }
            else
            {
                CompressDxt1( src, dst, blocks, width );
            }
            break;
        case Bc4:
            CompressBc4( src, dst, blocks, width );
            break;
        case Bc5:
            dst = ((uint64_t*)( m_data + m_dataOffset )) + offset * 2;
            CompressBc5( src, dst, blocks, width );
            break;
        default:
            assert( false );
            break;
        }
    }
}

void BlockData::ProcessRGBA( const uint32_t* src, uint32_t blocks, size_t offset, size_t width, bool useHeuristics )
{
    auto dst = ((uint64_t*)( m_data + m_dataOffset )) + offset * 2;

    switch( m_type )
    {
    case Etc2_RGBA:
        CompressEtc2Rgba( src, dst, blocks, width, useHeuristics );
        break;
    case Dxt5:
        CompressDxt5( src, dst, blocks, width );
        break;
    default:
        assert( false );
        break;
    }
}

BitmapPtr BlockData::Decode()
{
    auto ret = std::make_shared<Bitmap>( m_size );
    const uint64_t* src = (const uint64_t*)( m_data + m_dataOffset );

    switch( m_type )
    {
    case Etc1:
    case Etc2_RGB:
        DecodeRGB( src, ret->Data(), m_size.x, m_size.y );
        break;
    case Etc2_RGBA:
        DecodeRGBA( src, ret->Data(), m_size.x, m_size.y );
        break;
    case Etc2_R11:
        DecodeR( src, ret->Data(), m_size.x, m_size.y );
        break;
    case Etc2_RG11:
        DecodeRG( src, ret->Data(), m_size.x, m_size.y );
        break;
    case Dxt1:
        DecodeDxt1( src, ret->Data(), m_size.x, m_size.y );
        break;
    case Dxt5:
        DecodeDxt5( src, ret->Data(), m_size.x, m_size.y );
        break;
    case Bc4:
        DecodeBc4( src, ret->Data(), m_size.x, m_size.y );
        break;
    case Bc5:
        DecodeBc5( src, ret->Data(), m_size.x, m_size.y );
        break;
    default:
        assert( false );
        return nullptr;
    }
	
	return ret;
}
