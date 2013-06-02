#include <assert.h>

#include "BlockBitmap.hpp"

BlockBitmap::BlockBitmap( const BitmapPtr& bmp, Channels type )
    : m_data( new uint8[bmp->Size().x * bmp->Size().y * ( type == Channels::RGB ? 3 : 1 )] )
    , m_size( bmp->Size() )
    , m_type( type )
{
    const uint32* src = bmp->Data();
    uint8* dst = m_data;

    assert( m_size.x % 4 == 0 && m_size.y % 4 == 0 );

    if( type == Channels::RGB )
    {
        for( int by=0; by<m_size.y/4; by++ )
        {
            for( int bx=0; bx<m_size.x/4; bx++ )
            {
                for( int x=0; x<4; x++ )
                {
                    for( int y=0; y<4; y++ )
                    {
                        const uint32 c = *src;
                        src += m_size.x;
                        *dst++ = ( c & 0x00FF0000 ) >> 16;
                        *dst++ = ( c & 0x0000FF00 ) >> 8;
                        *dst++ =   c & 0x000000FF;
                    }
                    src -= m_size.x * 4 - 1;
                }
            }
            src += m_size.x * 3;
        }
    }
    else
    {
        for( int by=0; by<m_size.y/4; by++ )
        {
            for( int bx=0; bx<m_size.x/4; bx++ )
            {
                for( int x=0; x<4; x++ )
                {
                    for( int y=0; y<4; y++ )
                    {
                        *dst++ = 255 - ( *src >> 24 );
                        src += m_size.x;
                    }
                    src -= m_size.x * 4 - 1;
                }
            }
            src += m_size.x * 3;
        }
    }
}

BlockBitmap::~BlockBitmap()
{
    delete[] m_data;
}
