/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_SOURCE_YUM_REPOMDFILECOLLECTOR
#define ZYPP_SOURCE_YUM_REPOMDFILECOLLECTOR

#include <zypp-core/base/Easy.h>
#include <zypp-core/OnMediaLocation>
#include <zypp-core/Pathname.h>
#include <zypp/Locale.h>
#include <map>
#include <string>
#include <functional>

namespace zypp::repo::yum
{

  struct RepomdFileCollector
  {
    NON_COPYABLE( RepomdFileCollector );
    NON_MOVABLE( RepomdFileCollector );

    using FinalizeCb = std::function<void ( const OnMediaLocation &file )>;

    RepomdFileCollector( const Pathname & destDir_r );
    virtual ~RepomdFileCollector();

    bool collect( const OnMediaLocation & loc_r, const std::string & typestr_r );

    void finalize( const FinalizeCb &cb );

  protected:
    virtual const RepoInfo &repoInfo() const = 0;
    virtual const Pathname &deltaDir() const = 0;

  private:

    bool wantLocale( const Locale & locale_r ) const;
    void addWantedLocale( Locale locale_r );

  protected:
    const Pathname & _destDir;

    LocaleSet _wantedLocales;	///< Locales do download
    std::map<std::string,OnMediaLocation> _wantedFiles;
  };
}
#endif
