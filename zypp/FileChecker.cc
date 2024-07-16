/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/FileChecker.cc
 *
*/
#include <iostream>
#include <utility>
#include <zypp/zypp_detail/ZYppImpl.h>
#include <zypp/base/Logger.h>
#include <zypp/FileChecker.h>
#include <zypp/ZYppFactory.h>
#include <zypp/Digest.h>
#include <zypp/KeyRing.h>
#include <zypp/ng/Context>
#include <zypp/ng/workflows/keyringwf.h>
#include <zypp/ng/workflows/checksumwf.h>
#include <zypp/ng/workflows/signaturecheckwf.h>

using std::endl;

#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "FileChecker"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ChecksumFileChecker::ChecksumFileChecker( CheckSum checksum )
    : _checksum(std::move(checksum))
  {}

  void ChecksumFileChecker::operator()( const Pathname &file ) const
  {
    const auto &res = zyppng::CheckSumWorkflow::verifyChecksum ( zypp_detail::GlobalStateHelper::context(), _checksum, file );
    if ( !res ) {
      std::rethrow_exception( res.error ( ) );
    }
  }

  void NullFileChecker::operator()(const Pathname &file ) const
  {
    MIL << "+ null check on " << file << endl;
    return;
  }

  void CompositeFileChecker::operator()(const Pathname &file ) const
  {
    //MIL << _checkers.size() << " checkers" << endl;
    for ( std::list<FileChecker>::const_iterator it = _checkers.begin(); it != _checkers.end(); ++it )
    {
      if ( *it )
      {
        //MIL << "+ chk" << endl;
        (*it)(file);
      }
      else
      {
        ERR << "Invalid checker" << endl;
      }
    }
  }

  void CompositeFileChecker::add( const FileChecker &checker )
  { _checkers.push_back(checker); }


  SignatureFileChecker::SignatureFileChecker()
  {}

  SignatureFileChecker::SignatureFileChecker( Pathname signature_r )
  { _verifyContext.signature( std::move(signature_r) ); }

  void SignatureFileChecker::addPublicKey( const Pathname & publickey_r )
  { addPublicKey( PublicKey(publickey_r) ); }

  void SignatureFileChecker::addPublicKey( const PublicKey & publickey_r )
  { getZYpp()->keyRing()->importKey( publickey_r, false ); }

  void SignatureFileChecker::operator()( const Pathname & file_r ) const
  {
    // const_cast because the workflow is allowed to store result values here
    SignatureFileChecker & self { const_cast<SignatureFileChecker&>(*this) };
    self._verifyContext.file( file_r );

    auto res = zyppng::SignatureFileCheckWorkflow::verifySignature ( zypp_detail::GlobalStateHelper::context(), keyring::VerifyFileContext(_verifyContext) );
    if ( !res ) {
      std::rethrow_exception( res.error ( ) );
    }
    self._verifyContext = std::move( *res );
  }

  keyring::VerifyFileContext &SignatureFileChecker::verifyContext()
  {
    return _verifyContext;
  }

  const keyring::VerifyFileContext &SignatureFileChecker::verifyContext() const
  {
    return _verifyContext;
  }

  /******************************************************************
  **
  **	FUNCTION NAME : operator<<
  **	FUNCTION TYPE : std::ostream &
  */
  std::ostream & operator<<( std::ostream & str, const FileChecker & obj )
  {
    return str;
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
