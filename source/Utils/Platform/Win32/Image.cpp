#include "Utils/Config/PlatformConfig.hpp"

#if defined( Utils_PlatformWin32 )

#include "Utils/Graphics/Image.hpp"
#include "Utils/Data/Path.hpp"
#include "Utils/Graphics/Rectangle.hpp"
#include "Utils/Log/Logger.hpp"

extern "C"
{
#	include <FreeImage.h>
}

namespace utils
{
	//************************************************************************************************

	namespace
	{
		uint32_t next2Pow( uint32_t p_uiDim )
		{
			static uint32_t TwoPows[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576 };// should be enough for image dimensions ...
			static uint32_t Size = sizeof( TwoPows ) / sizeof( uint32_t );
			uint32_t uiReturn = 0;

			for ( uint32_t i = 0; i < Size && uiReturn < p_uiDim; i++ )
			{
				uiReturn = TwoPows[i];
			}

			return uiReturn;
		}

		uint32_t DLL_CALLCONV readProc( void * p_buffer, uint32_t p_uiSize, uint32_t p_count, fi_handle p_fiHandle )
		{
			BinaryFile * pFile = reinterpret_cast< BinaryFile * >( p_fiHandle );
			return uint32_t( pFile->readArray( reinterpret_cast< uint8_t * >( p_buffer ), p_uiSize * p_count ) );
		}

		int DLL_CALLCONV seekProc( fi_handle p_fiHandle, long p_lOffset, int p_iOrigin )
		{
			BinaryFile * pFile = reinterpret_cast< BinaryFile * >( p_fiHandle );
			return pFile->seek( p_lOffset, File::OffsetMode( p_iOrigin ) );
		}

		long DLL_CALLCONV tellProc( fi_handle p_fiHandle )
		{
			BinaryFile * pFile = reinterpret_cast< BinaryFile * >( p_fiHandle );
			return long( pFile->tell() );
		}

		void freeImageErrorHandler( FREE_IMAGE_FORMAT fif, const char * message )
		{
			if ( fif != FIF_UNKNOWN )
			{
				Logger::logWarning( std::stringstream() << "FreeImage - " << FreeImage_GetFormatFromFIF( fif ) << " Format - " << message );
			}
			else
			{
				Logger::logWarning( std::stringstream() << "FreeImage - Unknown Format - " << message );
			}
		}

		void swapComponents( uint8_t * p_pixels, ashes::Format p_format, uint32_t p_width, uint32_t p_height )
		{
			uint32_t count{ p_width * p_height };
			uint32_t bpp{ PF::getBytesPerPixel( p_format ) };
			uint32_t bpc{ 0u };

			if ( PF::hasAlpha( p_format ) )
			{
				bpc = bpp / 4;
			}
			else
			{
				bpc = bpp / 3;
			}

			uint8_t * r{ p_pixels + 0 * bpc };
			uint8_t * b{ p_pixels + 2 * bpc };

			for ( uint32_t i = 0; i < count; i++ )
			{
				std::swap( *r, *b );
				r += bpp;
				b += bpp;
			}
		}

		bool preMultiplyWithAlpha( FIBITMAP * dib )
		{
			if ( !FreeImage_HasPixels( dib ) )
			{
				return false;
			}

			if ( ( FreeImage_GetBPP( dib ) != 32 ) || ( FreeImage_GetImageType( dib ) != FIT_BITMAP ) )
			{
				return false;
			}

			int width = FreeImage_GetWidth( dib );
			int height = FreeImage_GetHeight( dib );

			for ( int y = 0; y < height; y++ )
			{
				auto * bits = FreeImage_GetScanLine( dib, y );

				for ( int x = 0; x < width; x++, bits += 4 )
				{
					const BYTE alpha = bits[FI_RGBA_ALPHA];

					// slightly faster: care for two special cases
					if ( alpha == 0x00 )
					{
						// special case for alpha == 0x00
						// color * 0x00 / 0xFF = 0x00
						bits[FI_RGBA_BLUE] = 0x00;
						bits[FI_RGBA_GREEN] = 0x00;
						bits[FI_RGBA_RED] = 0x00;
					}
					else if ( alpha == 0xFF )
					{
						// nothing to do for alpha == 0xFF
						// color * 0xFF / 0xFF = color
						continue;
					}
					else
					{
						bits[FI_RGBA_BLUE] = ( BYTE )( ( alpha * ( WORD )bits[FI_RGBA_BLUE] + 127 ) / 255 );
						bits[FI_RGBA_GREEN] = ( BYTE )( ( alpha * ( WORD )bits[FI_RGBA_GREEN] + 127 ) / 255 );
						bits[FI_RGBA_RED] = ( BYTE )( ( alpha * ( WORD )bits[FI_RGBA_RED] + 127 ) / 255 );
					}
				}
			}

			return true;
		}
	}

