/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/PluginSigcheck.h
 */
#ifndef ZYPP_REPO_PLUGINSIGCHECK_H
#define ZYPP_REPO_PLUGINSIGCHECK_H

#include <iosfwd>
#include <string>
#include <vector>
#include <map>

#include <zypp-core/Pathname.h>
#include <zypp-core/base/PtrTypes.h>
#include <zypp-core/base/Exception.h>

namespace zypp
{
  /** \defgroup SIGCHECKPLUGIN Mandatory signature checking
   * \see \ref plugin-sigcheck
   */

  ///////////////////////////////////////////////////////////////////

  /** Exception thrown by \ref SigcheckPlugins.
   * \ingroup SIGCHECKPLUGIN
   */
  struct SigcheckPluginException : public Exception
  {
    SigcheckPluginException( const Exception & history_r )
    : Exception( "SigcheckPlugin:", history_r )
    {}
    SigcheckPluginException( const std::string & msg_r )
    : Exception( "SigcheckPlugin: "+msg_r )
    {}
    SigcheckPluginException( const std::string & msg_r, const Exception & history_r )
    : Exception( "SigcheckPlugin: "+msg_r, history_r )
    {}
  };

  ///////////////////////////////////////////////////////////////////

  /**
   * \ingroup SIGCHECKPLUGIN
   */
  class SigcheckPlugin
  {
  public:
    SigcheckPlugin( const SigcheckPlugin & )                 = delete;
    SigcheckPlugin & operator=( const SigcheckPlugin & )     = delete;
    SigcheckPlugin( SigcheckPlugin && ) noexcept             = default;
    SigcheckPlugin & operator=( SigcheckPlugin && ) noexcept = default;
    ~SigcheckPlugin()                                        = default;

  public:
    using Args = std::vector<std::string>;

    /** Creates a SigcheckPlugin. */
    SigcheckPlugin( Pathname script_r );
    SigcheckPlugin( Pathname script_r, Args args_r );

    /** Launch the plugin.
     * \throws SigcheckPluginException on error of if already launched
     */
    void launch( const Pathname & chroot_r = Pathname() );

    /** Extension of a signature file to retrieve */
    std::string sigExtension() const;

    /** Extension of a key file to retrieve */
    std::string keyExtension() const;

    /** Let plugin do the signature check
     * \throws SigcheckPluginException on error
     */
    void sigcheck( const Pathname & data_r, const Pathname & sig_r = Pathname(), const Pathname & key_r = Pathname() ) const;

  public:
    struct Impl;
  private:
    RW_pointer<Impl> _pimpl;
    friend std::ostream & operator<<( std::ostream & str, const SigcheckPlugin & obj );
  };

  ///////////////////////////////////////////////////////////////////

  /** Handle a bunch of SigcheckPlugins.
   *
   * \note: The class asserts the address of the contained SigcheckPlugins
   * does not change throughout it's lifetime. It's suitable for workflows
   * to capture the address of an included \ref SigcheckPlugin.
   *
   * \ingroup SIGCHECKPLUGIN
   */
  class SigcheckPlugins
  {
  public:
    SigcheckPlugins( const SigcheckPlugins & )                 = delete;
    SigcheckPlugins & operator=( const SigcheckPlugins & )     = delete;
    SigcheckPlugins( SigcheckPlugins && ) noexcept             = default;
    SigcheckPlugins & operator=( SigcheckPlugins && ) noexcept = default;
    ~SigcheckPlugins()                                         = default;

  public:
    using Plugins = std::vector<SigcheckPlugin>;

    /** No plugins. */
    SigcheckPlugins();

    /** Setup from a \-escaped commandline.
     * Multiple commands are separated by ';'.
     * Command and arguments separated by ' '.
     * Literal '\' and separators must be \-escaped.
     *
     * The plugins are expected to be found in the \a "sigcheck/"
     * subdirectory (of \a plugindir_r if defined).
     */
    SigcheckPlugins( const std::string & cmdline_r, const Pathname & plugindir_r = Pathname() );

    /** Whether there are plugins defined. */
    explicit operator bool() const;

    /** All plugins (their addresses can be captured). */
    const Plugins & plugins();

    /** Launch all plugins (optionally chrooted). */
    void launch( const Pathname & chroot_r = Pathname() );

  public:
    struct Impl;
  private:
    RW_pointer<Impl> _pimpl;
    friend std::ostream & operator<<( std::ostream & str, const SigcheckPlugins & obj );
  };

} // namespace zypp
#endif // ZYPP_REPO_PLUGINSIGCHECK_H
