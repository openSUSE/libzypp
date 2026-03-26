/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/**
 * \file	zypp/ng/sat/lookupattr.cc
 */
#include <iostream>
#include <sstream>
#include <utility>

#include <zypp-core/base/LogTools.h>
#include <zypp-core/base/String.h>

#include <zypp/ng/sat/pool.h>
#include <zypp/ng/idstringtype.h>
#include <zypp/ng/sat/lookupattr.h>
#include <zypp/base/StrMatcher.h>
#include <zypp/ng/sat/capability.h>

#include <zypp-core/CheckSum.h>

using std::endl;

namespace zyppng::sat
{
    using detail::noSolvableId;

    /**
     * LookupAttr implementation.
     *
     * Repository and Solvable must not be set at the same time!
     *
     * \note When looking in pool or repo, \ref Solvable \c _solv is
     * somewhat abused to store either \c Id \c 0 or \c SOLVID_META, which
     * indicates whether the dataiterator should look into solvable or
     * repository metadata. Remember that all \ref Solvables with an
     * \e invalid \c Id, are treated as <tt>== Solvable::noSolvable</tt>,
     * and in a boolean context evaluate to \c false. Thus \c noSolvable
     * may have different \c Ids.
     */
    class LookupAttr::Impl
    {
      public:
        Impl()
        : _pool( nullptr )
        , _parent( SolvAttr::noAttr )
        {}
        Impl( detail::CPool * pool_r, const SolvAttr& attr_r, Location loc_r )
        : _pool( pool_r ), _attr( attr_r ), _parent( attr_r.parent() ), _solv( loc_r == REPO_ATTR ? SOLVID_META : noSolvableId )
        {}
        Impl( detail::CPool * pool_r, const SolvAttr& attr_r, Repository repo_r, Location loc_r )
        : _pool( pool_r ), _attr( attr_r ), _parent( attr_r.parent() ), _repo( repo_r ), _solv( loc_r == REPO_ATTR ? SOLVID_META : noSolvableId )
        {}
        Impl( detail::CPool * pool_r, const SolvAttr& attr_r, Solvable solv_r )
        : _pool( pool_r ), _attr( attr_r ), _parent( attr_r.parent() ), _solv( solv_r )
        {}

      public:
        SolvAttr attr() const
        { return _attr; }

        void setAttr( SolvAttr attr_r )
        {
          _attr = std::move(attr_r);
          SolvAttr p( _attr.parent() );
          if ( p != SolvAttr::noAttr )
            _parent = p;
        }

        const StrMatcher & strMatcher() const
        { return _strMatcher; }

        void setStrMatcher( const StrMatcher & matcher_r )
        {
          matcher_r.compile();
          _strMatcher = matcher_r;
        }

      public:
        bool pool() const
        { return ! (_repo || _solv); }

        void setPool( Location loc_r )
        {
          _repo = Repository::noRepository;
          _solv = Solvable( loc_r == REPO_ATTR ? SOLVID_META : noSolvableId );
        }

        Repository repo() const
        { return _repo; }

        void setRepo( Repository repo_r, Location loc_r  )
        {
          _repo = repo_r;
          _solv = Solvable( loc_r == REPO_ATTR ? SOLVID_META : noSolvableId );
          // Extract pool from repo
          if ( _repo )
            _pool = _repo.pool().get();
        }

        Solvable solvable() const
        { return _solv; }

        void setSolvable( Solvable solv_r )
        {
          _repo = Repository::noRepository;
          _solv = solv_r;
          // Extract pool from solvable
          if ( _solv )
            _pool = _solv.pool().get();
        }

        SolvAttr parent() const
        { return _parent; }

        void setParent( SolvAttr attr_r )
        { _parent = std::move(attr_r); }

