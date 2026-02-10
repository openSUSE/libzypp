/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/MirroredOrigin.h
 *
*/
#ifndef ZYPP_REPO_ORIGIN_INCLUDED_H
#define ZYPP_REPO_ORIGIN_INCLUDED_H

#include <zypp-core/Url.h>
#include <zypp-core/base/PtrTypes.h>

#include <boost/iterator/iterator_facade.hpp>

#include <any>
#include <string>
#include <vector>

namespace zypp {

   /**
    * @brief Represents a single, configurable network endpoint, combining a URL with specific access settings.
    *
    * An OriginEndpoint object encapsulates a Uniform Resource Locator (URL) and an
    * associated configuration. This allows each endpoint (whether it's
    * an authoritative source or a mirror) to have its own distinct configuration,
    * such as priority, enabled status, region, timeouts configured via custom tags.
    *
    * This class is primarily intended to be used by classes like `MirroredOrigin`
    * to define the individual characteristics of the primary data source and each of
    * its alternative mirror locations. By using OriginEndpoint, both authority and
    * mirrors can be treated uniformly as configurable access points.
    *
    * \see MirroredOrigin for an example of how OriginEndpoint instances are utilized.
    */
   class ZYPP_API OriginEndpoint {
   public:

// The header is exposed and in old SPs yast builds with c++11.
#ifdef __cpp_lib_any
     using SettingsMap = std::unordered_map<std::string, std::any>;
     OriginEndpoint(zypp::Url url, SettingsMap settings );
#endif

     OriginEndpoint();
     OriginEndpoint( zypp::Url url ); // combine with other constructor once we remove the C++11 constraint

     ~OriginEndpoint() = default;
     OriginEndpoint(const OriginEndpoint &) = default;
     OriginEndpoint(OriginEndpoint &&) = default;
     OriginEndpoint &operator=(const OriginEndpoint &) = default;
     OriginEndpoint &operator=(OriginEndpoint &&) = default;

     const zypp::Url &url() const;
     zypp::Url &url();
     void setUrl(const zypp::Url &newUrl);

     bool hasConfig( const std::string &key ) const;

// The header is exposed and in old SPs yast builds with c++11.
#ifdef __cpp_lib_any
     void setConfig( const std::string &key, std::any value );
     const std::any &getConfig( const std::string &key ) const;
     std::any &getConfig( const std::string &key );
     void eraseConfigValue( const std::string &key );

     const SettingsMap &config() const;
     SettingsMap &config();

     template <typename T>
     std::enable_if_t<!std::is_same_v<T, std::any>> setConfig ( const std::string &key, T &&value ) {
       setConfig( key, std::make_any<T>( std::forward<T>(value) ) );
     }

     template <typename T>
     std::enable_if_t<!std::is_same_v<T, std::any>, const T&> getConfig( const std::string &key ) const {
       const std::any &c = getConfig(key);
       // use the pointer overloads to get to a const reference of the containing type
       // we need to throw std::bad_any_cast manually here
       const T* ref = std::any_cast<const T>(&c);
       if ( !ref )
         throw std::bad_any_cast();

       return *ref;
     }

     template <typename T>
     std::enable_if_t<!std::is_same_v<T, std::any>, T&> getConfig( const std::string &key ) {
       std::any &c = getConfig(key);
       // use the pointer overloads to get to a const reference of the containing type
       // we need to throw std::bad_any_cast manually here
       T* ref = std::any_cast<T>(&c);
       if ( !ref )
         throw std::bad_any_cast();

       return *ref;
     }
#endif

     /**
      * \return Returns the endpoints scheme, this is the same as calling endpoint.url().getScheme()
      */
     std::string scheme() const;

     /**
      * \returns true if the Url has a downloading scheme
      */
     bool schemeIsDownloading() const;

     bool isValid() const;

   private:
     struct Private;
     RWCOW_pointer<Private> _pimpl;
   };


   ZYPP_API std::ostream & operator<<( std::ostream & str, const OriginEndpoint & url );

   /**
    * needed for std::set
    */
   ZYPP_API bool operator<( const OriginEndpoint &lhs, const OriginEndpoint &rhs );

   /**
    * needed for find, two OriginEndpoint's are equal when the Urls match, currently settings are not compared
    */
   ZYPP_API bool operator==( const OriginEndpoint &lhs, const OriginEndpoint &rhs );

   /**
    * needed for find, two OriginEndpoint's are equal when the Urls match, currently settings are not compared
    */
   ZYPP_API bool operator!=( const OriginEndpoint &lhs, const OriginEndpoint &rhs );

