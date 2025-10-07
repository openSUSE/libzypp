/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-core/fs/TmpPath.cc
 *
*/

#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <iostream>
#include <utility>

#include <zypp-core/base/ReferenceCounted.h>
#include <zypp-core/base/NonCopyable.h>
#include <zypp-core/base/Logger.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/fs/TmpPath.h>

using std::endl;

namespace zypp {
  namespace filesystem {

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : TmpPath::Impl
    //
    /**
     * Clean or delete a directory on destruction.
     **/
    class TmpPath::Impl : public base::ReferenceCounted, private base::NonCopyable
    {
      public:

        enum Flags
          {
            NoOp         = 0,
            Autodelete   = 1L << 0,
            KeepTopdir   = 1L << 1,
            //
            CtorDefault  = Autodelete
          };

    public:
        Impl(Pathname &&path_r, Flags flags_r = CtorDefault)
          : _path(std::move(path_r)), _flags(flags_r) {
            MIL << _path << endl;
          }

        Impl(const Impl &) = delete;
        Impl(Impl &&) = delete;
        Impl &operator=(const Impl &) = delete;
        Impl &operator=(Impl &&) = delete;

        ~Impl() override
        {
          if ( ! (_flags & Autodelete) || _path.empty() )
            return;

          PathInfo p( _path, PathInfo::LSTAT );
          if ( ! p.isExist() )
            return;

          int res = 0;
          if ( p.isDir() )
            {
              if ( _flags & KeepTopdir )
                res = clean_dir( _path );
              else
                res = recursive_rmdir( _path );
            }
          else
            res = unlink( _path );

          if ( res )
            INT << "TmpPath cleanup error (" << res << ") " << p << endl;
          else
            DBG << "TmpPath cleaned up " << p << endl;
        }

        const Pathname & path() const
        { return _path; }

        bool autoCleanup() const
        { return( _flags & Autodelete ); }

        void autoCleanup( bool yesno_r )
        { _flags = yesno_r ? CtorDefault : NoOp; }

      private:
        Pathname _path;
        Flags    _flags;
    };
    ///////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //	CLASS NAME : TmpPath
    ///////////////////////////////////////////////////////////////////

    TmpPath::TmpPath()
    {}

    TmpPath::TmpPath( Pathname tmpPath_r )
    :_impl( tmpPath_r.empty() ? nullptr : new Impl( std::move(tmpPath_r) ) )
    {}

    TmpPath::~TmpPath()
    {
      // virtual not inlined dtor.
    }

    TmpPath::operator bool() const
    {
      return _impl.get();
    }

    Pathname TmpPath::path() const
    {
      return _impl.get() ? _impl->path() : Pathname();
    }

    const Pathname & TmpPath::defaultLocation()
    {
      static Pathname p( getenv("ZYPPTMPDIR") ? getenv("ZYPPTMPDIR") : "/var/tmp" );
      return p;
    }

    bool TmpPath::autoCleanup() const
    { return _impl.get() ? _impl->autoCleanup() : false; }

    void TmpPath::autoCleanup( bool yesno_r )
    { if ( _impl.get() ) _impl->autoCleanup( yesno_r ); }

    ///////////////////////////////////////////////////////////////////
    //	CLASS NAME : TmpFile
    ///////////////////////////////////////////////////////////////////

    TmpFile::TmpFile( const Pathname & inParentDir_r,
                      const std::string & prefix_r )
    {
      // parent dir must exist
      if ( filesystem::assert_dir( inParentDir_r ) != 0 )
      {
        ERR << "Parent directory '" << inParentDir_r << "' can't be created." << endl;
        return;
      }

      // create the temp file
      Pathname tmpPath = (inParentDir_r + prefix_r).extend( "XXXXXX");
      AutoFREE<char> buf { ::strdup( tmpPath.asString().c_str() ) };
      if ( ! buf )
        {
          ERR << "Out of memory" << endl;
          return;
        }

      int tmpFd = ::mkostemp( buf, O_CLOEXEC );
      if ( tmpFd != -1 )
        {
          // success; create _impl
          ::close( tmpFd );
          _impl = RW_pointer<Impl>( new Impl( Pathname(buf) ) );
        }
      else
        ERR << "Cant create '" << buf << "' " << ::strerror( errno ) << endl;
    }

    TmpFile TmpFile::makeSibling( const Pathname & sibling_r )
    { return makeSibling( sibling_r, -1U ); }

    TmpFile TmpFile::makeSibling( const Pathname & sibling_r, unsigned mode )
    {
      TmpFile ret( sibling_r.dirname(), sibling_r.basename() );
      if ( ret ) {
        // clone mode if sibling_r exists
        PathInfo p( sibling_r );
        if ( p.isFile() ) {
          ::chmod( ret.path().c_str(), p.st_mode() );
        } else if ( mode != -1U ) {
          ::chmod( ret.path().c_str(), applyUmaskTo( mode ) );
        }
      }
      return ret;
    }

    ManagedFile TmpFile::asManagedFile()
    {
      return asManagedFile( defaultLocation(), defaultPrefix() );
    }

    ManagedFile TmpFile::asManagedFile(const Pathname &inParentDir_r, const std::string &prefix_r)
    {
      filesystem::TmpFile tmpFile( inParentDir_r, prefix_r );
      ManagedFile mFile ( tmpFile.path(), filesystem::unlink );
      tmpFile.autoCleanup(false); //cleaned up by ManagedFile
      return mFile;
    }

    const std::string & TmpFile::defaultPrefix()
    {
      static std::string p( "TmpFile." );
      return p;
    }

    ///////////////////////////////////////////////////////////////////
    //	CLASS NAME : TmpDir
    ///////////////////////////////////////////////////////////////////

    TmpDir::TmpDir( const Pathname & inParentDir_r,
                    const std::string & prefix_r )
    {
      // parent dir must exist
      if ( filesystem::assert_dir( inParentDir_r ) != 0  )
      {
        ERR << "Parent directory '" << inParentDir_r << "' can't be created." << endl;
        return;
      }

      // create the temp dir
      Pathname tmpPath = (inParentDir_r + prefix_r).extend( "XXXXXX");
      AutoFREE<char> buf { ::strdup( tmpPath.asString().c_str() ) };
      if ( ! buf )
        {
          ERR << "Out of memory" << endl;
          return;
        }

      char * tmp = ::mkdtemp( buf );
      if ( tmp )
        // success; create _impl
        _impl = RW_pointer<Impl>( new Impl( tmp ) );
      else
        ERR << "Cant create '" << tmpPath << "' " << ::strerror( errno ) << endl;
    }

    TmpDir TmpDir::makeSibling( const Pathname & sibling_r )
    { return makeSibling( sibling_r, -1U ); }

    TmpDir TmpDir::makeSibling( const Pathname & sibling_r, unsigned mode )
    {
      TmpDir ret( sibling_r.dirname(), sibling_r.basename() );
      if ( ret ) {
        // clone mode if sibling_r exists
        PathInfo p( sibling_r );
        if ( p.isDir() ) {
          ::chmod( ret.path().c_str(), p.st_mode() );
        } else if ( mode != -1U ) {
          ::chmod( ret.path().c_str(), applyUmaskTo( mode ) );
        }
      }
      return ret;
    }

    const std::string & TmpDir::defaultPrefix()
    {
      static std::string p( "TmpDir." );
      return p;
    }

  } // namespace filesystem

  Pathname myTmpDir()
  {
    static filesystem::TmpDir _tmpdir( filesystem::TmpPath::defaultLocation(), "zypp." );
    return _tmpdir.path();
  }

} // namespace zypp
