/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/Target.cc
 *
*/
#include <cassert>

#include <iostream>

#include <zypp/Target.h>
#include <zypp/target/TargetImpl.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  IMPL_PTR_TYPE(Target);

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Target::Target
  //	METHOD TYPE : Ctor
  //
  Target::Target( const Pathname & root, bool doRebuild_r )
  : _pimpl( new Impl(root,doRebuild_r) )
  {
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Target::Target
  //	METHOD TYPE : Ctor
  //
  Target::Target( const Impl_Ptr & impl_r )
  : _pimpl( impl_r )
  {
    assert( impl_r );
  }

  std::ostream & operator<<( std::ostream & str, const Target::DistributionLabel & obj )
  {
    str << "summary=" << obj.summary << endl;
    str << "shortName=" << obj.shortName << endl;
    return str;
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	Forward to TargetImpl:
  //
  ///////////////////////////////////////////////////////////////////

  void Target::buildCache()
  { _pimpl->buildCache(); }

  void Target::cleanCache()
  { _pimpl->clearCache(); }

  void Target::load()
  { _pimpl->load(); }

  void Target::reload()
  { _pimpl->reload(); }
  
  void Target::unload()
  { _pimpl->unload(); }

  target::rpm::RpmDb & Target::rpmDb()
  { return _pimpl->rpm(); }

  Pathname Target::root() const
  { return _pimpl->root(); }

  bool Target::providesFile (const std::string & name_str, const std::string & path_str) const
  { return _pimpl->providesFile (name_str, path_str); }

  std::string Target::whoOwnsFile (const std::string & path_str) const
  { return _pimpl->whoOwnsFile (path_str); }

  std::ostream & Target::dumpOn( std::ostream & str ) const
  { return _pimpl->dumpOn( str ); }

  Product::constPtr Target::baseProduct() const
  { return _pimpl->baseProduct(); }

  LocaleSet Target::requestedLocales() const
  { return _pimpl->requestedLocales(); }
  LocaleSet Target::requestedLocales( const Pathname & root_r )
  { return target::TargetImpl::requestedLocales( root_r ); }

  void Target::updateAutoInstalled()
  { return _pimpl->updateAutoInstalled(); }

  std::string Target::targetDistribution() const
  { return _pimpl->targetDistribution(); }
  std::string Target::targetDistribution( const Pathname & root_r )
  { return target::TargetImpl::targetDistribution( root_r ); }

  std::string Target::targetDistributionRelease() const
  { return _pimpl->targetDistributionRelease(); }
  std::string Target::targetDistributionRelease( const Pathname & root_r )
  { return target::TargetImpl::targetDistributionRelease( root_r ); }

  std::string Target::targetDistributionFlavor() const
  { return _pimpl->targetDistributionFlavor(); }
  std::string Target::targetDistributionFlavor( const Pathname & root_r )
  { return target::TargetImpl::targetDistributionFlavor( root_r ); }

  Target::DistributionLabel Target::distributionLabel() const
  { return _pimpl->distributionLabel(); }
  Target::DistributionLabel Target::distributionLabel( const Pathname & root_r )
  { return target::TargetImpl::distributionLabel( root_r ); }

  std::string Target::distributionVersion() const
  { return _pimpl->distributionVersion(); }
  std::string Target::distributionVersion( const Pathname & root_r )
  { return target::TargetImpl::distributionVersion( root_r ); }

  std::string Target::distributionFlavor() const
  { return _pimpl->distributionFlavor(); }
  std::string Target::distributionFlavor( const Pathname & root_r )
  { return target::TargetImpl::distributionFlavor( root_r ); }

  std::string Target::anonymousUniqueId() const
  { return _pimpl->anonymousUniqueId(); }
  std::string Target::anonymousUniqueId( const Pathname & root_r )
  { return target::TargetImpl::anonymousUniqueId( root_r ); }

  const VendorAttr & Target::vendorAttr() const
  { return _pimpl->vendorAttr(); }
  void Target::vendorAttr( VendorAttr vendorAttr_r )
  { _pimpl->vendorAttr( std::move(vendorAttr_r) ); }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
