/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/Pool.h
 *
*/
#ifndef ZYPP_SAT_POOL_H
#define ZYPP_SAT_POOL_H

#include <iosfwd>

#include <zypp/Pathname.h>

#include <zypp/sat/detail/PoolMember.h>
#include <zypp/Repository.h>
#include <zypp/sat/WhatProvides.h>
#include <zypp/sat/SolvableSet.h>
#include <zypp/sat/Queue.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  class SerialNumber;
  class RepoInfo;

  ///////////////////////////////////////////////////////////////////
  namespace sat
  { /////////////////////////////////////////////////////////////////

    class SolvableSpec;

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : Pool
    //
    /** Global sat-pool.
     *
     * Explicitly shared singleton \ref Pool::instance.
     */
    class ZYPP_API Pool : protected detail::PoolMember
    {
      public:
        using SolvableIterator = detail::SolvableIterator;
        using RepositoryIterator = zypp::detail::RepositoryIterator;
        using size_type = detail::size_type;

      public:
        /** Singleton ctor. */
        static Pool instance()
        { return Pool(); }

        /** Ctor from \ref PoolMember. */
        Pool( const detail::PoolMember & )
        {}

      public:
        /** Internal array size for stats only. */
        size_type capacity() const;

        /** Housekeeping data serial number. */
        const SerialNumber & serial() const;

        /** Serial number changing whenever resusePoolIDs==true was used. ResPool must also invalidate its PoolItems! */
        const SerialNumber & serialIDs() const;

        /** Update housekeeping data if necessary (e.g. whatprovides). */
        void prepare() const;

        /** Get rootdir (for file conflicts check) */
        Pathname rootDir() const;

        /** Set rootdir (for file conflicts check) */
        void rootDir( const Pathname & root_r );

      public:
        /** Whether \ref Pool contains repos. */
        bool reposEmpty() const;

        /** Number of repos in \ref Pool. */
        size_type reposSize() const;

        /** Iterator to the first \ref Repository. */
        RepositoryIterator reposBegin() const;

        /** Iterator behind the last \ref Repository. */
        RepositoryIterator reposEnd() const;

        /** Iterate the repositories. */
        Iterable<RepositoryIterator> repos() const
        { return makeIterable( reposBegin(), reposEnd() ); }

        /** Return a \ref Repository named \c alias_r.
         * It a such a \ref Repository does not already exist
         * a new empty \ref Repository is created.
         */
        Repository reposInsert( const std::string & alias_r );

        /** Find a \ref Repository named \c alias_r.
         * Returns \ref noRepository if there is no such \ref Repository.
         */
        Repository reposFind( const std::string & alias_r ) const;

        /** Remove a \ref Repository named \c alias_r.
         * \see \ref Repository::eraseFromPool
         */
        void reposErase( const std::string & alias_r )
        { reposFind( alias_r ).eraseFromPool(); }

        /** Remove all repos from the pool.
         * This also shrinks a pool which may have become large
         * after having added and removed repos lots of times.
         */
        void reposEraseAll()
        { while ( ! reposEmpty() ) reposErase( reposBegin()->alias() ); }

      public:
        /** Reserved system repository alias \c @System. */
        static const std::string & systemRepoAlias();

        /** Return the system repository if it is on the pool. */
        Repository findSystemRepo() const;

        /** Return the system repository, create it if missing. */
        Repository systemRepo();

      public:
        /** Load \ref Solvables from a solv-file into a \ref Repository named \c name_r.
         * In case of an exception the \ref Repository is removed from the \ref Pool.
         * \throws Exception if loading the solv-file fails.
         * \see \ref Repository::EraseFromPool
        */
        Repository addRepoSolv( const Pathname & file_r, const std::string & name_r );
        /** \overload Using the files basename as \ref Repository name. */
        Repository addRepoSolv( const Pathname & file_r );
        /** \overload Using the \ref RepoInfo::alias \ref Repo name.
         * Additionally stores the \ref RepoInfo. \See \ref Prool::setInfo.
        */
        Repository addRepoSolv( const Pathname & file_r, const RepoInfo & info_r );

      public:
        /** Load \ref Solvables from a helix-file into a \ref Repository named \c name_r.
         * Supports loading of gzip compressed files (.gz). In case of an exception
         * the \ref Repository is removed from the \ref Pool.
         * \throws Exception if loading the helix-file fails.
         * \see \ref Repository::EraseFromPool
        */
        Repository addRepoHelix( const Pathname & file_r, const std::string & name_r );
        /** \overload Using the files basename as \ref Repository name. */
        Repository addRepoHelix( const Pathname & file_r );
        /** \overload Using the \ref RepoInfo::alias \ref Repo name.
         * Additionally stores the \ref RepoInfo. \See \ref Prool::setInfo.
        */
        Repository addRepoHelix( const Pathname & file_r, const RepoInfo & info_r );

      public:
        /** Whether \ref Pool contains solvables. */
        bool solvablesEmpty() const;

        /** Number of solvables in \ref Pool. */
        size_type solvablesSize() const;

        /** Iterator to the first \ref Solvable. */
        SolvableIterator solvablesBegin() const;

        /** Iterator behind the last \ref Solvable. */
        SolvableIterator solvablesEnd() const;

        /** Iterate the solvables. */
        Iterable<SolvableIterator> solvables() const
        { return makeIterable( solvablesBegin(), solvablesEnd() ); }

      public:
        /** \name Iterate all Solvables matching a \c TFilter. */
        //@{
        template<class TFilter>
        filter_iterator<TFilter,SolvableIterator> filterBegin( const TFilter & filter_r ) const
        { return make_filter_iterator( filter_r, solvablesBegin(), solvablesEnd() ); }

        template<class TFilter>
        filter_iterator<TFilter,SolvableIterator> filterEnd( const TFilter & filter_r ) const
        { return make_filter_iterator( filter_r, solvablesEnd(), solvablesEnd() ); }
        //@}

     public:
        /** Conainer of all \ref Solvable providing \c cap_r.  */
        WhatProvides whatProvides( Capability cap_r ) const
        { return WhatProvides( cap_r ); }

        Queue whatMatchesDep( const SolvAttr &attr, const Capability &cap ) const;
        Queue whatMatchesSolvable ( const SolvAttr &attr, const Solvable &solv ) const;
        Queue whatContainsDep ( const SolvAttr &attr, const Capability &cap ) const;

      public:
        /** \name Requested locales. */
        //@{
        /** Set the default language for retrieving translated texts.
         * Updated when calling \ref ZConfig::setTextLocale.
         */
        void setTextLocale( const Locale & locale_r );

        /** Set the requested locales.
         * Languages to be supported by the system, e.g. language specific
         * packages to be installed.
         */
        void setRequestedLocales( const LocaleSet & locales_r );

        /** Add one \ref Locale to the set of requested locales.
         * Return \c true if \c locale_r was newly added to the set.
        */
        bool addRequestedLocale( const Locale & locale_r );

        /** Erase one \ref Locale from the set of requested locales.
        * Return \c false if \c locale_r was not found in the set.
         */
        bool eraseRequestedLocale( const Locale & locale_r );

        /** Return the requested locales.
         * \see \ref setRequestedLocales
        */
        const LocaleSet & getRequestedLocales() const;

        /** Whether this \ref Locale is in the set of requested locales. */
        bool isRequestedLocale( const Locale & locale_r ) const;


        /** Start tracking changes based on this \a locales_r. */
        void initRequestedLocales( const LocaleSet & locales_r );

        /** Added since last initRequestedLocales. */
        const LocaleSet & getAddedRequestedLocales() const;

        /** Removed since last initRequestedLocales.*/
        const LocaleSet & getRemovedRequestedLocales() const;


        /** Get the set of available locales.
         * This is computed from the package data so it actually
         * represents all locales packages claim to support.
         */
        const LocaleSet & getAvailableLocales() const;

        /** Whether this \ref Locale is in the set of available locales. */
        bool isAvailableLocale( const Locale & locale_r ) const;
        //@}

      public:
        /** \name Multiversion install.
         * Whether the pool contains packages which are multiversion installable.
         * \see \ref Solvable::multiversionInstall
         * \see \ref ZConfig::multiversionSpec
         */
        //@{
        using MultiversionList = SolvableSet;
        const MultiversionList & multiversion() const;
        /** \deprecated Legacy, use multiversion().empty() instead. */
        ZYPP_DEPRECATED bool multiversionEmpty() const { return multiversion().empty(); }
        //@}

      public:
        /** \name Autoinstalled */
        //@{
        /** Get ident list of all autoinstalled solvables. */
        Queue autoInstalled() const;
        /** Set ident list of all autoinstalled solvables. */
        void setAutoInstalled( const Queue & autoInstalled_r );
        //@}

      public:
        /** \name Needreboot */
        //@{
        /** Solvables which should trigger the reboot-needed hint if installed/updated. */
        void setNeedrebootSpec( sat::SolvableSpec needrebootSpec_r );
        //@}

      public:
        /** Expert backdoor. */
        detail::CPool * get() const;
      private:
        /** Default ctor */
        Pool() {}
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates Pool Stream output */
    std::ostream & operator<<( std::ostream & str, const Pool & obj ) ZYPP_API;

    /** \relates Pool */
    inline bool operator==( const Pool & lhs, const Pool & rhs )
    { return lhs.get() == rhs.get(); }

    /** \relates Pool */
    inline bool operator!=( const Pool & lhs, const Pool & rhs )
    { return lhs.get() != rhs.get(); }

    /** Create solv file content digest for zypper bash completion */
    void updateSolvFileIndex( const Pathname & solvfile_r );

    /////////////////////////////////////////////////////////////////
  } // namespace sat
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SAT_POOL_H
