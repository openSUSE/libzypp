/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include <iostream>
#include <fstream>

#include "credentialmanager.h"
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/base/Logger.h>

#include <zypp-media/mediaexception.h>
#include <zypp-media/auth/credentialfilereader.h>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>

namespace bpci = boost::interprocess;


namespace zyppng::media {

  CredentialManager::CredentialManager( ZYPP_PRIVATE_CONSTR_ARG, zypp::media::CredManagerSettings options )
    : _options(std::move(options))
    , _globalDirty(false)
    , _userDirty(false)
  {
    init_globalCredentials();
    init_userCredentials();
  }

  CredentialManager::~CredentialManager()
  { }


  void CredentialManager::init_globalCredentials()
  {
    if (_options.globalCredFilePath.empty())
      DBG << "global cred file not known";
    else if ( zypp::PathInfo(_options.globalCredFilePath).isExist())
    {
    /*  list<Pathname> entries;
      if (filesystem::readdir(entries, _options.globalCredFilePath, false) != 0)
        ZYPP_THROW(Exception("failed to read directory"));

      for_(it, entries.begin(), entries.end())*/

      zypp::media::CredentialFileReader(_options.globalCredFilePath,
          bind(&CredentialManager::processCredentials, this, std::placeholders::_1));
    }
    else
      DBG << "global cred file does not exist";

    _credsGlobal = _credsTmp; _credsTmp.clear();
    DBG << "Got " << _credsGlobal.size() << " global records." << std::endl;
  }


  void CredentialManager::init_userCredentials()
  {
    if (_options.userCredFilePath.empty())
      DBG << "user cred file not known";
    else if ( zypp::PathInfo(_options.userCredFilePath).isExist() )
    {
    /*  list<Pathname> entries;
      if (filesystem::readdir(entries, _options.userCredFilePath, false ) != 0)
        ZYPP_THROW(Exception("failed to read directory"));

      for_(it, entries.begin(), entries.end())*/
      zypp::media::CredentialFileReader(_options.userCredFilePath,
          bind(&CredentialManager::processCredentials, this, std::placeholders::_1));
    }
    else
      DBG << "user cred file does not exist" << std::endl;

    _credsUser = _credsTmp; _credsTmp.clear();
    DBG << "Got " << _credsUser.size() << " user records." << std::endl;
  }


  bool CredentialManager::processCredentials( zypp::media::AuthData_Ptr & cred )
  {
    _credsTmp.insert(cred);
    return true;
  }


  zypp::media::AuthData_Ptr CredentialManager::findIn(const CredentialManager::CredentialSet & set,
                             const zypp::Url & url,
                             zypp::url::ViewOption vopt)
  {
    const std::string & username = url.getUsername();
    for( CredentialManager::CredentialIterator it = set.begin(); it != set.end(); ++it )
    {
      if ( !(*it)->url().isValid() )
        continue;

      // this ignores url params - not sure if it is good or bad...
      if ( url.asString(vopt).find((*it)->url().asString(vopt)) == 0 )
      {
        if ( username.empty() || username == (*it)->username() )
          return *it;
      }
    }

    return zypp::media::AuthData_Ptr();
  }

  zypp::media::AuthData_Ptr CredentialManager::getCredFromUrl( const zypp::Url & url ) const
  {
    zypp::media::AuthData_Ptr result;

    // compare the urls via asString(), but ignore password
    // default url::ViewOption will take care of that.
    // operator==(Url,Url) compares the whole Url

    zypp::url::ViewOption vopt;
    vopt = vopt
      - zypp::url::ViewOption::WITH_USERNAME
      - zypp::url::ViewOption::WITH_PASSWORD
      - zypp::url::ViewOption::WITH_QUERY_STR;

    // search in global credentials
    result = findIn(_credsGlobal, url, vopt);

    // search in home credentials
    if (!result)
      result = findIn(_credsUser, url, vopt);

    if (result)
      DBG << "Found credentials for '" << url << "':" << std::endl << *result;
    else
      DBG << "No credentials for '" << url << "'" << std::endl;

    return result;
  }