      public:
        LookupAttr::iterator begin() const
        {
          if ( _attr == SolvAttr::noAttr || ! _pool || ! _pool->nrepos )
            return end();

          detail::RepoIdType whichRepo = detail::noRepoId; // all repos
          if ( _solv )
            whichRepo = _solv.repository();
          else if ( _repo )
            whichRepo = _repo.id();

          detail::DIWrap dip( _pool, whichRepo, _solv.id(), _attr.id(), _strMatcher.searchstring(), _strMatcher.flags().get() );
          if ( _parent != SolvAttr::noAttr )
            ::dataiterator_prepend_keyname( dip.get(), _parent.id() );

          return iterator( dip ); // iterator takes over ownership!
        }

        LookupAttr::iterator end() const
        { return iterator(); }

      private:
        detail::CPool * _pool;
        SolvAttr   _attr;
        SolvAttr   _parent;
        Repository _repo;
        Solvable   _solv;
        StrMatcher _strMatcher;

      private:
        friend Impl * zypp::rwcowClone<Impl>( const Impl * rhs );
        /** clone for RWCOW_pointer */
        Impl * clone() const
        { return new Impl( *this ); }
    };

    /**
     *	CLASS NAME : LookupAttr
     */

    LookupAttr::LookupAttr()
      : _pimpl( new Impl )
    {}

    LookupAttr::LookupAttr( Pool & pool, SolvAttr attr_r, Location loc_r )
      : _pimpl( new Impl( pool.get(), std::move(attr_r), std::move(loc_r) ) )
    {}

    LookupAttr::LookupAttr( Pool & pool, SolvAttr attr_r, SolvAttr parent_r, Location loc_r )
      : _pimpl( new Impl( pool.get(), std::move(attr_r), std::move(loc_r) ) )
    { _pimpl->setParent( std::move(parent_r) ); }

    LookupAttr::LookupAttr( SolvAttr attr_r, Repository repo_r, Location loc_r )
    {
      detail::CPool * cp = repo_r.pool().get();
      _pimpl.reset( new Impl( cp, std::move(attr_r), std::move(repo_r), std::move(loc_r) ) );
    }

    LookupAttr::LookupAttr( SolvAttr attr_r, SolvAttr parent_r, Repository repo_r, Location loc_r )
    {
      detail::CPool * cp = repo_r.pool().get();
      _pimpl.reset( new Impl( cp, std::move(attr_r), std::move(repo_r), std::move(loc_r) ) );
      _pimpl->setParent( std::move(parent_r) );
    }

    LookupAttr::LookupAttr( SolvAttr attr_r, Solvable solv_r )
    {
      detail::CPool * cp = solv_r.pool().get();
      _pimpl.reset( new Impl( cp, std::move(attr_r), std::move(solv_r) ) );
    }

    LookupAttr::LookupAttr( SolvAttr attr_r, SolvAttr parent_r, Solvable solv_r )
    {
      detail::CPool * cp = solv_r.pool().get();
      _pimpl.reset( new Impl( cp, std::move(attr_r), std::move(solv_r) ) );
      _pimpl->setParent( std::move(parent_r) );
    }


    SolvAttr LookupAttr::attr() const
    { return _pimpl->attr(); }

    void LookupAttr::setAttr( SolvAttr attr_r )
    { _pimpl->setAttr( std::move(attr_r) ); }

    const StrMatcher & LookupAttr::strMatcher() const
    { return _pimpl->strMatcher(); }

    void LookupAttr::setStrMatcher( const StrMatcher & matcher_r )
    { _pimpl->setStrMatcher( matcher_r ); }

    bool LookupAttr::pool() const
    { return _pimpl->pool(); }

    void LookupAttr::setPool(Location loc_r )
    { _pimpl->setPool( loc_r ); }

    Repository LookupAttr::repo() const
    { return _pimpl->repo(); }

    void LookupAttr::setRepo( Repository repo_r, Location loc_r )
    { _pimpl->setRepo( repo_r, loc_r ); }

    Solvable LookupAttr::solvable() const
    { return _pimpl->solvable(); }

