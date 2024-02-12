/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/target/rpm/RpmException.h
 *
*/
#ifndef ZYPP_TARGET_RPM_RPMEXCEPTION_H
#define ZYPP_TARGET_RPM_RPMEXCEPTION_H

#include <iosfwd>

#include <string>
#include <utility>

#include <zypp/base/Exception.h>
#include <zypp/Pathname.h>
#include <zypp/Url.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
namespace target
{
///////////////////////////////////////////////////////////////
namespace rpm
{
///////////////////////////////////////////////////////////////
//
//        CLASS NAME : RpmException
/** Just inherits Exception to separate media exceptions
 *
 **/
class RpmException : public Exception
{
public:
  /** Ctor taking message.
   * Use \ref ZYPP_THROW to throw exceptions.
  */
  RpmException()
      : Exception( "Rpm Exception" )
  {}
  /** Ctor taking message.
   * Use \ref ZYPP_THROW to throw exceptions.
  */
  RpmException( const std::string & msg_r )
      : Exception( msg_r )
  {}
  /** Dtor. */
  ~RpmException() throw() override
  {};
};

class GlobalRpmInitException : public RpmException
{
public:
  /** Ctor taking message.
   * Use \ref ZYPP_THROW to throw exceptions.
  */
  GlobalRpmInitException()
      : RpmException("Global RPM initialization failed")
  {}
  /** Dtor. */
  ~GlobalRpmInitException() throw() override
  {};
private:
};

class RpmInvalidRootException : public RpmException
{
public:
  /** Ctor taking message.
   * Use \ref ZYPP_THROW to throw exceptions.
  */
  RpmInvalidRootException( const Pathname & root_r,
                           const Pathname & dbpath_r )
      : RpmException()
      , _root(root_r.asString())
      , _dbpath(dbpath_r.asString())
  {}
  /** Dtor. */
  ~RpmInvalidRootException() throw() override
  {};
  std::string root() const
  {
    return _root;
  }
  std::string dbpath() const
  {
    return _dbpath;
  }
protected:
  std::ostream & dumpOn( std::ostream & str ) const override;
private:
  std::string _root;
  std::string _dbpath;
};

class RpmAccessBlockedException : public RpmException
{
public:
  RpmAccessBlockedException( const Pathname & root_r,
                             const Pathname & dbpath_r )
      : RpmException()
      , _root(root_r.asString())
      , _dbpath(dbpath_r.asString())
  {}
  ~RpmAccessBlockedException() throw() override
  {};
  std::string root() const
  {
    return _root;
  }
  std::string dbpath() const
  {
    return _dbpath;
  }
protected:
  std::ostream & dumpOn( std::ostream & str ) const override;
private:
  std::string _root;
  std::string _dbpath;
};

class RpmSubprocessException : public RpmException
{
public:
  RpmSubprocessException(std::string  errmsg_r)
      : RpmException()
      , _errmsg(std::move(errmsg_r))
  {}
  ~RpmSubprocessException() throw() override
  {};
protected:
  std::ostream & dumpOn( std::ostream & str ) const override;
private:
  std::string _errmsg;
};

class RpmInitException : public RpmException
{
public:
  RpmInitException(const Pathname & root_r,
                   const Pathname & dbpath_r)
      : RpmException()
      , _root(root_r.asString())
      , _dbpath(dbpath_r.asString())
  {}
  ~RpmInitException() throw() override
  {};
protected:
  std::ostream & dumpOn( std::ostream & str ) const override;
private:
  std::string _root;
  std::string _dbpath;
};

class RpmDbOpenException : public RpmException
{
public:
  RpmDbOpenException(const Pathname & root_r,
                     const Pathname & dbpath_r)
      : RpmException()
      , _root(root_r.asString())
      , _dbpath(dbpath_r.asString())
  {}
  ~RpmDbOpenException() throw() override
  {};
protected:
  std::ostream & dumpOn( std::ostream & str ) const override;
private:
  std::string _root;
  std::string _dbpath;
};

class RpmDbAlreadyOpenException : public RpmException
{
public:
  RpmDbAlreadyOpenException(const Pathname & old_root_r,
                            const Pathname & old_dbpath_r,
                            const Pathname & new_root_r,
                            const Pathname & new_dbpath_r)
      : RpmException()
      , _old_root(old_root_r.asString())
      , _old_dbpath(old_dbpath_r.asString())
      , _new_root(new_root_r.asString())
      , _new_dbpath(new_dbpath_r.asString())
  {}
  ~RpmDbAlreadyOpenException() throw() override
  {};
protected:
  std::ostream & dumpOn( std::ostream & str ) const override;
private:
  std::string _old_root;
  std::string _old_dbpath;
  std::string _new_root;
  std::string _new_dbpath;
};

class RpmDbNotOpenException : public RpmException
{
public:
  RpmDbNotOpenException()
      : RpmException()
  {}
  ~RpmDbNotOpenException() throw() override
  {};
protected:
  std::ostream & dumpOn( std::ostream & str ) const override;
private:
};

class RpmDbConvertException : public RpmException
{
public:
  RpmDbConvertException()
      : RpmException()
  {}
  ~RpmDbConvertException() throw() override
  {};
protected:
  std::ostream & dumpOn( std::ostream & str ) const override;
private:
};

class RpmNullDatabaseException : public RpmException
{
public:
  RpmNullDatabaseException()
      : RpmException()
  {}
  ~RpmNullDatabaseException() throw() override
  {};
protected:
  std::ostream & dumpOn( std::ostream & str ) const override;
private:
};

class RpmTransactionFailedException : public RpmException
{
public:
  RpmTransactionFailedException(std::string  errmsg_r)
    : RpmException()
    , _errmsg(std::move(errmsg_r))
  {}
  ~RpmTransactionFailedException() throw() override
    {};
protected:
  std::ostream & dumpOn( std::ostream & str ) const override;
private:
  std::string _errmsg;
};



/////////////////////////////////////////////////////////////////
} // namespace rpm
} // namespace target
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_TARGET_RPM_RPMEXCEPTION_H
