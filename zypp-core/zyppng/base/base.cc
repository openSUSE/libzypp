#include <zypp-core/zyppng/base/private/base_p.h>

namespace zyppng {

  BasePrivate::~BasePrivate()
  { }

  void BasePrivate::init()
  { }

  ZYPP_IMPL_PRIVATE(Base)

  Base::Base ( BasePrivate &dd )
    : d_ptr ( &dd )
  {
    d_ptr->z_ptr = this;
    d_ptr->init();
  }

  Base::Base() : Base( *new BasePrivate(*this) )
  { }

  Base::~Base()
  { }

  Base::WeakPtr Base::parent() const
  {
    return d_func()->parent;
  }

  void Base::addChild( const Base::Ptr& child )
  {
    Z_D();
    if ( !child )
      return;

    //we are already the parent
    auto childParent = child->d_func()->parent.lock();
    if ( childParent.get() == this )
      return;

    if ( childParent ) {
      childParent->removeChild( child );
    }

    d->children.insert( child );

    auto tracker = this->weak_this<Base>();
    child->d_func()->parent = tracker;
  }

  void Base::removeChild( const Base::Ptr& child )
  {
    if ( !child )
      return;

    //we are not the child of this object
    if ( child->d_func()->parent.lock().get() != this )
      return;

    Z_D();
    d->children.erase( child );
    child->d_func()->parent.reset();
  }

  const std::unordered_set<Base::Ptr> &Base::children() const
  {
    return d_func()->children;
  }

  std::thread::id Base::threadId() const
  {
    return d_func()->threadId;
  }

} // namespace zyppng
