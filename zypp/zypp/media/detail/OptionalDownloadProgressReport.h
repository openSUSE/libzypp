/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_MEDIA_DETAIL_OPTIONALDOWNLOADPROGRESSREPORT_INCLUDED
#define ZYPP_MEDIA_DETAIL_OPTIONALDOWNLOADPROGRESSREPORT_INCLUDED

#include <zypp/ZYppCallbacks.h>
#include <zypp/Callback.h>


namespace internal {

  /// \brief Bottleneck filtering all DownloadProgressReport issued from Media[Muli]Curl.
  /// - Optional files will send no report until data are actually received (we know it exists).
  /// - Control the progress report frequency passed along to the application.
  struct OptionalDownloadProgressReport : public zypp::callback::ReceiveReport<zypp::media::DownloadProgressReport>
  {
    using TimePoint = std::chrono::steady_clock::time_point;

    OptionalDownloadProgressReport( bool isOptional=false );

    OptionalDownloadProgressReport(const OptionalDownloadProgressReport &) = delete;
    OptionalDownloadProgressReport(OptionalDownloadProgressReport &&) = delete;
    OptionalDownloadProgressReport &operator= (const OptionalDownloadProgressReport &) = delete;
    OptionalDownloadProgressReport &operator= (OptionalDownloadProgressReport &&) = delete;

    ~OptionalDownloadProgressReport() override;

    void reportbegin() override;

    void reportend() override;

    void report( const UserData & userData_r = UserData() ) override;

    void start( const zypp::Url & file_r, zypp::Pathname localfile_r ) override;

    bool progress( int value_r, const zypp::Url & file_r, double dbps_avg_r = -1, double dbps_current_r = -1 ) override;

    Action problem( const zypp::Url & file_r, Error error_r, const std::string & description_r ) override;

    void finish( const zypp::Url & file_r, Error error_r, const std::string & reason_r ) override;

  private:
    // _isOptional also indicates the delayed start
    bool notStarted() const;

    void sendStart();

  private:
    Receiver *const _oldRec;
    bool     _isOptional;
    zypp::Url      _startFile;
    zypp::Pathname _startLocalfile;
    TimePoint _lastProgressSent;
  };


}


#endif
