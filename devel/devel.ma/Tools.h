#ifndef Tools_h
#define Tools_h

#include <iostream>

#include "Printing.h"

#include <zypp/base/Counter.h>
#include <zypp/base/Measure.h>

#include <zypp/Date.h>
#include <zypp/ResObject.h>
#include <zypp/pool/PoolStats.h>

#include <zypp/ZYppFactory.h>
#include <zypp/ResPool.h>
#include <zypp/ResPoolProxy.h>

#include <zypp/SourceFactory.h>
#include <zypp/source/susetags/SuseTagsImpl.h>

using namespace zypp;
using zypp::debug::Measure;
using std::endl;

#define for_(IT,BEG,END) for ( typeof(BEG) IT = BEG; IT != END; ++IT )

///////////////////////////////////////////////////////////////////
//

template<class _Condition>
  struct SetTrue
  {
    SetTrue( _Condition cond_r )
    : _cond( cond_r )
    {}

    template<class _Tp>
      bool operator()( _Tp t ) const
      {
        _cond( t );
        return true;
      }

    _Condition _cond;
  };

template<class _Condition>
  inline SetTrue<_Condition> setTrue_c( _Condition cond_r )
  {
    return SetTrue<_Condition>( cond_r );
  }

struct PrintPoolItem
{
  void operator()( const PoolItem & pi ) const
  { USR << pi << " (" << pi.resolvable().get() << ")" <<endl; }
};

template <class _Iterator>
  std::ostream & vdumpPoolStats( std::ostream & str,
                                 _Iterator begin_r, _Iterator end_r )
  {
    pool::PoolStats stats;
    std::for_each( begin_r, end_r,

                   functor::chain( setTrue_c(PrintPoolItem()),
                                   setTrue_c(functor::functorRef<void,ResObject::constPtr>(stats)) )

                 );
    return str << stats;
  }

///////////////////////////////////////////////////////////////////
// rstats

typedef zypp::pool::PoolStats Rstats;

template<class _Iterator>
  void rstats( _Iterator begin, _Iterator end )
  {
    DBG << __PRETTY_FUNCTION__ << endl;
    Rstats stats;
    for_each( begin, end, functor::functorRef<void,ResObject::constPtr>(stats) );
    MIL << stats << endl;
  }

template<class _Container>
  void rstats( const _Container & c )
  {
    rstats( c.begin(), c.end() );
  }

///////////////////////////////////////////////////////////////////
inline Source_Ref createSource( const Url & url_r, const std::string & alias_r = std::string() )
{
  Source_Ref ret;
  Measure x( "createSource: " + url_r.asString() );
  try
    {
      std::string alias( alias_r.empty() ? Date::now().asSeconds() : alias_r );
      try
        {
          ret = SourceFactory().createFrom( url_r, "/", alias );
        }
      catch ( const source::SourceUnknownTypeException & )
        {
          ret = SourceFactory().createFrom( "Plaindir", url_r, "/", alias, "", false, true );
        }
    }
  catch ( const Exception & )
    {
      return Source_Ref::noSource;
    }
  x.start( "parseSource: " + url_r.asString() );
  {
    //zypp::base::LogControl::TmpLineWriter shutUp;
    ret.resolvables();
  }
  x.stop();
  MIL << "Content " << ret << "{" << endl;
  rstats( ret.resolvables() );
  MIL << "}" << endl;

  return ret;
}
inline Source_Ref createSource( const std::string & url_r, const std::string & alias_r = std::string() )
{
  try
    {
      return createSource( Url(url_r), alias_r );
    }
  catch ( const Exception & )
    {
      return Source_Ref::noSource;
    }
}

///////////////////////////////////////////////////////////////////

template<class _Res>
ui::Selectable::Ptr getSel( const std::string & name_r )
{
  ResPoolProxy uipool( getZYpp()->poolProxy() );
  for_(it, uipool.byKindBegin<_Res>(), uipool.byKindEnd<_Res>() )
  {
    if ( (*it)->name() == name_r )
      return (*it);
  }
  return 0;
}

template<class _Res>
PoolItem getPi( const std::string & name_r, const Edition & ed_r, const Arch & arch_r )
{
  PoolItem ret;
  ResPool pool( getZYpp()->pool() );
  for_(it, pool.byNameBegin(name_r), pool.byNameEnd(name_r) )
  {
    if ( !ret && isKind<_Res>( (*it).resolvable() )
         && ( ed_r == Edition() || ed_r == (*it)->edition() )
         && ( arch_r == Arch()  || arch_r == (*it)->arch()  ) )
    {
      ret = (*it);
      MIL << "    ->" << *it << endl;
    }
    else
    {
      DBG << "     ?" << *it << endl;
    }
  }
  return ret;
}
template<class _Res>
PoolItem getPi( const std::string & name_r )
{
  return getPi<_Res>( name_r, Edition(), Arch() );
}
template<class _Res>
PoolItem getPi( const std::string & name_r, const Edition & ed_r )
{
  return getPi<_Res>( name_r, ed_r, Arch() );
}
template<class _Res>
PoolItem getPi( const std::string & name_r, const Arch & arch_r )
{
  return getPi<_Res>( name_r, Edition(), arch_r );
}
///////////////////////////////////////////////////////////////////

#endif // Tools_h
