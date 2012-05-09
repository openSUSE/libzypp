/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_REPO_SERVICE_REPOS
#define ZYPP_REPO_SERVICE_REPOS

#include "zypp/base/Macros.h"
#include "zypp/base/NonCopyable.h"
#include "zypp/ProgressData.h"
#include "zypp/ServiceInfo.h"
#include "zypp/RepoInfo.h"

namespace zypp
{
  namespace repo
  {
    /**
     * Retrieval of repository list for
     * a service
     */
    class ZYPP_API ServiceRepos : private base::NonCopyable
    {
    public:

     /**
      * Callback definition.
      * First parameter is a \ref RepoInfo object with the resource
      * second parameter is the resource type.
      *
      * Return false from the callback to get a \ref AbortRequestException
      * to be thrown and the processing to be cancelled.
      */
      typedef function< bool( const RepoInfo & )> ProcessRepo;

      ServiceRepos(const ServiceInfo &service,
                   const ProcessRepo & callback,
                   const ProgressData::ReceiverFnc &progress = ProgressData::ReceiverFnc() );
      ~ServiceRepos();

      /** Implementation  */
      class Impl;
    private:
      RW_pointer<Impl> _impl;
    };
  } // ns repo
} // ns zypp

#endif