	//************************************************************************************************

	Image::BinaryLoader::BinaryLoader()
	{
	}

	bool Image::BinaryLoader::operator()( Image & p_image, Path const & p_path )
	{
		if ( p_path.empty() )
		{
			LOADER_ERROR( "Can't load image : path is empty" );
		}

		p_image.m_buffer.reset();
		ashes::Format ePF = ashes::Format::eR8G8B8_UNORM;
		int flags = BMP_DEFAULT;
		FREE_IMAGE_FORMAT fiFormat = FreeImage_GetFileType( string::stringCast< char >( p_path ).c_str(), 0 );

		if ( fiFormat == FIF_UNKNOWN )
		{
			fiFormat = FreeImage_GetFIFFromFilename( string::stringCast< char >( p_path ).c_str() );
		}
		else if ( fiFormat == FIF_TIFF )
		{
			flags = TIFF_DEFAULT;
		}

		if ( fiFormat == FIF_UNKNOWN || !FreeImage_FIFSupportsReading( fiFormat ) )
		{
			LOADER_ERROR( "Can't load image : unsupported image format" );
		}

		auto fiImage = FreeImage_Load( fiFormat, string::stringCast< char >( p_path ).c_str() );

		if ( !fiImage )
		{
			BinaryFile file( p_path, uint32_t( File::OpenMode::eRead ) | uint32_t( File::OpenMode::eBinary ) );
			FreeImageIO fiIo;
			fiIo.read_proc = readProc;
			fiIo.write_proc = nullptr;
			fiIo.seek_proc = seekProc;
			fiIo.tell_proc = tellProc;
			fiImage = FreeImage_LoadFromHandle( fiFormat, & fiIo, fi_handle( & file ), flags );

			if ( !fiImage )
			{
				LOADER_ERROR( "Can't load image : " + string::stringCast< char >( p_path ) );
			}
		}

		FREE_IMAGE_COLOR_TYPE type = FreeImage_GetColorType( fiImage );
		uint32_t width = FreeImage_GetWidth( fiImage );
		uint32_t height = FreeImage_GetHeight( fiImage );
		Size size{ width, height };

		if ( type == FIC_PALETTE )
		{
			if ( FreeImage_IsTransparent( fiImage ) )
			{
				ePF = ashes::Format::eR8G8B8A8_UNORM;
				FIBITMAP * dib = FreeImage_ConvertTo32Bits( fiImage );
				preMultiplyWithAlpha( dib );
				FreeImage_Unload( fiImage );
				fiImage = dib;

				if ( !fiImage )
				{
					LOADER_ERROR( "Can't convert image to 32 bits with alpha : " + string::stringCast< char >( p_path ) );
				}
			}
			else
			{
				ePF = ashes::Format::eR8G8B8A8_UNORM;
				FIBITMAP * dib = FreeImage_ConvertTo32Bits( fiImage );
				FreeImage_Unload( fiImage );
				fiImage = dib;

				if ( !fiImage )
				{
					LOADER_ERROR( "Can't convert image to 24 bits : " + string::stringCast< char >( p_path ) );
				}
			}
		}
		else if ( type == FIC_RGBALPHA )
		{
			ePF = ashes::Format::eR8G8B8A8_UNORM;
			FIBITMAP * dib = FreeImage_ConvertTo32Bits( fiImage );
			preMultiplyWithAlpha( dib );
			FreeImage_Unload( fiImage );
			fiImage = dib;

			if ( !fiImage )
			{
				LOADER_ERROR( "Can't convert image to 32 bits with alpha : " + string::stringCast< char >( p_path ) );
			}
		}
		else if ( fiFormat == FIF_HDR
			|| fiFormat == FIF_EXR )
		{
			auto bpp = FreeImage_GetBPP( fiImage ) / 8;

			if ( bpp == PixelDefinitions< ashes::Format::eR16G16B16_SFLOAT >::Size )
			{
				ePF = ashes::Format::eR16G16B16_SFLOAT;
			}
			else if ( bpp == PixelDefinitions< ashes::Format::eR32G32B32_SFLOAT >::Size )
			{
				ePF = ashes::Format::eR32G32B32_SFLOAT;
			}
			else
			{
				LOADER_ERROR( "Unsupported HDR image format" );
			}
		}
		else
		{
			ePF = ashes::Format::eR8G8B8A8_UNORM;
			FIBITMAP * dib = FreeImage_ConvertTo32Bits( fiImage );
			FreeImage_Unload( fiImage );
			fiImage = dib;

			if ( !fiImage )
			{
				LOADER_ERROR( "Can't convert image to 24 bits : " + string::stringCast< char >( p_path ) );
			}
		}

		if ( !p_image.m_buffer )
		{
			uint8_t * pixels = FreeImage_GetBits( fiImage );
#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR

			swapComponents( pixels, ePF, width, height );

#endif
			p_image.m_buffer = PxBufferBase::create( size, ePF, pixels, ePF );
			FreeImage_Unload( fiImage );
		}

		return p_image.m_buffer != nullptr;
	}

