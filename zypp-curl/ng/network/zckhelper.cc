/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "zckhelper.h"
#include <zypp-core/AutoDispose.h>
#include <zypp-curl/ng/network/private/mediadebug_p.h>
#include <fcntl.h>
#include <fstream>

extern "C" {
  #include <zck.h>
}

namespace zyppng {

  bool ZckHelper::isZchunkFile(const zypp::Pathname &file) {
    std::ifstream dFile(file.c_str());
    if (!dFile.is_open())
      return false;

    constexpr std::string_view magic("\0ZCK1", 5);

    std::array<char, magic.size()> lead;
    lead.fill('\0');
    dFile.read(lead.data(), lead.size());
    return (magic == std::string_view(lead.data(), lead.size()));
  }

  ZckHelper::PrepareResult ZckHelper::prepareZck( const zypp::Pathname &delta, const zypp::Pathname &target, const zypp::ByteCount &expectedFileSize )
  {
    const auto &setFailed = []( PrepareResult::Code code, std::string message ){
      return PrepareResult {
        ._code = code,
        ._blocks = std::vector<Block>(),
        ._bytesReused = 0,
        ._message = std::move(message),
      };
    };

    zypp::AutoFD src_fd = open( delta.asString().c_str(), O_RDONLY);
    if(src_fd < 0)
      return setFailed ( PrepareResult::Error, zypp::str::Format("Unable to open %1%") % delta );

    zypp::AutoDispose<zckCtx *> zck_src ( zck_create(), []( auto ptr ) { if ( ptr ) zck_free( &ptr ); } );
    if( !zck_src )
      return setFailed ( PrepareResult::Error, zypp::str::Format("%1%") % zck_get_error(NULL) );

    if(!zck_init_read(zck_src, src_fd))
      return setFailed ( PrepareResult::Error, zypp::str::Format( "Unable to open %1%: %2%") %  delta % zck_get_error(zck_src) );

    zypp::AutoFD target_fd = open( target.asString().c_str(), O_RDWR);
    if(target_fd < 0)
      return setFailed ( PrepareResult::Error, zypp::str::Format("Unable to open %1%") % target );

    zypp::AutoDispose<zckCtx *> zckTarget ( zck_create(), []( auto ptr ) { if ( ptr ) zck_free( &ptr ); } );
    if( !zckTarget )
      return setFailed ( PrepareResult::Error, zypp::str::Format("%1%") % zck_get_error(NULL) );

    if(!zck_init_read(zckTarget, target_fd))
      return setFailed ( PrepareResult::Error, zypp::str::Format( "Unable to open %1%: %2%") %  target % zck_get_error(zckTarget) );

    // Returns 0 for error, -1 for invalid checksum and 1 for valid checksum
    switch ( zck_find_valid_chunks(zckTarget) ) {
      case 0: // Returns 0 if there was a error
        return setFailed ( PrepareResult::Error, zypp::str::Format( "Unable to open %1%: %2%") %  target % zck_get_error(zckTarget) );
      case 1: // getting a 1 would mean the file is already complete, basically impossible but lets handle it anyway
        return PrepareResult {
          ._code = PrepareResult::NothingToDo
        };
    }

    const auto srcHashType = zck_get_chunk_hash_type( zckTarget );
    const auto targetHashType = zck_get_chunk_hash_type( zckTarget );

    auto _fileSize = expectedFileSize;

    const size_t fLen = zck_get_length( zckTarget );
    if ( expectedFileSize > 0 ) {
      // check if the file size as reported by zchunk is equal to the one we expect
      if ( expectedFileSize != fLen ) {
        return setFailed(
              PrepareResult::ExceedsMaxLen,
              zypp::str::Format("Zchunk header reports a different filesize than what was expected ( Zck: %1% != Exp: %2%).") % fLen % _fileSize
              );
      }
    } else {
      _fileSize = fLen;
    }

    if( srcHashType != targetHashType )
      return setFailed ( PrepareResult::Error, zypp::str::Format( "ERROR: Chunk hash types don't match. Source Hash: %1% vs Target Hash: %2%")
                        % zck_hash_name_from_type ( srcHashType )
                        % zck_hash_name_from_type ( targetHashType ) );

    std::vector<Block> ranges;

    if(!zck_copy_chunks( zck_src, zckTarget ))
      return setFailed ( PrepareResult::Error, zypp::str::Format( "Unable to copy chunks from deltafile.") );

        // we calculate what is already downloaded by substracting the block sizes we still need to download from the full file size
        auto bytesReused = _fileSize;

        auto chunk = zck_get_first_chunk( zckTarget );
        do {
          // Get validity of current chunk: 1 = valid, 0 = missing, -1 = invalid
          if ( zck_get_chunk_valid( chunk ) == 1 )
            continue;

          zypp::AutoFREE<char> zckDigest( zck_get_chunk_digest( chunk ) );
          UByteArray chksumVec = zypp::Digest::hexStringToUByteArray( std::string_view( zckDigest.value() ) );
          std::string chksumName;
          std::optional<size_t> chksumCompareLen;

          switch ( targetHashType ) {
            case ZCK_HASH_SHA1: {
              chksumName = zypp::Digest::sha1();
              break;
            }
            case ZCK_HASH_SHA256: {
              chksumName = zypp::Digest::sha256();
              break;
            }
            case ZCK_HASH_SHA512: {
              chksumName = zypp::Digest::sha512();
              break;
            }
            case ZCK_HASH_SHA512_128: {
              // defined in zchunk as
              // SHA-512/128 (first 128 bits of SHA-512 checksum)
              chksumName = zypp::Digest::sha512();
              chksumCompareLen = chksumVec.size();
              break;
            }
            default: {
              return setFailed ( PrepareResult::Error, zypp::str::Format( "Unsupported chunk hash type: %1%.") % zck_hash_name_from_type( targetHashType ) );
            }
          }

          const auto s = static_cast<size_t>( zck_get_chunk_start( chunk ) );
          const auto l = static_cast<size_t>( zck_get_chunk_comp_size ( chunk ) );

          MIL_MEDIA << "Downloading block " << s << " with length " << l << " checksum " << zckDigest.value() << " type " << chksumName << std::endl;
          ranges.push_back( Block {
            ._start = s,
            ._len   = l,
            ._chksumtype = chksumName,
            ._checksum  = std::move( chksumVec ),
            ._relevantDigestLen = std::move(chksumCompareLen)
          } );

          // substract the block length from the already downloaded bytes size
          bytesReused -= l;

        } while ( (chunk = zck_get_next_chunk( chunk )) );

        return PrepareResult {
          ._code = PrepareResult::Success,
          ._blocks = std::move(ranges),
          ._bytesReused = std::move(bytesReused),
          ._message = std::string()
        };
  }

