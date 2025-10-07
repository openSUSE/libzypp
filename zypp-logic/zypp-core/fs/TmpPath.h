/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-core/fs/TmpPath.h
 *
*/
#ifndef ZYPP_CORE_FS_TMPPATH_H
#define ZYPP_CORE_FS_TMPPATH_H

#include <iosfwd>

#include <zypp-core/Pathname.h>
#include <zypp-core/base/PtrTypes.h>
#include <zypp-core/ManagedFile.h>

namespace zypp {
  namespace filesystem {

    /**
     * @short Automatically deletes files or directories when no longer needed.
     *
     * TmpPath is constructed from a Pathname. Multiple TmpPath instances
     * created by copy and assign, share the same reference counted internal
     * representation.
     *
     * When the last reference drops any file or directory located at the path
     * passed to the ctor is deleted (recursively in case of directories). This
     * behavior can be changed by calling \ref autoCleanup.
     *
     * Principally serves as base class, but standalone usable.
     **/
    class ZYPP_API TmpPath
    {
      public:
        /**
         * Default Ctor. An empty Pathname.
         **/
        TmpPath();

        /**
         * Ctor. Takes a Pathname.
         **/
        explicit TmpPath( Pathname tmpPath_r );

        /**
         * Dtor.
         **/
        virtual ~TmpPath();

        /**
         * Test whether the Pathname is valid (i.e. not empty. NOT whether
         * it really denotes an existing file or directory).
         **/
        explicit operator bool() const;

        /**
         * @return The Pathname.
         **/
        Pathname path() const;

        /**
         * Type conversion to Pathname.
         **/
        operator Pathname() const
        { return path(); }

        /**
         * Whether path is valid and deleted when the last reference drops.
         */
        bool autoCleanup() const;

        /**
         * Turn \ref autoCleanup on/off if path is valid.
         */
        void autoCleanup( bool yesno_r );

      public:
        /**
         * @return The default directory where temporary files
         * should be are created: ($ZYPPTMPDIR|/var/tmp)/zypp.tmp/".
         * \Note bsc#1249435: We use 'zypp.tmp' as a fix directory
         * component to ease setting up SELinux policies.
         **/
        static const Pathname & defaultLocation();

      protected:
        class Impl;
        RW_pointer<Impl> _impl;

    };

    /**
     * \relates TmpPath Stream output as pathname.
     **/
    inline std::ostream & operator<<( std::ostream & str, const TmpPath & obj )
    { return str << static_cast<Pathname>(obj); }

    ///////////////////////////////////////////////////////////////////

    /**
     * @short Provide a new empty temporary file and delete it when no
     * longer needed.
     *
     * The temporary file is per default created in \ref TmpPath::defaultLocation
     * and named 'TmpFile.XXXXXX', with XXXXXX replaced by a string which
     * makes the name unique. Different location and file prefix may be
     * passed to the ctor. TmpFile is created with mode 0600.
     *
     * TmpFile provides the Pathname of the temporary file, or an empty
     * path in case of any error.
     **/
    class ZYPP_API TmpFile : public TmpPath
    {
      public:
        /**
         * Ctor. Takes a Pathname.
         **/
        explicit TmpFile( const Pathname & inParentDir_r = defaultLocation(),
                          const std::string & prefix_r = defaultPrefix() );

        /** Provide a new empty temporary directory as sibling.
         * \code
         *   TmpFile s = makeSibling( "/var/lib/myfile" );
         *   // returns: /var/lib/myfile.XXXXXX
         * \endcode
         * If \c sibling_r exists, sibling is created using the same mode.
         */
        static TmpFile makeSibling( const Pathname & sibling_r );
        /** \overload If \c sibling_r does not exist, the sibling is created using \c mode,
         * modified by the process's umask in the usual way, rather than the default 0700.
         */
        static TmpFile makeSibling( const Pathname & sibling_r, unsigned mode );

        /**
         * Create a temporary file and convert it to a automatically
         * cleaned up ManagedFile
         */
        static ManagedFile asManagedFile ();

        /**
         * Create a temporary file in \a inParentDir_r with prefix \a prefix_r and convert it to a automatically
         * cleaned up ManagedFile
         */
        static ManagedFile asManagedFile ( const Pathname & inParentDir_r, const std::string & prefix_r = defaultPrefix() );

      public:
        /**
         * @return The default prefix for temporary files (TmpFile.)
         **/
        static const std::string & defaultPrefix();

    };
    ///////////////////////////////////////////////////////////////////

    /**
     * @short Provide a new empty temporary directory and recursively
     * delete it when no longer needed.
     *
     * The temporary directory is per default created in \ref TmpPath::defaultLocation
     * and named 'TmpDir.XXXXXX', with XXXXXX replaced by a string which
     * makes the  name unique. Different location and file prefix may be
     * passed to the ctor. TmpDir is created with mode 0700.
     *
     * TmpDir provides the Pathname of the temporary directory , or an empty
     * path in case of any error.
     **/
    class ZYPP_API TmpDir : public TmpPath
    {
      public:
        /**
         * Ctor. Takes a Pathname.
         **/
        explicit TmpDir( const Pathname & inParentDir_r = defaultLocation(),
                         const std::string & prefix_r = defaultPrefix() );

        /** Provide a new empty temporary directory as sibling.
         * \code
         *   TmpDir s = makeSibling( "/var/lib/mydir" );
         *   // returns: /var/lib/mydir.XXXXXX
         * \endcode
         * If \c sibling_r exists, sibling is created using the same mode.
         */
        static TmpDir makeSibling( const Pathname & sibling_r );
        /** \overload If \c sibling_r does not exist, the sibling is created using \c mode,
         * modified by the process's umask in the usual way, rather than the default 0700.
         */
        static TmpDir makeSibling( const Pathname & sibling_r, unsigned mode );

      public:
        /**
         * @return The default prefix for temporary directories (TmpDir.)
         **/
        static const std::string & defaultPrefix();
    };
    ///////////////////////////////////////////////////////////////////

  } // namespace filesystem

  /** Global access to the zypp.TMPDIR (created on demand, deleted when libzypp is unloaded) */
  Pathname myTmpDir() ZYPP_API;

} // namespace zypp

#endif // ZYPP_CORE_FS_TMPPATH_H