	//************************************************************************************************

	Image::BinaryWriter::BinaryWriter()
	{
	}

	bool Image::BinaryWriter::operator()( Image const & p_image, Path const & p_path )
	{
		bool result = false;
		FIBITMAP * fiImage = nullptr;
		Size const & size = p_image.getDimensions();
		int32_t w = int32_t( size.width );
		int32_t h = int32_t( size.height );

		if ( p_path.getExtension() == cuT( "png" ) )
		{
			fiImage = FreeImage_Allocate( w, h, 32 );
			PxBufferBaseSPtr pBufferRGB = PxBufferBase::create( size, ashes::Format::eR8G8B8A8_UNORM, p_image.getBuffer(), p_image.getPixelFormat() );

#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR

			swapComponents( pBufferRGB->ptr(), ashes::Format::eR8G8B8A8_UNORM, w, h );

#endif

			if ( fiImage )
			{

				memcpy( FreeImage_GetBits( fiImage ), pBufferRGB->constPtr(), pBufferRGB->size() );
				FREE_IMAGE_FORMAT fif = FIF_PNG;
				result = FreeImage_Save( fif, fiImage, string::stringCast< char >( p_path ).c_str(), 0 ) != 0;
				FreeImage_Unload( fiImage );
			}
		}
		else if ( p_path.getExtension() == cuT( "hdr" ) )
		{
			if ( PF::hasAlpha( p_image.getPixelFormat() ) )
			{
				fiImage = FreeImage_AllocateT( FIT_RGBAF, w, h );
				PxBufferBaseSPtr pBufferRGB = PxBufferBase::create( size, ashes::Format::eR32G32B32A32_SFLOAT, p_image.getBuffer(), p_image.getPixelFormat() );

#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR

				swapComponents( pBufferRGB->ptr(), ashes::Format::eR32G32B32A32_SFLOAT, w, h );

#endif

				if ( fiImage )
				{
					memcpy( FreeImage_GetBits( fiImage ), pBufferRGB->constPtr(), pBufferRGB->size() );
					FIBITMAP * dib = FreeImage_ConvertToRGBF( fiImage );
					FreeImage_Unload( fiImage );
					fiImage = dib;
				}
			}
			else
			{
				fiImage = FreeImage_AllocateT( FIT_RGBF, w, h );
				PxBufferBaseSPtr pBufferRGB = PxBufferBase::create( size, ashes::Format::eR32G32B32_SFLOAT, p_image.getBuffer(), p_image.getPixelFormat() );

#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR

				swapComponents( pBufferRGB->ptr(), ashes::Format::eR32G32B32_SFLOAT, w, h );

#endif

				if ( fiImage )
				{
					memcpy( FreeImage_GetBits( fiImage ), pBufferRGB->constPtr(), pBufferRGB->size() );
				}
			}

			if ( fiImage )
			{
				FREE_IMAGE_FORMAT fif = FIF_HDR;
				result = FreeImage_Save( fif, fiImage, string::stringCast< char >( p_path ).c_str(), 0 ) != 0;
				FreeImage_Unload( fiImage );
			}
		}
		else
		{
			PxBufferBaseSPtr pBuffer;

			if ( p_image.getPixelFormat() != ashes::Format::eR8_UNORM )
			{
				fiImage = FreeImage_Allocate( w, h, 24 );
				pBuffer = PxBufferBase::create( size, ashes::Format::eR8G8B8_UNORM, p_image.getBuffer(), p_image.getPixelFormat() );

#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR

				swapComponents( pBuffer->ptr(), ashes::Format::eR8G8B8_UNORM, w, h );

#endif
				w *= 3;
			}
			else
			{
				fiImage = FreeImage_Allocate( w, h, 8 );
				pBuffer = p_image.getPixels();
			}

			if ( fiImage && pBuffer )
			{
				uint32_t pitch = FreeImage_GetPitch( fiImage );
				uint8_t * dst = FreeImage_GetBits( fiImage );
				uint8_t const * src = pBuffer->constPtr();

				if ( !( pitch % w ) )
				{
					memcpy( dst, src, pBuffer->size() );
				}
				else
				{
					for ( int32_t i = 0; i < h; ++i )
					{
						memcpy( dst, src, w );
						dst += pitch;
						src += w;
					}
				}

				result = FreeImage_Save( FIF_BMP, fiImage, string::stringCast< char >( p_path ).c_str(), 0 ) != 0;
				FreeImage_Unload( fiImage );
			}
		}

		return result;
	}

