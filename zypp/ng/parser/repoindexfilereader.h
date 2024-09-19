/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/parser/RepoindexFileReader.h
 * Interface of repoindex.xml file reader.
 */
#ifndef ZYPP_NG_PARSER_REPOINDEXFILEREADER_H_INCLUDED
#define ZYPP_NG_PARSER_REPOINDEXFILEREADER_H_INCLUDED

#include <zypp/base/PtrTypes.h>
#include <zypp/base/Function.h>
#include <zypp-core/base/InputStream>
#include <zypp/Pathname.h>
#include <zypp/Date.h>

#include <zypp/ng/context_fwd.h>

namespace zypp::xml {
  class Reader;
}

namespace zyppng
{
  class RepoInfo;

  namespace parser
  {
    class RepoIndexVarReplacer;

  /**
   * Reads through a repoindex.xml file and collects repositories.
   *
   * After each repository is read, a \ref RepoInfo
   * is prepared and \ref _callback
   * is called with this object passed in.
   *
   * The \ref _callback is provided on construction.
   *
   *
   * \code
   * RepoindexFileReader reader(repoindex_file,
   *                  bind( &SomeClass::callbackfunc, &SomeClassInstance, _1) );
   * \endcode
   */
  class RepoIndexFileReader
  {
  public:
   /**
    * Callback definition.
    * First parameter is a \ref RepoInfo object with the resource
    * FIXME return value is ignored
    */
    using ProcessResource = std::function<bool (const RepoInfo &)>;
    /**
     * CTOR. Creates also \ref xml::Reader and starts reading.
     *
     * \param repoindexFile is the repoindex.xml file you want to read
     * \param callback is a function.
     *
     * \see RepoIndexFileReader::ProcessResource
     */
    RepoIndexFileReader( ContextBaseRef ctx,
                         zypp::Pathname repoindexFile,
                         ProcessResource callback);

    /**
     * \short Constructor. Creates the reader and start reading.
     *
     * \param is a valid input stream
     * \param callback Callback that will be called for each repository.
     *
     * \see RepoIndexFileReader::ProcessResource
     */
     RepoIndexFileReader( ContextBaseRef ctx,
                          const zypp::InputStream &is,
                          ProcessResource callback );

    /**
     * DTOR
     */
    ~RepoIndexFileReader();

     RepoIndexFileReader(const RepoIndexFileReader &) = delete;
     RepoIndexFileReader(RepoIndexFileReader &&) = delete;
     RepoIndexFileReader &operator=(const RepoIndexFileReader &) = delete;
     RepoIndexFileReader &operator=(RepoIndexFileReader &&) = delete;


    /** Metadata TTL (repoindex.xml:xpath:/repoindex@ttl or 0). */
    zypp::Date::Duration ttl() const;

  private:
    void run( const zypp::InputStream &is );
    bool consumeNode( zypp::xml::Reader & reader_r );
    bool getAttrValue( const std::string & key_r, zypp::xml::Reader & reader_r, std::string & value_r );

  private:
    ContextBaseRef _zyppCtx;
    /** Function for processing collected data. Passed-in through constructor. */
    ProcessResource _callback;
    std::unique_ptr<RepoIndexVarReplacer> _replacer;
    zypp::DefaultIntegral<zypp::Date::Duration,0> _ttl;
  };


  } // ns parser
} // ns zypp

#endif /*ZYPP_NG_PARSER_REPOINDEXFILEREADER_H_INCLUDED*/