    void LookupAttr::setSolvable( Solvable solv_r )
    { _pimpl->setSolvable( solv_r ); }

    SolvAttr LookupAttr::parent() const
    { return _pimpl->parent(); }

    void LookupAttr::setParent( SolvAttr attr_r )
    { _pimpl->setParent( std::move(attr_r) ); }

    LookupAttr::iterator LookupAttr::begin() const
    { return _pimpl->begin(); }

    LookupAttr::iterator LookupAttr::end() const
    { return _pimpl->end(); }

    bool LookupAttr::empty() const
    { return begin() == end(); }

    LookupAttr::size_type LookupAttr::size() const
    {
      size_type c = 0;
      for ( auto it = begin(); it != end(); ++it )
        ++c;
      return c;
    }

    /**
     *	CLASS NAME : LookupRepoAttr
     */

    LookupRepoAttr::LookupRepoAttr( SolvAttr attr_r, Repository repo_r )
      : LookupAttr( std::move(attr_r), repo_r, REPO_ATTR )
    {}

    void LookupRepoAttr::setRepo( Repository repo_r )
    { LookupAttr::setRepo( repo_r, REPO_ATTR ); }

    /**
     *  CLASS NAME : detail::DIWrap
     */

    namespace detail
    {
      DIWrap::DIWrap( detail::CPool * pool, RepoIdType repoId_r, SolvableIdType solvId_r, IdType attrId_r,
                      std::string  mstring_r, int flags_r )
      : _dip( new ::Dataiterator )
      , _pool( pool )
      , _mstring(std::move( mstring_r ))
      {
        ::dataiterator_init( _dip, _pool, repoId_r, solvId_r, attrId_r,
                             _mstring.empty() ? 0 : _mstring.c_str(), flags_r );
      }

      DIWrap::DIWrap( detail::CPool * pool, RepoIdType repoId_r, SolvableIdType solvId_r, IdType attrId_r,
                      const char * mstring_r, int flags_r )
      : _dip( new ::Dataiterator )
      , _pool( pool )
      , _mstring( mstring_r ? mstring_r : "" )
      {
        ::dataiterator_init( _dip, _pool, repoId_r, solvId_r, attrId_r,
                             _mstring.empty() ? 0 : _mstring.c_str(), flags_r );
      }

      DIWrap::DIWrap( const DIWrap & rhs )
        : _dip( 0 )
        , _pool( rhs._pool )
        , _mstring( rhs._mstring )
      {
        if ( rhs._dip )
        {
          _dip = new ::Dataiterator;
          ::dataiterator_init_clone( _dip, rhs._dip );
          ::dataiterator_strdup( _dip );
        }
      }

      DIWrap::~DIWrap()
      {
        if ( _dip )
        {
          ::dataiterator_free( _dip );
          delete _dip;
        }
      }
    }

    /**
     *	CLASS NAME : LookupAttr::iterator
     */

    Repository LookupAttr::iterator::inRepo() const
    { return _dip ? Repository( _dip->repo ) : Repository::noRepository; }

    Solvable LookupAttr::iterator::inSolvable() const
    { return _dip ? Solvable( _dip->solvid ) : Solvable::noSolvable; }

    SolvAttr LookupAttr::iterator::inSolvAttr() const
    { return _dip ? SolvAttr( _dip->key->name ) : SolvAttr::noAttr; }

    void LookupAttr::iterator::nextSkipSolvAttr()
    { if ( _dip ) ::dataiterator_skip_attribute( _dip.get() ); }

    void LookupAttr::iterator::nextSkipSolvable()
    { if ( _dip ) ::dataiterator_skip_solvable( _dip.get() ); }

    void LookupAttr::iterator::nextSkipRepo()
    { if ( _dip ) ::dataiterator_skip_repo( _dip.get() ); }

    void LookupAttr::iterator::stayInThisSolvable()
    { if ( _dip ) { _dip.get()->repoid = -1; _dip.get()->flags |= SEARCH_THISSOLVID; } }

