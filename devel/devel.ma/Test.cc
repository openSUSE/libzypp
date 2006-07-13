#include <iostream>
#include <fstream>

#include "zypp/base/LogControl.h"
#include "zypp/base/LogTools.h"
#include <zypp/base/Logger.h>

#include "zypp/Source.h"
#include "zypp/Package.h"
#include "zypp/source/PackageDelta.h"
#include "zypp/detail/ImplConnect.h"
#include "zypp/ExternalProgram.h"
#include "zypp/AutoDispose.h"

using std::endl;
//using namespace std;
using namespace zypp;

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
namespace zypp
{
  typedef AutoDispose<const Pathname> ManagedFile;

  /////////////////////////////////////////////////////////////////
  ManagedFile sourceProvideFile( Source_Ref source_r, const source::OnMediaLocation & loc_r )
  {
    ManagedFile ret;
    INT << "sourceProvideFile " << loc_r << endl;
    try
      {
        ret = ManagedFile( source_r.provideFile( loc_r.filename(), loc_r.medianr() ),
                           boost::bind( &Source_Ref::releaseFile, source_r, _1, loc_r.medianr() ) );
      }
    catch( const Exception & excpt )
      {
        // download file error
        ZYPP_CAUGHT( excpt );
        return ManagedFile();
      }

    if ( loc_r.checksum().empty() )
      {
        // no checksum in metadata
        INT << "no checksum in metadata " << loc_r << endl;
      }
    else
      {
        std::ifstream input( ret->asString().c_str() );
        CheckSum retChecksum( loc_r.checksum().type(), input );
        input.close();
        if ( retChecksum.empty() )
          {
            // Can not compute checksum of downloaded file
            INT << "Can not compute checksum of file " << ret << endl;
            return ManagedFile();
          }
        else
          {
            if ( loc_r.checksum() != retChecksum )
              {
                // fails integity check
                INT << "Failed integity check: " << ret << " " << retChecksum
                    << " - expected " << loc_r.checksum() << endl;
                return ManagedFile();
              }
          }
      }

    INT << "Return:  " << ret << endl;
    return ret;
  }



  struct Applydeltarpm
  {
    static bool quickcheck( const std::string & sequenceinfo_r )
    { return check( sequenceinfo_r, true ); }

    static bool check( const std::string & sequenceinfo_r, bool quick_r = false )
    {
      if ( sequenceinfo_r.empty() )
        {
          DBG << "Applydeltarpm " << (quick_r?"quickcheck":"check") << " -> empty sequenceinfo" << endl;
          return false;
        }

      const char* argv[] = {
        "/usr/bin/applydeltarpm",
        ( quick_r ? "-C" : "-c" ),
        "-s", sequenceinfo_r.c_str(),
        NULL
      };

      ExternalProgram prog( argv, ExternalProgram::Stderr_To_Stdout );
      for ( std::string line = prog.receiveLine(); ! line.empty(); line = prog.receiveLine() )
        {
          DBG << "Applydeltarpm " << (quick_r?"quickcheck":"check") << ": " << line;
        }

      int exit_code = prog.close();
      DBG << "Applydeltarpm " << (quick_r?"quickcheck":"check") << " -> " << exit_code << endl;
      return( exit_code == 0 );
    }

    static bool provide( const Pathname & delta_r, const Pathname & new_r )
    {
      const char* argv[] = {
        "/usr/bin/applydeltarpm",
        "-p",
        "-v",
        delta_r.asString().c_str(),
        new_r.asString().c_str(),
        NULL
      };

      ExternalProgram prog( argv, ExternalProgram::Stderr_To_Stdout );
      for ( std::string line = prog.receiveLine(); ! line.empty(); line = prog.receiveLine() )
        {
          DBG << "Applydeltarpm: " << line;
        }

      int exit_code = prog.close();
      if ( exit_code != 0 )
        filesystem::unlink( new_r );
      DBG << "Applydeltarpm -> " << exit_code << endl;
      return( exit_code == 0 );
    }
  };

  class PackageProvider
  {
    typedef detail::ResImplTraits<Package::Impl>::constPtr PackageImpl_constPtr;

    typedef packagedelta::DeltaRpm DeltaRpm;
    typedef packagedelta::PatchRpm PatchRpm;


  public:
    PackageProvider( const Package::constPtr & package )
    : _package( package )
    , _implPtr( detail::ImplConnect::resimpl( _package ) )
    {}

    ~PackageProvider()
    {}

  public:
    ManagedFile get() const
    {
      DBG << "provide Package " << _package << endl;
      ManagedFile ret( doProvidePackage() );

      if ( ret->empty() )
        {
          ERR << "Failed to provide Package " << _package << endl;
#warning THROW ON ERROR
          return ManagedFile();
        }

      MIL << "provided Package " << _package << " at " << ret << endl;
      return ret;
    }

  private:
    bool considerDeltas() const
    { return true; }

    bool considerPatches() const
    { return true; }

