/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/Application.h
 */
#ifndef ZYPP_APPLICATION_H
#define ZYPP_APPLICATION_H

#include <iosfwd>

#include <zypp/ResObject.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{
  DEFINE_PTR_TYPE(Application);

  ///////////////////////////////////////////////////////////////////
  /// \class Application
  /// \brief Class representing an application (appdata.xml)
  ///////////////////////////////////////////////////////////////////
  class ZYPP_API Application : public ResObject
  {
  public:
    using Self = Application;
    using TraitsType = ResTraits<Self>;
    using Ptr = TraitsType::PtrType;
    using constPtr = TraitsType::constPtrType;

  public:
    // no attributes defined by now

  protected:
    friend Ptr make<Self>( const sat::Solvable & solvable_r );
    /** Ctor */
    Application( const sat::Solvable & solvable_r );
    /** Dtor */
    ~Application() override;
  };

} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_APPLICATION_H