    void LookupAttr::iterator::stayInThisRepo()
    { if ( _dip ) { _dip.get()->repoid = -1; } }

    detail::IdType LookupAttr::iterator::solvAttrType() const
    { return _dip ? _dip->key->type : detail::noId; }

    bool LookupAttr::iterator::solvAttrNumeric() const
    {
      switch ( solvAttrType() )
      {
        case REPOKEY_TYPE_NUM:
        case REPOKEY_TYPE_CONSTANT:
          return true;
          break;
      }
      return false;
    }

    bool LookupAttr::iterator::solvAttrString() const
    {
      switch ( solvAttrType() )
      {
        case REPOKEY_TYPE_ID:
        case REPOKEY_TYPE_IDARRAY:
        case REPOKEY_TYPE_CONSTANTID:
        case REPOKEY_TYPE_STR:
        case REPOKEY_TYPE_DIRSTRARRAY:
          return true;
          break;
      }
      return false;
    }

    bool LookupAttr::iterator::solvAttrIdString() const
    {
      switch ( solvAttrType() )
      {
        case REPOKEY_TYPE_ID:
        case REPOKEY_TYPE_IDARRAY:
        case REPOKEY_TYPE_CONSTANTID:
          return true;
          break;
      }
      return false;
    }

    bool LookupAttr::iterator::solvAttrCheckSum() const
    {
      switch ( solvAttrType() )
      {
        case REPOKEY_TYPE_MD5:
        case REPOKEY_TYPE_SHA1:
        case REPOKEY_TYPE_SHA256:
          return true;
          break;
      }
      return false;
    }

    namespace
    {
      enum SubType { ST_NONE,	// no sub-structure
                     ST_FLEX,	// flexarray
                     ST_SUB };	// inside sub-structure
      SubType subType( const detail::DIWrap & dip )
      {
        if ( ! dip )
          return ST_NONE;
        if ( dip.get()->key->type == REPOKEY_TYPE_FLEXARRAY )
          return ST_FLEX;
        return dip.get()->kv.parent ? ST_SUB : ST_NONE;
      }
    }

    bool LookupAttr::iterator::solvAttrSubEntry() const
    { return subType( _dip ) != ST_NONE; }

    bool LookupAttr::iterator::subEmpty() const
    { return( subBegin() == subEnd() ); }

    LookupAttr::size_type LookupAttr::iterator::subSize() const
    {
      size_type c = 0;
      for ( auto it = subBegin(); it != subEnd(); ++it )
        ++c;
      return c;
    }

    LookupAttr::iterator LookupAttr::iterator::subBegin() const
    {
      SubType subtype( subType( _dip ) );
      if ( subtype == ST_NONE )
        return subEnd();
      // setup the new sub iterator with the remembered position
      detail::DIWrap dip( _dip.pool(), 0, 0, 0 );
      ::dataiterator_clonepos( dip.get(), _dip.get() );
      switch ( subtype )
      {
        case ST_NONE:	// not reached
          break;
        case ST_FLEX:
          ::dataiterator_seek( dip.get(), DI_SEEK_CHILD|DI_SEEK_STAY );
          break;
        case ST_SUB:
          ::dataiterator_seek( dip.get(), DI_SEEK_REWIND|DI_SEEK_STAY );
          break;
      }
      return iterator( dip ); // iterator takes over ownership!
    }

    LookupAttr::iterator LookupAttr::iterator::subEnd() const
    {
      return iterator();
    }

    LookupAttr::iterator LookupAttr::iterator::subFind( const SolvAttr& attr_r ) const
    {
      iterator it = subBegin();
      if ( attr_r != SolvAttr::allAttr )
      {
        while ( it != subEnd() && it.inSolvAttr() != attr_r )
          ++it;
      }
      return it;
    }