    ManagedFile doProvidePackage() const
    {
      if ( considerDeltas() )
        {
          std::list<DeltaRpm> deltaRpms( _implPtr->deltaRpms() );
          DBG << "deltas: " << deltaRpms.size() << endl;
          if ( ! deltaRpms.empty() )
            {
              for( std::list<DeltaRpm>::const_iterator it = deltaRpms.begin();
                   it != deltaRpms.end(); ++it )
                {
                  DBG << "tryDelta " << *it << endl;
                  ManagedFile ret( tryDelta( *it ) );
                  if ( ! ret->empty() )
                    return ret;
                }
            }
        }

      if ( considerPatches() )
        {
          std::list<PatchRpm> patchRpms( _implPtr->patchRpms() );
          DBG << "patches: " << patchRpms.size() << endl;
          if ( ! patchRpms.empty() )
            {
             for( std::list<PatchRpm>::const_iterator it = patchRpms.begin();
                   it != patchRpms.end(); ++it )
                {
                  DBG << "tryPatch " << *it << endl;
                  ManagedFile ret( tryPatch( *it ) );
                  if ( ! ret->empty() )
                    return ret;
                }
            }
        }

      INT << "Need whole package download!" << endl;

      source::OnMediaLocation().medianr( _package->sourceMediaNr() )
                               .filename( _package->location() )
                               .checksum( _package->checksum() )
                               .downloadsize( _package->archivesize() );

      ManagedFile ret( sourceProvideFile( _package->source(),
                                          source::OnMediaLocation()
                                          .medianr( _package->sourceMediaNr() )
                                          .filename( _package->location() )
                                          .checksum( _package->checksum() ) ) );
      return ret;
    }

    ManagedFile tryDelta( const DeltaRpm & delta_r ) const
    {
      if ( ! Applydeltarpm::quickcheck( delta_r.baseversion().sequenceinfo() ) )
        return ManagedFile();

      ManagedFile delta( sourceProvideFile( _package->source(), delta_r.location() ) );
      if ( delta->empty() )
        return ManagedFile();

      if ( ! Applydeltarpm::check( delta_r.baseversion().sequenceinfo() ) )
        return ManagedFile();

      ManagedFile ret( "/tmp/delta.rpm",
                       filesystem::unlink );

      if ( ! Applydeltarpm::provide( delta, ret ) )
        return ManagedFile();

      return ret;
    }

    ManagedFile tryPatch( const PatchRpm & patch_r ) const
    {
      // installed edition is in baseversions?
      const PatchRpm::BaseVersions & baseversions( patch_r.baseversions() );
      USR << baseversions << endl;
      if ( std::find( baseversions.begin(), baseversions.end(), Edition() ) == baseversions.end() )
        return ManagedFile();

      ManagedFile patch( sourceProvideFile( _package->source(), patch_r.location() ) );
      if ( patch->empty() )
        return ManagedFile();

      return patch;
    }


  private:
    Package::constPtr    _package;
    PackageImpl_constPtr _implPtr;
  };



  Pathname testProvidePackage( Package::constPtr package )
  {
    if ( package->arch() != Arch_x86_64 )
      return Pathname();

    PackageProvider pkgProvider( package );

    ManagedFile r = pkgProvider.get();

    return Pathname();
  }
}
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

#include "Tools.h"
#include "zypp/ZYppFactory.h"
#include "zypp/ZYppCallbacks.h"

namespace zypp
{
  class Dln : public callback::ReceiveReport<media::DownloadProgressReport>
  {
    virtual void reportbegin()
    { _SEC("REPORT") << "+++DownloadProgress" << endl; }
    virtual void reportend()
    { _SEC("REPORT") << "---DownloadProgress" << endl; }
  };

  class Sfp : public callback::ReceiveReport<source::DownloadFileReport>
  {
    virtual void reportbegin()
    { _SEC("REPORT") << "+++source::DownloadFile" << endl; }
    virtual void reportend()
    { _SEC("REPORT") << "---source::DownloadFile" << endl; }
  };

  class SRP : public callback::ReceiveReport<source::DownloadResolvableReport>
  {
    virtual void reportbegin()
    { _SEC("REPORT") << "+++source::DownloadResolvable" << endl; }
    virtual void reportend()
    { _SEC("REPORT") << "---source::DownloadResolvable" << endl; }
  };
}

void test( const ResObject::constPtr & res )
{
  if ( ! isKind<Package>( res ) )
    return;

  SEC << "Test " << res << endl;

  if ( ! res->source() )
    return;

  try
    {
      MIL << zypp::testProvidePackage( asKind<Package>( res ) ) << endl;
    }
  catch ( Exception & expt )
    {
      ERR << expt << endl;
    }
}

void show( const Pathname & file )
{
  WAR << "show " << PathInfo( file ) << endl;
}

struct ValRelease
{
  ValRelease( int i )
  : _i( i )
  {}
  void operator()( const Pathname & file ) const
  { WAR << "ValRelease " << _i << " " << PathInfo( file ) << endl; }

  int _i;
};

/******************************************************************
**
**      FUNCTION NAME : main
**      FUNCTION TYPE : int
*/
int main( int argc, char * argv[] )
{
  //zypp::base::LogControl::instance().logfile( "log.restrict" );
  INT << "===[START]==========================================" << endl;
  if ( 0 ) {
    ManagedFile f;
    {
      f = ManagedFile( "./xxx", show );
      f = ManagedFile( "./xxx", ValRelease(3) );
      ValRelease r(4);
      f = ManagedFile( "./xxx", boost::ref(r) );
      r._i=5;
    }
    MIL << endl;
    return 0;
  }

  ResPool pool( getZYpp()->pool() );

  Source_Ref src( createSource( "dir:////Local/PATCHES" ) );
  getZYpp()->addResolvables( src.resolvables() );

  MIL << pool << endl;

  dumpRange( MIL, pool.byNameBegin("glibc"), pool.byNameEnd("glibc") ) << endl;

  Dln dnl;
  Sfp sfp;
  SRP srp;
  dnl.connect();
  sfp.connect();
  srp.connect();

  std::for_each( pool.byNameBegin("glibc"), pool.byNameEnd("glibc"), test );

  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::instance().logNothing();
  return 0;
}

