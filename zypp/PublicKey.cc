/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/PublicKey.cc
 *
*/
#include <iostream>
#include <vector>

#include "zypp/base/Logger.h"
#include "zypp/base/Exception.h"
#include "zypp/base/String.h"

#include "zypp/ExternalProgram.h"
#include "zypp/PublicKey.h"
#include "zypp/TmpPath.h"
#include "zypp/PathInfo.h"
#include "zypp/Date.h"
#include "zypp/TmpPath.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : PublicKey::Impl
  //
  /** PublicKey implementation. */
  struct PublicKey::Impl
  {
    /** Data we extract from one key. */
    struct KeyData
    {
      std::string _id;
      std::string _name;
      std::string _fingerprint;
      Date        _created;
      Date        _expires;
    };

    Impl()
    {}

    Impl(const Pathname &file)
    {
      readFromFile(file);
      MIL << "Done reading key" << std::endl;
    }

    public:
      /** Offer default Impl. */
      static shared_ptr<Impl> nullimpl()
      {
        static shared_ptr<Impl> _nullimpl( new Impl );
        return _nullimpl;
      }

    std::string asString() const
    { return "[" + id() + "-" + str::hexstring(created(),8).substr(2) + "] [" + name() + "] [" + fingerprint() + "]"; }

    std::string id() const
    { return _keyData._id; }

    std::string name() const
    { return _keyData._name; }

    std::string fingerprint() const
    { return _keyData._fingerprint; }

    Date created() const
    { return _keyData._created; }

    Date expires() const
    { return _keyData._expires; }

    Pathname path() const
    { return _data_file.path(); }

    protected:

     void readFromFile( const Pathname &keyfile)
     {

       PathInfo info(keyfile);
       MIL << "Reading pubkey from " << keyfile << " of size " << info.size() << " and sha1 " << filesystem::checksum(keyfile, "sha1")<< endl;
       if ( !info.isExist() )
         ZYPP_THROW(Exception("Can't read public key from " + keyfile.asString() + ", file not found"));

       if ( copy( keyfile, _data_file.path() ) != 0 )
         ZYPP_THROW(Exception("Can't copy public key data from " + keyfile.asString() + " to " +  _data_file.path().asString() ));

        filesystem::TmpDir dir;
        const char* argv[] =
        {
          "gpg",
          "-v",
          "--no-default-keyring",
          "--fixed-list-mode",
          "--with-fingerprint",
          "--with-colons",
          "--homedir",
          dir.path().asString().c_str(),
          "--quiet",
          "--no-tty",
          "--no-greeting",
          "--batch",
          "--status-fd",
          "1",
          _data_file.path().asString().c_str(),
          NULL
        };

        ExternalProgram prog(argv,ExternalProgram::Discard_Stderr, false, -1, true);

        // pub:-:1024:17:A84EDAE89C800ACA:971961473:1214043198::-:SuSE Package Signing Key <build@suse.de>:
        // fpr:::::::::79C179B2E1C820C1890F9994A84EDAE89C800ACA:
        // sig:::17:A84EDAE89C800ACA:1087899198:::::[selfsig]::13x:
        // sig:::17:9E40E310000AABA4:980442706::::[User ID not found]:10x:
        // sig:::1:77B2E6003D25D3D9:980443247::::[User ID not found]:10x:
        // sub:-:2048:16:197448E88495160C:971961490:1214043258::: [expires: 2008-06-21]
        // sig:::17:A84EDAE89C800ACA:1087899258:::::[keybind]::18x:
	KeyData keyData;
	std::vector<std::string> words;
	enum { pNONE, pPUB, pSIG, pFPR, pUID } parseEntry;
	bool sawSig = false;
	bool sawSub = false;
        for ( std::string line = prog.receiveLine(); !line.empty(); line = prog.receiveLine() )
        {
          if ( line.empty() )
            continue;

	  if ( sawSub )
	  {
	    if ( line[0] == 'p' && line[1] == 'u' && line[2] == 'b' && line[3] == ':' )
	      sawSub = false;	// parse next key
	    else
	      continue;		// don't parse in subkeys
	  }

	  // quick check for interesting entries
	  parseEntry = pNONE;
	  switch ( line[0] )
	  {
#define DOTEST( C1, C2, C3, E ) case C1: if ( line[1] == C2 && line[2] == C3 && line[3] == ':' ) parseEntry = E; break
	    DOTEST( 'p', 'u', 'b', pPUB );
	    DOTEST( 's', 'i', 'g', pSIG );
	    DOTEST( 'f', 'p', 'r', pFPR );
	    DOTEST( 'u', 'i', 'd', pUID );
#undef DOTEST
	  }
	  if ( parseEntry == pNONE )
	  {
	    if ( line[0] == 's' && line[1] == 'u' && line[2] == 'b' && line[3] == ':' )
	      sawSub = true;	// don't parse in subkeys
	    continue;
	  }

          if ( line[line.size()-1] == '\n' )
            line.erase( line.size()-1 );

	  words.clear();
          str::splitFields( line, std::back_inserter(words), ":" );

	  switch ( parseEntry )
	  {
	    case pPUB:
	      keyData = KeyData();	// reset upon new key
	      sawSig  = false;
	      keyData._id      = words[4];
	      keyData._name    = words[9];
	      keyData._created = Date(str::strtonum<Date::ValueType>(words[5]));
	      keyData._expires = Date(str::strtonum<Date::ValueType>(words[6]));
	      break;

	    case pSIG:
	      if ( !sawSig && words[words.size()-2] == "13x"  )
	      {
		// update creation and expire dates from 1st signature type "13x"
		if ( ! words[5].empty() )
		  keyData._created = Date(str::strtonum<Date::ValueType>(words[5]));
		if ( ! words[6].empty() )
		  keyData._expires = Date(str::strtonum<Date::ValueType>(words[6]));
		sawSig = true;
	      }
	      break;

	    case pFPR:
	      if ( ! words[9].empty() )
		keyData._fingerprint = words[9];
	      break;

	    case pUID:
	      if ( ! words[9].empty() )
		keyData._name = words[9];
	      break;

	    case pNONE:
	      break;
	  }
        }
        prog.close();

        if ( keyData._id.empty() )
	  ZYPP_THROW( BadKeyException( "File " + _data_file.path().asString() + " doesn't contain public key data" , _data_file.path() ) );

        //replace all escaped semicolon with real ':'
        str::replace_all( keyData._name, "\\x3a", ":" );

	_keyData = keyData;
     }

  private:
    filesystem::TmpFile	_data_file;
    KeyData		_keyData;

  private:
    friend Impl * rwcowClone<Impl>( const Impl * rhs );
    /** clone for RWCOW_pointer */
    Impl * clone() const
    { return new Impl( *this ); }
  };
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : PublicKey::PublicKey
  //	METHOD TYPE : Ctor
  //
  PublicKey::PublicKey()
  : _pimpl( Impl::nullimpl() )
  {}

  PublicKey::PublicKey( const Pathname &file )
  : _pimpl( new Impl(file) )
  { MIL << *this << endl; }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : PublicKey::~PublicKey
  //	METHOD TYPE : Dtor
  //
  PublicKey::~PublicKey()
  {}

  ///////////////////////////////////////////////////////////////////
  //
  // Forward to implementation:
  //
  ///////////////////////////////////////////////////////////////////

  std::string PublicKey::asString() const
  { return _pimpl->asString(); }

  std::string PublicKey::armoredData() const
  { return std::string(); }

  std::string PublicKey::id() const
  { return _pimpl->id(); }

  std::string PublicKey::name() const
  { return _pimpl->name(); }

  std::string PublicKey::fingerprint() const
  { return _pimpl->fingerprint(); }

  Date PublicKey::created() const
  { return _pimpl->created(); }

  Date PublicKey::expires() const
  { return _pimpl->expires(); }

  Pathname PublicKey::path() const
  { return _pimpl->path(); }

  bool PublicKey::operator==( PublicKey b ) const
  {
     return(   b.id() == id()
            && b.fingerprint() == fingerprint()
            && b.created() == created() );
  }

  bool PublicKey::operator==( std::string sid ) const
  { return sid == id(); }

  std::ostream & dumpOn( std::ostream & str, const PublicKey & obj )
  {
    str << "[" << obj.name() << "]" << endl;
    str << "  fpr " << obj.fingerprint() << endl;
    str << "   id " << obj.id() << endl;
    str << "  cre " << obj.created() << endl;
    str << "  exp " << obj.expires() << endl;
    str << "]";
    return str;
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
