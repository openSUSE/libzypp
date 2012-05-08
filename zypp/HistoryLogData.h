/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

/** \file zypp/HistoryLogData.h
 *
 */
#ifndef ZYPP_HISTORYLOGDATA_H_
#define ZYPP_HISTORYLOGDATA_H_

#include <iosfwd>

#include "zypp/base/Macros.h"
#include "zypp/Date.h"
#include "zypp/Edition.h"
#include "zypp/Arch.h"
#include "zypp/CheckSum.h"
#include "zypp/Url.h"

#define HISTORY_LOG_DATE_FORMAT "%Y-%m-%d %H:%M:%S"

namespace zypp
{


  ///////////////////////////////////////////////////////////////////
  //
  //  CLASS NAME : HistoryActionID
  //
  /**
   * Enumeration of known history actions.
   *
   * \ingroup g_EnumerationClass
   */
  struct ZYPP_API HistoryActionID
  {
    static const HistoryActionID NONE;

    static const HistoryActionID INSTALL;
    static const HistoryActionID REMOVE;
    static const HistoryActionID REPO_ADD;
    static const HistoryActionID REPO_REMOVE;
    static const HistoryActionID REPO_CHANGE_ALIAS;
    static const HistoryActionID REPO_CHANGE_URL;

    enum ID
    {
      NONE_e,

      INSTALL_e,
      REMOVE_e,
      REPO_ADD_e,
      REPO_REMOVE_e,
      REPO_CHANGE_ALIAS_e,
      REPO_CHANGE_URL_e
    };

    HistoryActionID() : _id(NONE_e) {}

    HistoryActionID(ID id) : _id(id) {}

    explicit HistoryActionID(const std::string & strval_r);

    ID toEnum() const { return _id; }

    static HistoryActionID::ID parse(const std::string & strval_r);

    const std::string & asString(bool pad = false) const;

    private:
    ID _id;
  };

  /** \relates HistoryActionID */
  std::ostream & operator << (std::ostream & str, const HistoryActionID & id);
  ///////////////////////////////////////////////////////////////////


  /////////////////////////////////////////////////////////////////////
  //
  // CLASS NAME: HistoryItem
  //
  class ZYPP_API HistoryItem
  {
  public:
    typedef shared_ptr<HistoryItem> Ptr;
    typedef std::vector<std::string> FieldVector;

  public:
    HistoryItem(FieldVector & fields);
    virtual ~HistoryItem()
    {}

    virtual void dumpTo(std::ostream & str) const;

  public:
    Date date;
    HistoryActionID action;
  };
  /////////////////////////////////////////////////////////////////////


  /////////////////////////////////////////////////////////////////////
  //
  // CLASS NAME: HistoryItemInstall
  //
  class ZYPP_API HistoryItemInstall : public HistoryItem
  {
  public:
    typedef shared_ptr<HistoryItemInstall> Ptr;

    HistoryItemInstall(FieldVector & fields);
    virtual ~HistoryItemInstall()
    {}

    virtual void dumpTo(std::ostream & str) const;

  public:
    std::string   name;
    Edition       edition;
    Arch          arch;
    std::string   reqby; // TODO make this a class ReqBy
    std::string   repoalias;
    CheckSum      checksum;
  };
  /////////////////////////////////////////////////////////////////////


  /////////////////////////////////////////////////////////////////////
  //
  // CLASS NAME: HistoryItemRemove
  //
  class ZYPP_API HistoryItemRemove : public HistoryItem
  {
  public:
    typedef shared_ptr<HistoryItemRemove> Ptr;

    HistoryItemRemove(FieldVector & fields);
    virtual ~HistoryItemRemove()
    {}

    virtual void dumpTo(std::ostream & str)  const;

  public:
    std::string name;
    Edition     edition;
    Arch        arch;
    std::string reqby;
  };
  /////////////////////////////////////////////////////////////////////


  /////////////////////////////////////////////////////////////////////
  //
  // CLASS NAME: HistoryItemRepoAdd
  //
  class ZYPP_API HistoryItemRepoAdd : public HistoryItem
  {
  public:
    typedef shared_ptr<HistoryItemRepoAdd> Ptr;

    HistoryItemRepoAdd(FieldVector & fields);
    virtual ~HistoryItemRepoAdd()
    {}

    virtual void dumpTo(std::ostream & str) const;

  public:
    std::string alias;
    Url         url;
  };
  /////////////////////////////////////////////////////////////////////


  /////////////////////////////////////////////////////////////////////
  //
  // CLASS NAME: HistoryItemRepoRemove
  //
  class ZYPP_API HistoryItemRepoRemove : public HistoryItem
  {
  public:
    typedef shared_ptr<HistoryItemRepoRemove> Ptr;

    HistoryItemRepoRemove(FieldVector & fields);
    virtual ~HistoryItemRepoRemove()
    {}

    virtual void dumpTo(std::ostream & str) const;

  public:
    std::string alias;
  };
  /////////////////////////////////////////////////////////////////////


  /////////////////////////////////////////////////////////////////////
  //
  // CLASS NAME: HistoryItemRepoAliasChange
  //
  class ZYPP_API HistoryItemRepoAliasChange : public HistoryItem
  {
  public:
    typedef shared_ptr<HistoryItemRepoAliasChange> Ptr;

    HistoryItemRepoAliasChange(FieldVector & fields);
    virtual ~HistoryItemRepoAliasChange()
    {}

    virtual void dumpTo(std::ostream & str) const;

  public:
    std::string oldalias;
    std::string newalias;
  };
  /////////////////////////////////////////////////////////////////////


  /////////////////////////////////////////////////////////////////////
  //
  // CLASS NAME: HistoryItemRepoUrlChange
  //
  class ZYPP_API HistoryItemRepoUrlChange : public HistoryItem
  {
  public:
    typedef shared_ptr<HistoryItemRepoUrlChange> Ptr;

    HistoryItemRepoUrlChange(FieldVector & fields);
    virtual ~HistoryItemRepoUrlChange()
    {}

    virtual void dumpTo(std::ostream & str) const;

  public:
    std::string alias;
    Url newurl;
  };
  /////////////////////////////////////////////////////////////////////

  ZYPP_API std::ostream & operator<<(std::ostream & str, const HistoryItem & obj);

}

#endif /* ZYPP_HISTORYLOGDATA_H_ */