  /**
   * \brief Manages a data source characterized by an authoritative URL and a list of mirror URLs.
   *
   * A MirroredOrigin object encapsulates the access information for a data source
   * that has a primary (authoritative) access point and potentially multiple
   * alternative (mirror) access points. This class is designed to be generic
   * and can be used for various types of data sources where redundancy or
   * alternative access points are needed.
   *
   * The core components managed by MirroredOrigin are:
   * - An **authoritative URL**: This is the main, canonical URL for accessing
   * the data source.
   * - A collection of **mirror URLs**: These are alternative URLs that point to
   * (ideally) identical copies of the data source. Mirrors are typically used for
   * improving download speeds, providing redundancy, or distributing load.
   *
   * Beyond just the URLs, this class can be extended or used in conjunction with
   * mechanisms to store configuration settings pertinent to accessing these URLs
   * (e.g., priorities, credentials, retry strategies). This makes it a more
   * comprehensive definition for a data endpoint than a simple URL string.
   *
   * \par Example Use Case:
   * A software component might need to download a large data file. A MirroredOrigin
   * instance could define the primary download server and several mirror servers.
   * The component would then use this information to attempt downloads, potentially
   * falling back to mirrors if the primary is unavailable or slow.
   *
   * This class provides a structured way to handle primary and alternative locations
   * for any resource that might be replicated.
   */
  class ZYPP_API MirroredOrigin {

  public:
     template <class Parent, class Value>
     class iter : public boost::iterator_facade<
           iter<Parent, Value>
           , Value
           , boost::forward_traversal_tag
      >
     {
     public:
        iter(){}

        explicit iter( Parent *list, uint idx )
           : _list(list)
           , _idx(idx)
        {}

     private:
        friend class boost::iterator_core_access;

        bool equal(iter<Parent, Value> const& other) const
        {
           return ((this->_list == other._list) && (this->_idx == other._idx));
        }

        void increment()
        {
           if ( !_list )
              return;
           // allow _idx max to be endpointCount for end() iterator
           if ( (_idx+1) > _list->endpointCount() )
              return;
           _idx++;
        }

        Value& dereference() const
        {
           if ( !_list || _idx >= _list->endpointCount() )
              throw std::out_of_range( "OriginEndpoint index out of range." );
           return _list->operator [](_idx);
        }

        Parent *_list = nullptr;
        uint _idx = 0;
     };
     using endpoint_iterator = iter<MirroredOrigin, OriginEndpoint> ;
     using endpoint_const_iterator = iter<MirroredOrigin const, OriginEndpoint const>;

    MirroredOrigin( );
    MirroredOrigin( OriginEndpoint authority, std::vector<OriginEndpoint> mirrors = {} );

    /*!
     * Changes the authorities to \a newAuthority
     */
    void setAuthority ( OriginEndpoint newAuthority );

    /*!
     * \return the authority Urls
     */
    const std::vector<OriginEndpoint> &authorities() const;

    /*!
     * \return the mirrors for this origin, may be empty
     */
    const std::vector<OriginEndpoint> &mirrors() const;

    /**
     * \return true if the authority contains a valid URL
     */
    bool isValid() const;

    bool addAuthority( OriginEndpoint newAuthority );
    bool addMirror( OriginEndpoint newMirror );
    void setMirrors( std::vector<OriginEndpoint> mirrors );
    void clearMirrors();

    /**
     * \return Returns the authority's scheme, all mirrors should follow
     */
    std::string scheme() const;

    /**
     * \returns true if the authority has a downloading scheme
     */
    bool schemeIsDownloading() const;

    /*!
     * A iterator over all endpoints, including authority at index 0
     */
    endpoint_iterator begin() {
       return endpoint_iterator( this, 0 );
    }

    /*!
     * End iterator over all endpoints, including authority at index 0
     */
    endpoint_iterator end() {
       return endpoint_iterator( this, endpointCount() );
    }

    endpoint_const_iterator begin() const {
       return endpoint_const_iterator( this, 0 );
    }

    endpoint_const_iterator end() const {
       return endpoint_const_iterator( this, endpointCount() );
    }

    /*!
     * \return the total number of endpoints, including authority
     */
    uint endpointCount() const;

    /*!
     * \return the total number of authorities
     */
    uint authorityCount() const;

    /*!
     * Index based access to endpoints, index 0 is always the authority
     * \throws std::out_of_range on a out of range index param
     */
    const OriginEndpoint &operator[]( uint index ) const { return at(index); }
    OriginEndpoint &operator[]( uint index ) { return at(index); }

    /*!
     * Index based access to endpoints, index 0 is always the authority
     */
    const OriginEndpoint &at( uint index ) const;
    OriginEndpoint &at( uint index );

  private:
    struct Private;
    RWCOW_pointer<Private> _pimpl;
  };

  ZYPP_API std::ostream & operator<<( std::ostream & str, const MirroredOrigin & origin );


  /**
   * @brief A smart container that manages a collection of MirroredOrigin objects,
   * automatically grouping endpoints and preserving the insertion order of their schemes.
   *
   * This class acts as a high-level manager for data sources. When an
   * OriginEndpoint is added, this set automatically determines its URL scheme
   * (e.g., 'http', 'ftp', 'file') and adds it to the appropriate MirroredOrigin
   * instance. Currently only schemes seen as "downloading" are grouped.
   * \sa zypp::Url::schemeIsDownloading
   *
   * If a MirroredOrigin for a given scheme does not yet exist, it is created
   * automatically, with the first added endpoint for that scheme becoming the
   * authoritative endpoint. Subsequent endpoints for the same scheme are added
   * as mirrors.
   *
   * \par Ordering Guarantee
   * This class guarantees that iterating over the origins will follow the
   * order in which schemes were first introduced. For example, if the first
   * endpoint added is HTTP, then DVD, then another HTTP, the "http" origin
   * will always appear before the "dvd" origin during iteration.
   */
  class ZYPP_API MirroredOriginSet {