    LookupAttr::iterator LookupAttr::iterator::subFind( const zypp::C_Str & attrname_r ) const
    {
      if ( attrname_r.empty() )
        return subBegin();

      SubType subtype( subType( _dip ) );
      if ( subtype == ST_NONE )
        return subBegin();

      std::string subattr( inSolvAttr().asString() );
      if ( subtype == ST_FLEX )
      {
        // append ":attrname"
        subattr += ":";
        subattr += attrname_r;
      }
      else
      {
        // replace "oldname" after ':' with "attrname"
        std::string::size_type pos( subattr.rfind( ':' ) );
        if ( pos != std::string::npos )
        {
          subattr.erase( pos+1 );
          subattr += attrname_r;
        }
        else
          subattr = attrname_r; // no ':' so replace all.
      }
      return subFind( SolvAttr( subattr ) );
    }

    int LookupAttr::iterator::asInt() const
    {
      if ( _dip )
      {
        switch ( solvAttrType() )
        {
          case REPOKEY_TYPE_NUM:
          case REPOKEY_TYPE_CONSTANT:
            return _dip->kv.num;
            break;
        }
      }
      return 0;
    }

    unsigned LookupAttr::iterator::asUnsigned() const
    { return asInt(); }

    unsigned long long LookupAttr::iterator::asUnsignedLL() const
    {
      if ( _dip )
      {
        switch ( solvAttrType() )
        {
          case REPOKEY_TYPE_NUM:
          case REPOKEY_TYPE_CONSTANT:
            return SOLV_KV_NUM64(&_dip->kv);
            break;
        }
      }
      return 0;
    }

    bool LookupAttr::iterator::asBool() const
    { return asInt(); }


    const char * LookupAttr::iterator::c_str() const
    {
      if ( _dip )
      {
        switch ( solvAttrType() )
        {
          case REPOKEY_TYPE_ID:
          case REPOKEY_TYPE_IDARRAY:
          case REPOKEY_TYPE_CONSTANTID:
            if ( _dip->data && _dip->data->localpool )
              return ::stringpool_id2str( &_dip->data->spool, _dip->kv.id ); // in local pool
            else
              return IdString( _dip->kv.id ).c_str(); // in global pool
            break;

          case REPOKEY_TYPE_STR:
            return _dip->kv.str;
            break;

          case REPOKEY_TYPE_DIRSTRARRAY:
            // may or may not be stringified depending on SEARCH_FILES flag
            return( _dip->flags & SEARCH_FILES
                    ? _dip->kv.str
                    : ::repodata_dir2str( _dip->data, _dip->kv.id, _dip->kv.str ) );
            break;
        }
      }
      return 0;
    }

    std::string LookupAttr::iterator::asString() const
    {
      if ( _dip )
      {
        switch ( solvAttrType() )
        {
          case REPOKEY_TYPE_ID:
          case REPOKEY_TYPE_IDARRAY:
          case REPOKEY_TYPE_CONSTANTID:
            {
              detail::IdType id = ::repodata_globalize_id( _dip->data, _dip->kv.id, 1 );
              return ISRELDEP(id) ? Capability( id ).asString()
                                  : IdString( id ).asString();
            }
            break;

          case REPOKEY_TYPE_STR:
          case REPOKEY_TYPE_DIRSTRARRAY:
            {
              const char * ret( c_str() );
              return ret ? ret : "";
            }
            break;

          case REPOKEY_TYPE_NUM:
          case REPOKEY_TYPE_CONSTANT:
            return zypp::str::numstring( asInt() );
            break;

          case REPOKEY_TYPE_MD5:
          case REPOKEY_TYPE_SHA1:
          case REPOKEY_TYPE_SHA256:
            {
              return asCheckSum().asString();
            }
            break;

          case REPOKEY_TYPE_FLEXARRAY:
            {
              std::ostringstream str;
              str << "{" << endl;
              for ( auto it = subBegin(); it != subEnd(); ++it )
              {
                str << "  " << it.inSolvAttr() << " = " << it.asString() << endl;
              }
              str << "}";
             return str.str();
            }
            break;
        }
      }
      return std::string();
    }