  bool ZckHelper::validateZckFile(const zypp::Pathname &file, std::string &error)
  {
    const auto &setFailed = [&]( std::string &&err ) {
      error = std::move(err);
      return false;
    };

    zypp::AutoFD target_fd = open( file.asString().c_str(), O_RDONLY );
    if( target_fd < 0 )
      return setFailed ( zypp::str::Format("Unable to open %1%") % file );

    zypp::AutoDispose<zckCtx *> zckTarget ( zck_create(), []( auto ptr ) { if ( ptr ) zck_free( &ptr ); } );
    if( !zckTarget )
      return setFailed ( zypp::str::Format("%1%") % zck_get_error(nullptr) );

    if(!zck_init_read(zckTarget, target_fd))
      return setFailed ( zypp::str::Format( "Unable to open %1%: %2%") %  file % zck_get_error(zckTarget) );

    /* Validate the chunk and data checksums for the current file.
     * Returns 0 for error, -1 for invalid checksum and 1 for valid checksum */
    const auto res = zck_validate_checksums( zckTarget );
    if ( res == 0 || res == -1 ) {
      if( zck_is_error(nullptr) ) {
        std::string err = zck_get_error(NULL);
        zck_clear_error(NULL);
        return setFailed( std::move(err) );
      }
      if( zck_is_error(zckTarget) )
        return setFailed( zck_get_error(zckTarget) );
      return setFailed( "zck_validate_checksums returned a unknown error." );
    }

    return true;
  }





} // namespace zyppng
