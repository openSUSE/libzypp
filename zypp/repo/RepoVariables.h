/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/RepoVariables.h
 */
#ifndef ZYPP_REPO_VARIABLES_H_
#define ZYPP_REPO_VARIABLES_H_

#include <string>
#include <zypp/base/Function.h>
#include <zypp/base/ValueTransform.h>
#include <zypp/Url.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace repo
  {
    /** Return whether \a str_r has embedded variables. */
    bool hasRepoVarsEmbedded( const std::string & str_r );

    ///////////////////////////////////////////////////////////////////
    /// \class RepoVarExpand
    /// \brief Functor expanding repo variables in a string
    ///
    /// Known variables are determined by a callback function taking a variable
    /// name and returning a pointer to the variable value or \c nullptr if unset.
    ///
    /// The \c $ character introduces variable expansion. A valid variable name
    /// is any non-empty case-insensitive sequence of <tt>[[:alnum:]_]</tt>.
    /// The variable name to be expanded may be enclosed in braces, which are
    /// optional but serve to protect the variable to be expanded from characters
    /// immediately following it which could be interpreted as part of the name.
    ///
    /// When braces are used, the matching ending brace is the first \c } not
    /// escaped by a backslash and not within an embedded variable expansion.
    /// Within braces only \c $, \c } and \c backslash are escaped by a
    /// backslash. There is no escaping outside braces, to stay compatible
    /// with \c YUM (which does not support braces).
    ///
    /// <ul>
    /// <li> \c ${variable}
    /// If \c variable is unset the original is preserved like in \c YUM.
    /// Otherwise, the value of \c variable is substituted.</li>
    ///
    /// <li> \c ${variable:-word} (default value)
    /// If \c variable is unset or empty, the expansion of \c word is substituted.
    /// Otherwise, the value of \c variable is substituted.</li>
    ///
    /// <li> \c ${variable:+word} (alternate value)
    /// If variable is unset or empty nothing is substituted.
    /// Otherwise, the expansion of \c word is substituted.</li>
    /// </ul>
    struct RepoVarExpand
    {
      /** Function taking a variable name and returning a pointer to the variable value or \c nullptr if unset. */
      typedef function<const std::string * ( const std::string & )> VarRetriever;

      /** Return a copy of \a value_r with embedded variables expanded. */
      std::string operator()( const std::string & value_r, VarRetriever varRetriever_r ) const;

      /** \overload moving */
      std::string operator()( std::string && value_r, VarRetriever varRetriever_r ) const;
    };

    /**
     * \short Functor replacing repository variables
     *
     * Replaces the built-in '$arch', '$basearch' and '$releasever' ans also
     * custom repo variables (\see \ref zypp-repovars) in a string with the
     * assigned values.
     *
     * Additionally $releasever_major and $releasever_minor can be used
     * to refer to $releasever major number (everything up to the 1st \c '.' )
     * and minor number (everything after the 1st \c '.' ).
     *
     * \note The $releasever value is overwritten by the environment
     * variable \c ZYPP_REPO_RELEASEVER. This might  be handy for
     * distribution upgrades like this:
     * \code
     *   $ export ZYPP_REPO_RELEASEVER=13.2
     *   $ zypper lr -u
     *   $ zypper dup
     *   ....upgrades to 13.2...
     * \endcode
     * The same can be achieved by using zyppers --releasever global option:
     * \code
     *   $ zypper --releasever 13.2 lr -u
     *   $ zypper --releasever 13.2 dup
     *   ....upgrades to 13.2...
     * \endcode
     * (see \ref zypp-envars)
     *
     * \code
     * Example:
     * ftp://user:secret@site.net/$arch/ -> ftp://user:secret@site.net/i686/
     * http://site.net/?basearch=$basearch -> http://site.net/?basearch=i386
     * \endcode
     *
     * \see \ref RepoVarExpand for supported variable syntax.
     */
    struct RepoVariablesStringReplacer
    {
      std::string operator()( const std::string & value_r ) const;

      /** \overload moving */
      std::string operator()( std::string && value_r ) const;
    };

    /**
     * \short Functor replacing repository variables
     *
     * Replaces repository variables in the URL
     * \see \ref RepoVariablesStringReplacer and \ref RawUrl
     */
    struct RepoVariablesUrlReplacer
    {
      Url operator()( const Url & url_r ) const;
    };
  } // namespace repo
  ///////////////////////////////////////////////////////////////////

  /** \relates RepoVariablesStringReplacer Helper managing repo variables replaced strings */
  typedef base::ValueTransform<std::string, repo::RepoVariablesStringReplacer> RepoVariablesReplacedString;

  /** \relates RepoVariablesStringReplacer Helper managing repo variables replaced string lists */
  typedef base::ContainerTransform<std::list<std::string>, repo::RepoVariablesStringReplacer> RepoVariablesReplacedStringList;

  /** \relates RepoVariablesUrlReplacer Helper managing repo variables replaced urls */
  typedef base::ValueTransform<Url, repo::RepoVariablesUrlReplacer> RepoVariablesReplacedUrl;

  /** \relates RepoVariablesUrlReplacer Helper managing repo variables replaced url lists */
  typedef base::ContainerTransform<std::list<Url>, repo::RepoVariablesUrlReplacer> RepoVariablesReplacedUrlList;

  ////////////////////////////////////////////////////////////////////
  /// \class RawUrl
  /// \brief Convenience type to create \c zypp-rawurl: Urls.
  ///
  /// Strings containing embedded RepoVariables (\see \ref repo::RepoVarExpand)
  /// may not form a valid \ref Url until the variables are actually expanded.
  /// The \c zypp-rawurl: schema was introduced to allow shipping such strings
  /// through legacy APIs expecting a valid \ref Url. Prior to that even the
  /// unexpanded string has to form a valid \ref Url which limited the use of
  /// repo variables in URLs.
  ///
  /// The unexpanded string is shipped as payload encoded in the Url's fragment
  /// part. Later a \ref RepoVariablesStringReplacer can be used to form the
  /// intended \ref Url by substituting the variables.
  ///
  /// The typical use case are url strings read from a .repo (or .service) file.
  /// They are stored as\c zypp-rawurl: within a \ref RepoInfo (or \ref ServiceInfo)
  /// in case they have to written back to the file. The application usually refers
  /// to the expanded \ref Url, but those classes offer to retrieve the raw Urls
  /// as well - as type \ref Url.
  ///
  /// If the string does not contain any variables at all, the native \u Url is
  /// constructed.
  ///
  /// \code
  /// std::string s { "$DISTURL/leap/15.6/repo/oss" }
  /// Url    raw { s }; // Throws because s does not form a valid Url
  /// RawUrl raw { s }; // OK; creates a zypp-rawurl: with s as payload
  ///
  /// // with DISTURL=https://cdn.opensuse.org/distribution
  /// Url url { RepoVariablesUrlReplacer()( raw ) };
  /// // forms https://cdn.opensuse.org/distribution/leap/15.6/repo/oss
  /// \endcode
  ///
  /// \note Support for repo variables forming valid URIs after expansion only
  ///       requires a libzypp provinding 'libzypp(repovarexpand) >= 1.2'
  ///       (\see \ref feature-test).
  ///
  /// \note Methods returning RawUrls are often deprecated because printing them
  /// may accidentally expose credentials (password,proxypassword) embedded in the
  /// raw url-string. An url-string with embedded RepoVariables, forms a valid
  /// \ref Url after variables have been expanded. In the expanded Url passwords
  /// are protected. The unexpanded url-string however does not even need to form
  /// a valid Url, so sensitive data in the string can not be detected and protected.
  /// This is why the \c zypp-rawurl: schema, which is used to store the unexpanded
  /// url-strings, per default prints just it's schema, but not it's content.
  struct RawUrl : public Url
  {
    RawUrl() : Url() {}
    RawUrl( const Url & url_r ) : Url( url_r ) {}
    RawUrl( const std::string & encodedUrl_r );
  };

} // namespace zypp
///////////////////////////////////////////////////////////////////

#endif