    IdString LookupAttr::iterator::idStr() const
    {
      if ( _dip )
      {
        switch ( solvAttrType() )
        {
          case REPOKEY_TYPE_ID:
          case REPOKEY_TYPE_IDARRAY:
          case REPOKEY_TYPE_CONSTANTID:
            return IdString( ::repodata_globalize_id( _dip->data, _dip->kv.id, 1 ) );
            break;
        }
      }
      return IdString();
    }

    CheckSum LookupAttr::iterator::asCheckSum() const
    {
      if ( _dip )
      {
        switch ( solvAttrType() )
        {
          case REPOKEY_TYPE_MD5:
            return CheckSum::md5( ::repodata_chk2str( _dip->data, solvAttrType(), (unsigned char *)_dip->kv.str ) );
            break;

          case REPOKEY_TYPE_SHA1:
            return CheckSum::sha1( ::repodata_chk2str( _dip->data, solvAttrType(), (unsigned char *)_dip->kv.str ) );
            break;

          case REPOKEY_TYPE_SHA224:
            return CheckSum::sha224( ::repodata_chk2str( _dip->data, solvAttrType(), (unsigned char *)_dip->kv.str ) );
            break;

          case REPOKEY_TYPE_SHA256:
            return CheckSum::sha256( ::repodata_chk2str( _dip->data, solvAttrType(), (unsigned char *)_dip->kv.str ) );
            break;

          case REPOKEY_TYPE_SHA384:
            return CheckSum::sha384( ::repodata_chk2str( _dip->data, solvAttrType(), (unsigned char *)_dip->kv.str ) );
            break;

          case REPOKEY_TYPE_SHA512:
            return CheckSum::sha512( ::repodata_chk2str( _dip->data, solvAttrType(), (unsigned char *)_dip->kv.str ) );
            break;
        }
      }
      return CheckSum();
    }

    LookupAttr::iterator::iterator()
    {}

    LookupAttr::iterator::iterator( const iterator & rhs )
    : _dip( rhs._dip )
    {}

    LookupAttr::iterator::iterator( detail::DIWrap & dip_r )
    {
      _dip.swap( dip_r ); // take ownership!
      increment();
    }

    LookupAttr::iterator::~iterator()
    {}

    LookupAttr::iterator & LookupAttr::iterator::operator=( const iterator & rhs )
    {
      if ( &rhs != this )
      {
        _dip = rhs._dip;
      }
      return *this;
    }

    bool LookupAttr::iterator::dip_equal( const detail::CDataiterator & lhs, const detail::CDataiterator & rhs ) const
    {
      // Iterator equal is same position in same container.
      // Here: same attribute in same solvable.
      return( lhs.solvid == rhs.solvid && lhs.key->name == rhs.key->name );
    }

    detail::IdType LookupAttr::iterator::operator*() const
    {
      return _dip ? ::repodata_globalize_id( _dip->data, _dip->kv.id, 1 )
                  : detail::noId;
    }

    LookupAttr::iterator& LookupAttr::iterator::operator++()
    {
      increment();
      return *this;
    }

    LookupAttr::iterator LookupAttr::iterator::operator++(int)
    {
      iterator tmp = *this;
      increment();
      return tmp;
    }

    bool LookupAttr::iterator::operator==( const iterator & rhs ) const
    {
      return ( bool(_dip) == bool(rhs._dip) )
          && ( ! _dip || dip_equal( *_dip.get(), *rhs._dip.get() ) );
    }

    void LookupAttr::iterator::increment()
    {
      if ( _dip )
      {
        if ( ! ::dataiterator_step( _dip.get() ) )
        {
          _dip.reset();
        }
        else
        {
          ::dataiterator_strdup( _dip.get() );
        }
      }
    }

    template<> CheckSum LookupAttr::iterator::asType<CheckSum>() const
    { return asCheckSum(); }

} // namespace zyppng::sat
