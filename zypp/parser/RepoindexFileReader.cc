/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/parser/RepoindexFileReader.cc
 * Implementation of repoindex.xml file reader.
 */

#include "RepoindexFileReader.h"
#include <zypp/ng/parser/repoindexfilereader.h>
#include <zypp/zypp_detail/ZYppImpl.h>
#include <zypp/RepoInfo.h>

namespace zypp
{
  namespace parser
  {
  RepoindexFileReader::RepoindexFileReader( Pathname repoindex_file, ProcessResource callback )
    : _pimpl( new Impl( zypp_detail::GlobalStateHelper::context()
              , InputStream(std::move(repoindex_file))
              , [cb = std::move(callback)]( const zyppng::RepoInfo &ng ){ return cb(RepoInfo(ng)); } ))
  {}

  RepoindexFileReader::RepoindexFileReader(const InputStream &is, ProcessResource callback )
  : _pimpl( new Impl( zypp_detail::GlobalStateHelper::context()
                      ,is
                      , [cb = std::move(callback)]( const zyppng::RepoInfo &ng ){ return cb(RepoInfo(ng)); } ))
  {}

  RepoindexFileReader::~RepoindexFileReader()
  {}

  Date::Duration RepoindexFileReader::ttl() const { return _pimpl->ttl(); }

  } // ns parser
} // ns zypp

// vim: set ts=2 sts=2 sw=2 et ai:
