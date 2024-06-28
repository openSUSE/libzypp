/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-media/Mount
 *
*/

// -*- C++ -*-

#ifndef ZYPP_MEDIA_MOUNT_H
#define ZYPP_MEDIA_MOUNT_H

#include <set>
#include <map>
#include <string>
#include <iosfwd>

#include <zypp/ExternalProgram.h>
#include <utility>
#include <zypp-core/KVMap>

namespace zypp {
  namespace media {


    /**
     * A "struct mntent" like mount entry structure,
     * but using std::strings.
     */
    struct MountEntry
    {
        MountEntry(std::string source,
                   std::string target,
                   std::string fstype,
                   std::string options,
                   const int         dumpfreq = 0,
                   const int         passnum  = 0)
            : src(std::move(source))
            , dir(std::move(target))
            , type(std::move(fstype))
            , opts(std::move(options))
            , freq(dumpfreq)
            , pass(passnum)
        {}

        /**
         * Returns true if the src part points to a block device in /dev
         */
        bool isBlockDevice () const;

        std::string src;  //!< name of mounted file system
        std::string dir;  //!< file system path prefix
        std::string type; //!< filesystem / mount type
        std::string opts; //!< mount options
        int         freq; //!< dump frequency in days
        int         pass; //!< pass number on parallel fsck
    };

    /** \relates MountEntry
     * A vector of mount entries.
     */
    using MountEntries = std::vector<MountEntry>;

    /** \relates MountEntry Stream output */
    std::ostream & operator<<( std::ostream & str, const MountEntry & obj );

    /**
     * @short Interface to the mount program
     */
    class Mount
    {
    public:

        /**
         * For passing additional environment variables
         * to mount
         **/
        using Environment = ExternalProgram::Environment;

        /**
         * Mount options. 'key' or 'key=value' pairs, separated by ','
         **/
        using Options = KVMap<kvmap::KVMapBase::CharSep<'=', ','>>;

    public:

        /**
        * Create an new instance.
        */
        Mount();

        /**
        * Clean up.
        */
        ~Mount();

        /**
        * mount device
        *
        * @param source what to mount (e.g. /dev/hda3)
        * @param target where to mount (e.g. /mnt)
        * @param filesystem which filesystem to use (e.g. reiserfs) (-t parameter)
        * @param options mount options (e.g. ro) (-o parameter)
        * @param environment optinal environment to pass (e.g. PASSWD="sennah")
        *
        * \throws MediaException
        *
        */

        void mount ( const std::string& source,
                        const std::string& target,
                        const std::string& filesystem,
                        const std::string& options,
                        const Environment& environment = Environment() );

        /** umount device
         *
         * @param path device or mountpoint to umount
        *
        * \throws MediaException
        *
         * */
        void umount (const std::string& path);

    public:

        /**
        * Return mount entries from /etc/mtab or /etc/fstab file.
        *
        * @param mtab The name of the (mounted) file system description
        *             file to read from. This file should be one /etc/mtab,
        *             /etc/fstab or /proc/mounts. Default is to read
        *             /proc/mounts and /etc/mtab in case is not a symlink
        *             to /proc/mounts.
        * @returns A vector with mount entries or empty vector if reading
        *          or parsing of the mtab file(s) failed.
        */
        static MountEntries
        getEntries(const std::string &mtab = "");

       /**
        * Get the modification time of the /etc/mtab file.
        * \return Modification time of the /etc/mtab file.
        */
        static time_t getMTime();
    };


  } // namespace media
} // namespace zypp

#endif
