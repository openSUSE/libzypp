/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "CredentialManager.h"
#include <zypp-media/mediaconfig.h>
#include <zypp-media/ng/auth/credentialmanager.h>
#include <zypp/zypp_detail/ZYppImpl.h>

namespace zypp::media {

  CredManagerOptions::CredManagerOptions(const Pathname & rootdir)
  {
    this->globalCredFilePath = rootdir / zypp_detail::GlobalStateHelper::config().mediaConfig().credentialsGlobalFile();
    this->customCredFileDir  = rootdir / zypp_detail::GlobalStateHelper::config().mediaConfig().credentialsGlobalDir();
    char * homedir = getenv("HOME");
    if (homedir)
      userCredFilePath = rootdir / homedir / CredManagerSettings::userCredFile();
  }

  AuthData_Ptr CredentialManager::findIn(const CredentialManager::CredentialSet & set,
                             const Url & url,
                             url::ViewOption vopt)
  { return zyppng::media::CredentialManager::findIn( set, url, vopt ); }

  //////////////////////////////////////////////////////////////////////
  //
  // CLASS NAME : CredentialManager
  //
  //////////////////////////////////////////////////////////////////////

  CredentialManager::CredentialManager( zyppng::media::CredentialManagerRef pimpl )
    : _pimpl( std::move(pimpl) )
  {}

  CredentialManager::CredentialManager( CredManagerOptions opts )
    : _pimpl( zyppng::media::CredentialManager::create(opts) )
  {}

  AuthData_Ptr CredentialManager::getCred(const Url & url)
  { return _pimpl->getCred(url); }

  AuthData_Ptr CredentialManager::getCredFromFile(const Pathname & file)
  { return _pimpl->getCredFromFile(file); }

  void CredentialManager::addCred(const AuthData & cred)
  { return _pimpl->addCred (cred); }

  time_t CredentialManager::timestampForCredDatabase ( const zypp::Url &url )
  { return _pimpl->timestampForCredDatabase(url); }

  void CredentialManager::addGlobalCred(const AuthData & cred)
  { return _pimpl->addGlobalCred(cred); }

  void CredentialManager::addUserCred(const AuthData & cred)
  { return _pimpl->addUserCred ( cred ); }

  void CredentialManager::saveInGlobal(const AuthData & cred)
  { return _pimpl->saveInGlobal (cred); }

  void CredentialManager::saveInUser(const AuthData & cred)
  { return _pimpl->saveInUser(cred); }

  void CredentialManager::saveInFile(const AuthData & cred, const Pathname & credFile)
  { return _pimpl->saveInFile(cred, credFile); }

  void CredentialManager::clearAll(bool global)
  { return _pimpl->clearAll(global); }

  CredentialManager::CredentialIterator CredentialManager::credsGlobalBegin() const
  { return _pimpl->credsGlobalBegin(); }

  CredentialManager::CredentialIterator CredentialManager::credsGlobalEnd() const
  { return _pimpl->credsGlobalEnd(); }

  CredentialManager::CredentialSize CredentialManager::credsGlobalSize() const
  { return _pimpl->credsGlobalSize(); }

  bool CredentialManager::credsGlobalEmpty() const
  { return _pimpl->credsGlobalEmpty(); }

  CredentialManager::CredentialIterator CredentialManager::credsUserBegin() const
  { return _pimpl->credsUserBegin(); }

  CredentialManager::CredentialIterator CredentialManager::credsUserEnd() const
  { return _pimpl->credsUserEnd(); }

  CredentialManager::CredentialSize CredentialManager::credsUserSize() const
  { return _pimpl->credsUserSize(); }

  bool CredentialManager::credsUserEmpty() const
  { return _pimpl->credsUserEmpty(); }

}
