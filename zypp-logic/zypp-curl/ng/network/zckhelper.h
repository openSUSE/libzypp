/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPPNG_CURL_ZCKHELPER_H_INCLUDED
#define ZYPPNG_CURL_ZCKHELPER_H_INCLUDED

#include <zypp-curl/ng/network/curlmultiparthandler.h>
#include <zypp-curl/ng/network/rangedesc.h>

#include <zypp-core/AutoDispose.h>
#include <zypp-core/ng/base/Base>

extern "C" {
  typedef struct zckCtx zckCtx;
}

namespace zyppng {

  class ZckError : public zypp::Exception
  {
  public:
    ZckError( const std::string & msg_r );
    ZckError( std::string && msg_r );
  };

  class ZckLoader : public Base {
  public:

      enum State {
        Initial,
        DownloadLead,
        DownloadHeader,
        DownloadChunks,
        Finished
      };

    using Block = RangeDesc;

    struct PrepareResult {
      enum Code {
        Error,          // we got an error
        NothingToDo,    // Target file is already complete
        ExceedsMaxLen,  // Zchunk header reports a different filesize than what was expected
        Success         // Returns a list of blocks to fetch
      };

      Code _code;
      std::vector<Block> _blocks;
      zypp::ByteCount _bytesReused;
      std::string _message;
    };


    /*!
     * This function kickstarts the build process. The sigBlocksRequired and sigFinished signals need to be connected in order to
     * download required blocks or get the finished event.
     */
    expected<void> buildZchunkFile( const zypp::Pathname &target, const zypp::Pathname &delta, const std::optional<zypp::ByteCount> &expectedFileSize, const std::optional<zypp::ByteCount> &zcKHeaderSize );

    /*!
     * Advances the statemachine after sigBlocksRequired was triggered and blocks have been downloaded
     * into the targetFile
     */
    expected<void> cont();

    /*!
     * Advances the statemanchine into the failed state and emits sigFinished
     */
    void setFailed( const std::string &msg );

    /**
     * Signal to notify the caller about required blocks, once the blocks are downloaded call
     * \ref cont to continue with the process
     */
    SignalProxy<void(const std::vector<Block> &)> sigBlocksRequired();

    /**
     * Called once the zchunk build process is finished, either with error or success
     */
    SignalProxy<void( PrepareResult )> sigFinished();


    /**
     * The minimum size to download to have enough data to know the full header size
     */
    static zypp::ByteCount minZchunkDownloadSize();

    /*!
     * Checks if a given file is a zck file
     */
    static bool isZchunkFile(const zypp::Pathname &file);

    /*!
     * Prepares the file in \a target with already existing chunks in \a delta. The \a target file must
     * already contain the zck header data of the to be downloaded file.
     */
    static PrepareResult prepareZck ( const zypp::Pathname &delta, const zypp::Pathname &target, const zypp::ByteCount &expectedFileSize );

    /*!
     * Checks if a given zck file is internally valid
     */
    static bool validateZckFile( const zypp::Pathname &file, std::string &error );


  private:
    State _state = Initial;
    zypp::AutoDispose<zckCtx *> _zchunkContext;
    zypp::AutoFD _targetFd;
    zypp::ByteCount _bytesReused = 0;

    zypp::Pathname _target;
    zypp::Pathname _delta;
    std::optional<zypp::ByteCount> _expectedFileSize;
    std::optional<zypp::ByteCount> _zcKHeaderSize;

    Signal<void(const std::vector<Block> &)> _sigBlocksRequired;
    Signal<void( PrepareResult )> _sigFinished;
  };
}
#endif