  zypp::media::AuthData_Ptr CredentialManager::getCredFromFile(const zypp::Pathname & file)
  {
    zypp::media::AuthData_Ptr result;

    zypp::Pathname credfile;
    if (file.absolute())
      // get from that file
      credfile = file;
    else
      // get from /etc/zypp/credentials.d, delete the leading path
      credfile = _options.customCredFileDir / file.basename();

    zypp::PathInfo pi { credfile };
    if ( pi.userMayR() ) try {
      // make sure only our thread accesses the file
      bpci::file_lock lockFile ( credfile.c_str() );
      bpci::scoped_lock lock( lockFile );

      zypp::media::CredentialFileReader(credfile, bind(&CredentialManager::processCredentials, this, std::placeholders::_1));
    }
    catch ( ... ) {
      WAR << pi << " failed to lock file for reading." << std::endl;
    }

    if (_credsTmp.empty())
      WAR << pi << " does not contain valid credentials or is not readable." << std::endl;
    else
    {
      result = *_credsTmp.begin();
      _credsTmp.clear();
    }

    return result;
  }

  static int save_creds_in_file(
      CredentialManager::CredentialSet &creds,
      const zypp::Pathname & file,
      const mode_t mode)
  {
    int ret = 0;
    zypp::filesystem::assert_file_mode( file, mode );

    const auto now = time( nullptr );

    zypp::PathInfo pi { file };
    if ( pi.userMayRW() ) try {
      // make sure only our thread accesses the file
      bpci::file_lock lockFile ( file.c_str() );
      bpci::scoped_lock lock( lockFile );

      std::ofstream fs(file.c_str());
      for_(it, creds.begin(), creds.end())
      {
        (*it)->dumpAsIniOn(fs);
        (*it)->setLastDatabaseUpdate( now );
        fs << std::endl;
      }
      if ( !fs ) {
        WAR << pi << " failed to write credentials to file." << std::endl;
        ret = 1;
      }
      fs.close();
    }
    catch ( ... ) {
      WAR << pi << " failed to lock file for writing." << std::endl;
      ret = 1;
    }

    return ret;
  }

  void  CredentialManager::saveGlobalCredentials()
  {
    save_creds_in_file(_credsGlobal, _options.globalCredFilePath, 0640);
  }

  void  CredentialManager::saveUserCredentials()
  {
    save_creds_in_file( _credsUser, _options.userCredFilePath, 0600);
  }

  zypp::media::AuthData_Ptr CredentialManager::getCred(const zypp::Url & url)
  {
    std::string credfile = url.getQueryParam("credentials");
    if (credfile.empty())
      return getCredFromUrl(url);
    return getCredFromFile(credfile);
  }

  void CredentialManager::addCred(const zypp::media::AuthData & cred)
  {
    if ( !cred.url().isValid() )
      ZYPP_THROW( zypp::media::MediaInvalidCredentialsException( "URL must be valid in order to save AuthData." ) );

    zypp::Pathname credfile = cred.url().getQueryParam("credentials");
    if (credfile.empty())
      //! \todo ask user where to store these creds. saving to user creds for now
      addUserCred(cred);
    else
      saveInFile(cred, credfile);
  }

  time_t CredentialManager::timestampForCredDatabase ( const zypp::Url &url )
  {
    zypp::Pathname credfile;
    if ( url.isValid() ) {
      credfile = url.getQueryParam("credentials");
    }

    if (credfile.empty())
      credfile = _options.userCredFilePath;

    zypp::PathInfo pi(credfile);
    if ( pi.isExist() && pi.isFile() )
      return pi.mtime();

    return 0;
  }

