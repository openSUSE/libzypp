/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_REPO_REPOINFOBASE_H_INCLUDED
#define ZYPP_NG_REPO_REPOINFOBASE_H_INCLUDED

#include <iosfwd>

#include <zypp/ng/context.h>
#include <zypp/ng/repo/repoinfobaseshareddata.h>
#include <zypp-core/Globals.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/base/PtrTypes.h>


namespace zyppng::repo
{

    /**
     * \short Base class implementing common features of \ref RepoInfo and
     *        \ref ServiceInfo.
     *
     * \note Name is subject to repo variable replacement
     * (\see \ref RepoVariablesStringReplacer).
     */
    class RepoInfoBase
    {
      friend std::ostream & operator<<( std::ostream & str, const RepoInfoBase & obj );

    public:

      virtual ~RepoInfoBase();

    protected:
      RepoInfoBase(const RepoInfoBase &) = default;
      RepoInfoBase(RepoInfoBase &&) noexcept = default;
      RepoInfoBase &operator=(const RepoInfoBase &) = default;
      RepoInfoBase &operator=(RepoInfoBase &&) noexcept = default;

    public:

      /**
       * Returns the associated context of the RepoInfo
       */
      zyppng::ContextBaseRef context() const;

      /**
       * unique identifier for this source. If not specified
       * It should be generated from the base url.
       *
       * Normally, in a .repo file the section name is used
       * ( [somerepo] )
       */
      const std::string &alias() const;

      /**
       * Same as alias(), just escaped in a way to be a valid file name.
       */
      const std::string &escaped_alias() const;

      /**
       * \short Repository name
       *
       * Short label or description of the repository. Defaults to \ref alias.
       * Subject to repo variable replacement (\see \ref RepoVariablesStringReplacer).
       * ie: "SUSE Linux 10.2 updates"
       */
      std::string name() const;

      /** The raw metadata name (no default, no variables replaced). */
      const std::string &rawName() const;

      /**
       * \short Label for use in messages for the user interface.
       *
       * Returns an alias or name, according to ZConfig::repoLabelIsAlias().
       */
      std::string label() const;

      /** User string: \ref label (alias or name) */
      std::string asUserString() const
      { return label(); }

      /**
       * If enabled is false, then this repository must be ignored as if does
       * not exists, except when checking for duplicate alias.
       */
      bool enabled() const;

      /**
       * If true, the repostory must be refreshed before creating resolvables
       * from it
       */
      bool autorefresh() const;

      /**
       * \short File where this repo was read from
       *
       * \note could be an empty pathname for repo
       * infos created in memory.
       */
       const zypp::Pathname &filepath() const;


    public:

      /**
       * set the repository alias \see alias
       * \param alias
       */
      void setAlias( const std::string &alias );

      /**
       * set the repository name \see name
       * \param name
       */
      void setName( const std::string &name );

      /**
       * enable or disable the repository \see enabled
       * \param enabled
       */
      void setEnabled( bool enabled );

      /**
       * enable or disable autorefresh \see autorefresh
       * \param enabled
       */
      void setAutorefresh( bool autorefresh );

      /**
       * \short set the path to the .repo file
       *
       * The path to the .repo file where this repository
       * was defined, or empty if nowhere.
       *
       * \param path File path
       */
      void setFilepath( const zypp::Pathname &filename );

      /**
       * Write a human-readable representation of this RepoInfoBase object
       * into the \a str stream. Useful for logging.
       */
      virtual std::ostream & dumpOn( std::ostream & str ) const;

      /**
       * Write this RepoInfoBase object into \a str in a <tr>.repo</tt> (ini) file format.
       * Raw values, no variable replacement.
       */
      virtual std::ostream & dumpAsIniOn( std::ostream & str ) const;

      /**
       * Write an XML representation of this object with content (if available).
       * Repo variables replaced.
       */
      virtual std::ostream & dumpAsXmlOn( std::ostream & str, const std::string & content = "" ) const;

    protected:
      RepoInfoBase( RepoInfoBaseSharedData &pimpl );

      // support rw_cow copying for derived types
      RepoInfoBase( const zypp::RWCOW_pointer<RepoInfoBaseSharedData> &other );

      /** Pointer to implementation */
      zypp::RWCOW_pointer<RepoInfoBaseSharedData> _pimpl;
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates RepoInfoBase */
    inline bool operator==( const RepoInfoBase & lhs, const RepoInfoBase & rhs )
    { return ( lhs.context() == rhs.context() && lhs.alias() == rhs.alias()); }

    /** \relates RepoInfoBase */
    inline bool operator!=( const RepoInfoBase & lhs, const RepoInfoBase & rhs )
    { return (lhs.context() != rhs.context() || lhs.alias() != rhs.alias()); }

    inline bool operator<( const RepoInfoBase & lhs, const RepoInfoBase & rhs )
    { return lhs.alias() < rhs.alias(); }

    /** \relates RepoInfoBase Stream output */
    std::ostream & operator<<( std::ostream & str, const RepoInfoBase & obj );


  } // namespace zyppng::repo

#endif /*ZYPP_NG_REPO_REPOINFOBASE_H_INCLUDED*/