  public:
    using iterator = std::vector<MirroredOrigin>::iterator;
    using const_iterator = std::vector<MirroredOrigin>::const_iterator;
    using size_type = size_t;
    using value_type = MirroredOrigin;

    MirroredOriginSet( );

    /**
     * \brief Constructs the set, initializing it with a starting list of endpoints.
     * \param eps A vector of OriginEndpoint objects to add to the set upon construction.
     * The standard grouping logic will be applied to these endpoints.
     */
    MirroredOriginSet( std::vector<OriginEndpoint> eps);

    MirroredOriginSet( std::vector<zypp::Url> urls );

    MirroredOriginSet( std::list<zypp::Url> urls );

    /**
     * \brief Accesses the MirroredOrigin at a specific index.
     * \param idx The index of the element to return (respects insertion order).
     * \return A const reference to the requested MirroredOrigin.
     * \throws std::out_of_range if idx >= size().
     */
    const MirroredOrigin &at( size_type idx ) const;

    /**
     * \brief Accesses the MirroredOrigin at a specific index.
     * \param idx The index of the element to return (respects insertion order).
     * \return A reference to the requested MirroredOrigin.
     * \throws std::out_of_range if idx >= size().
     */
    MirroredOrigin &at( size_type idx );

    /**
     * \brief Finds the MirroredOrigin that contains a specific URL.
     * This method searches through both the authority and all mirror URLs of every
     * MirroredOrigin in the set.
     * \param url The URL to search for.
     * \return A const_iterator to the matching MirroredOrigin, or end() if not found.
     */
    const_iterator findByUrl( const zypp::Url &url ) const;

    /**
     * \brief Finds the MirroredOrigin that contains a specific URL.
     * This method searches through both the authority and all mirror URLs of every
     * MirroredOrigin in the set.
     * \param url The URL to search for.
     * \return An iterator to the matching MirroredOrigin, or end() if not found.
     */
    iterator findByUrl( const zypp::Url &url );

    /**
     * \brief A convenience method to add multiple endpoints from a range.
     * \tparam InputIterator The type of the iterator for the source range.
     * \param first An iterator to the beginning of the range of OriginEndpoints to insert.
     * \param last An iterator to the end of the range of OriginEndpoints to insert.
     */
    template<typename InputIterator>
    void addEndpoints( InputIterator first, InputIterator last ) {
      std::for_each(first,last,[this]( OriginEndpoint ep ) { addEndpoint(std::move(ep));} );
    }

    /**
     * \brief Adds a single endpoint as an authority, routing it to the correct MirroredOrigin.
     * Extracts the scheme from the endpoint's URL. If a MirroredOrigin for that
     * scheme exists, the endpoint is added as an authority. Otherwise, a new
     * MirroredOrigin is created with this endpoint as its first authority.
     * \param endpoint The OriginEndpoint to add.
     */
    void addAuthorityEndpoint( OriginEndpoint endpoint );

    /**
     * \brief Adds a single endpoint, routing it to the correct MirroredOrigin.
     * Extracts the scheme from the endpoint's URL. If a MirroredOrigin for that
     * scheme exists, the endpoint is added as a mirror. Otherwise, a new
     * MirroredOrigin is created with this endpoint as its authority.
     * \param endpoint The OriginEndpoint to add.
     */
    void addEndpoint( OriginEndpoint endpoint );

    /**
     * \brief A convenience method to add multiple endpoints from a vector.
     * \param endpoints A vector of OriginEndpoint objects to add.
     */
    void addEndpoints(std::vector<OriginEndpoint> endpoints );

    /// \brief Returns an iterator to the first MirroredOrigin in insertion order.
    iterator begin();

    /// \brief Returns an iterator to the element following the last MirroredOrigin.
    iterator end();

    /// \brief Returns a const_iterator to the first MirroredOrigin in insertion order.
    const_iterator begin() const;

    /// \brief Returns a const_iterator to the element following the last MirroredOrigin.
    const_iterator end() const;

    /**
     * \brief Returns the number of MirroredOrigin objects in the set.
     * \return The number of elements.
     */
    size_type size() const;

    bool empty() const;

    void clear();

    /** Whether this set contains more than one Url in total (authorities or mirrors).
     * Some parts of the code like to raise an interactive MediaChangeReport
     * only if the Repository itself does not provide any fallbacks.
     */
    bool hasFallbackUrls() const;

  private:
    struct Private;
    RWCOW_pointer<Private> _pimpl;
  };

  ZYPP_API std::ostream & operator<<( std::ostream & str, const MirroredOriginSet & origin );

}
#endif