  void CredentialManager::addGlobalCred(const zypp::media::AuthData & cred)
  {
    if ( !cred.url().isValid() )
      ZYPP_THROW( zypp::media::MediaInvalidCredentialsException( "URL must be valid in order to save AuthData." ) );

    zypp::media::AuthData_Ptr c_ptr;
    c_ptr.reset(new zypp::media::AuthData(cred)); // FIX for child classes if needed
    std::pair<CredentialIterator, bool> ret = _credsGlobal.insert(c_ptr);
    if (ret.second)
      _globalDirty = true;
    else if ((*ret.first)->password() != cred.password())
    {
      _credsGlobal.erase(ret.first);
      _credsGlobal.insert(c_ptr);
      _globalDirty = true;
    }
  }


  void CredentialManager::addUserCred(const zypp::media::AuthData & cred)
  {
    if ( !cred.url().isValid() )
      ZYPP_THROW( zypp::media::MediaInvalidCredentialsException( "URL must be valid in order to save AuthData." ) );

    zypp::media::AuthData_Ptr c_ptr;
    c_ptr.reset(new zypp::media::AuthData(cred)); // FIX for child classes if needed
    std::pair<CredentialIterator, bool> ret = _credsUser.insert(c_ptr);
    if (ret.second)
      _userDirty = true;
    else if ((*ret.first)->password() != cred.password())
    {
      _credsUser.erase(ret.first);
      _credsUser.insert(c_ptr);
      _userDirty = true;
    }
  }


  void CredentialManager::save()
  {
    if (_globalDirty)
      saveGlobalCredentials();
    if (_userDirty)
      saveUserCredentials();
    _globalDirty = false;
    _userDirty = false;
  }


  void CredentialManager::saveInGlobal(const zypp::media::AuthData & cred)
  {
    addGlobalCred(cred);
    save();
  }


  void CredentialManager::saveInUser(const zypp::media::AuthData & cred)
  {
    addUserCred(cred);
    save();
  }


  void CredentialManager::saveInFile(const zypp::media::AuthData & cred, const zypp::Pathname & credFile)
  {
    zypp::media::AuthData_Ptr c_ptr;
    c_ptr.reset(new zypp::media::AuthData(cred)); // FIX for child classes if needed
    c_ptr->setUrl( zypp::Url() ); // don't save url in custom creds file
    CredentialManager::CredentialSet creds;
    creds.insert(c_ptr);

    int ret = 0;
    if (credFile.absolute())
      ret = save_creds_in_file(creds, credFile, 0640);
    else
      ret = save_creds_in_file(
          creds, _options.customCredFileDir / credFile, 0600);

    if (!ret)
    {
      //! \todo figure out the reason(?), call back to user
      ERR << "error saving the credentials" << std::endl;
    }
  }


  void CredentialManager::clearAll(bool global)
  {
    if (global)
    {
      if (!zypp::filesystem::unlink(_options.globalCredFilePath))
        ERR << "could not delete user credentials file "
            << _options.globalCredFilePath << std::endl;
      _credsUser.clear();
    }
    else
    {
      if (!zypp::filesystem::unlink(_options.userCredFilePath))
        ERR << "could not delete global credentials file"
            << _options.userCredFilePath << std::endl;
      _credsGlobal.clear();
    }
  }

  CredentialManager::CredentialIterator CredentialManager::credsGlobalBegin() const
  { return _credsGlobal.begin(); }

  CredentialManager::CredentialIterator CredentialManager::credsGlobalEnd() const
  { return _credsGlobal.end(); }

  CredentialManager::CredentialSize CredentialManager::credsGlobalSize() const
  { return _credsGlobal.size(); }

  bool CredentialManager::credsGlobalEmpty() const
  { return _credsGlobal.empty(); }

  CredentialManager::CredentialIterator CredentialManager::credsUserBegin() const
  { return _credsUser.begin(); }

  CredentialManager::CredentialIterator CredentialManager::credsUserEnd() const
  { return _credsUser.end(); }

  CredentialManager::CredentialSize CredentialManager::credsUserSize() const
  { return _credsUser.size(); }

  bool CredentialManager::credsUserEmpty() const
  { return _credsUser.empty(); }

}