	//************************************************************************************************

	Image & Image::resample( Size const & p_size )
	{
		Size const & size = getDimensions();

		if ( p_size != size )
		{
			FIBITMAP * fiImage = nullptr;
			int32_t w = int32_t( size.width );
			int32_t h = int32_t( size.height );
			ashes::Format ePF = getPixelFormat();
			uint32_t uiBpp = PF::getBytesPerPixel( ePF );

			switch ( ePF )
			{
			case ashes::Format::eR8G8B8_UNORM:
				fiImage = FreeImage_AllocateT( FIT_BITMAP, w, h, 24 );
				break;

			case ashes::Format::eR8G8B8A8_UNORM:
				fiImage = FreeImage_AllocateT( FIT_BITMAP, w, h, 32 );
				break;
			}

			if ( fiImage )
			{
#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR

				swapComponents( m_buffer->ptr(), m_buffer->format(), w, h );

#endif
				memcpy( FreeImage_GetBits( fiImage ), m_buffer->constPtr(), m_buffer->size() );
				uint32_t width = p_size.width;
				uint32_t height = p_size.height;
				FREE_IMAGE_COLOR_TYPE type = FreeImage_GetColorType( fiImage );
				FIBITMAP * pRescaled = FreeImage_Rescale( fiImage, width, height, FILTER_BICUBIC );

				if ( pRescaled )
				{
					FreeImage_Unload( fiImage );
					fiImage = pRescaled;
					width = FreeImage_GetWidth( fiImage );
					height = FreeImage_GetHeight( fiImage );
					uint8_t * pixels = FreeImage_GetBits( fiImage );

#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR

					swapComponents( pixels, m_buffer->format(), width, height );

#endif
					m_buffer = PxBufferBase::create( p_size, ePF, pixels, ePF );
				}

				FreeImage_Unload( fiImage );
			}
		}

		return *this;
	}

	void Image::initialiseImageLib()
	{
		FreeImage_Initialise();
		FreeImage_SetOutputMessage( freeImageErrorHandler );
	}

	void Image::cleanupImageLib()
	{
		FreeImage_DeInitialise();
	}

	//************************************************************************************************
}

#endif