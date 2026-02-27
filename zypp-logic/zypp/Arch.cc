/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/Arch.cc
 *
*/


extern "C"
{
#include <features.h>
#include <sys/utsname.h>
#if __GLIBC_PREREQ (2,16)
#include <sys/auxv.h>	// getauxval for PPC64P7 detection
#endif
#include <unistd.h>
}

#include <iostream>
#include <list>
#include <fstream>

#include <zypp-core/base/IOStream.h>
#include <zypp-core/base/Logger.h>
#include <zypp-core/base/Exception.h>
#include <zypp-core/base/NonCopyable.h>
#include <zypp-core/base/Hash.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp/Arch.h>
#include <zypp/Bit.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  namespace {
    // From rpm's lib/rpmrc.c
#   if defined(__linux__) && defined(__x86_64__)
    inline void cpuid(uint32_t op, uint32_t op2, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
    {
        asm volatile (
            "cpuid\n"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "a" (op), "c" (op2));
    }

    /* From gcc's gcc/config/i386/cpuid.h */
    /* Features (%eax == 1) */
    /* %ecx */
    #define bit_SSE3        (1 << 0)
    #define bit_SSSE3       (1 << 9)
    #define bit_FMA         (1 << 12)
    #define bit_CMPXCHG16B  (1 << 13)
    #define bit_SSE4_1      (1 << 19)
    #define bit_SSE4_2      (1 << 20)
    #define bit_MOVBE       (1 << 22)
    #define bit_POPCNT      (1 << 23)
    #define bit_OSXSAVE     (1 << 27)
    #define bit_AVX         (1 << 28)
    #define bit_F16C        (1 << 29)

    /* Extended Features (%eax == 0x80000001) */
    /* %ecx */
    #define bit_LAHF_LM     (1 << 0)
    #define bit_LZCNT       (1 << 5)

    /* Extended Features (%eax == 7) */
    /* %ebx */
    #define bit_BMI         (1 << 3)
    #define bit_AVX2        (1 << 5)
    #define bit_BMI2        (1 << 8)
    #define bit_AVX512F     (1 << 16)
    #define bit_AVX512DQ    (1 << 17)
    #define bit_AVX512CD    (1 << 28)
    #define bit_AVX512BW    (1 << 30)
    #define bit_AVX512VL    (1u << 31)

    int get_x86_64_level(void)
    {
        int level = 1;

        unsigned int op_1_ecx = 0, op_80000001_ecx = 0, op_7_ebx = 0, unused = 0;
        cpuid(1, 0, &unused, &unused, &op_1_ecx, &unused);
        cpuid(0x80000001, 0, &unused, &unused, &op_80000001_ecx, &unused);
        cpuid(7, 0, &unused, &op_7_ebx, &unused, &unused);

        const unsigned int op_1_ecx_lv2 = bit_SSE3 | bit_SSSE3 | bit_CMPXCHG16B | bit_SSE4_1 | bit_SSE4_2 | bit_POPCNT;
        if ((op_1_ecx & op_1_ecx_lv2) == op_1_ecx_lv2 && (op_80000001_ecx & bit_LAHF_LM))
            level = 2;

        const unsigned int op_1_ecx_lv3 = bit_FMA | bit_MOVBE | bit_OSXSAVE | bit_AVX | bit_F16C;
        const unsigned int op_7_ebx_lv3 = bit_BMI | bit_AVX2 | bit_BMI2;
        if (level == 2 && (op_1_ecx & op_1_ecx_lv3) == op_1_ecx_lv3 && (op_7_ebx & op_7_ebx_lv3) == op_7_ebx_lv3
            && (op_80000001_ecx & bit_LZCNT))
            level = 3;

        const unsigned int op_7_ebx_lv4 = bit_AVX512F | bit_AVX512DQ | bit_AVX512CD | bit_AVX512BW | bit_AVX512VL;
        if (level == 3 && (op_7_ebx & op_7_ebx_lv4) == op_7_ebx_lv4)
            level = 4;

        return level;
    }
#   endif

    Arch _autoDetectSystemArchitecture() {
      struct ::utsname buf;
      if ( ::uname( &buf ) < 0 )
      {
        ERR << "Can't determine system architecture" << endl;
        return Arch_noarch;
      }

      Arch architecture( buf.machine );
      MIL << "Uname architecture is '" << buf.machine << "'" << endl;

      if ( architecture == Arch_x86_64 )
      {
  #if     defined(__linux__) && defined(__x86_64__)
        switch ( get_x86_64_level() )
        {
          case 2:
            architecture = Arch_x86_64_v2;
            WAR << "CPU has 'x86_64': architecture upgraded to '" << architecture << "'" << endl;
            break;
          case 3:
            architecture = Arch_x86_64_v3;
            WAR << "CPU has 'x86_64': architecture upgraded to '" << architecture << "'" << endl;
            break;
          case 4:
            architecture = Arch_x86_64_v4;
            WAR << "CPU has 'x86_64': architecture upgraded to '" << architecture << "'" << endl;
            break;
        }
  #       endif
      }
      else if ( architecture == Arch_i686 )
      {
        // some CPUs report i686 but dont implement cx8 and cmov
        // check for both flags in /proc/cpuinfo and downgrade
        // to i586 if either is missing (cf bug #18885)
        std::ifstream cpuinfo( "/proc/cpuinfo" );
        if ( cpuinfo )
        {
          for( iostr::EachLine in( cpuinfo ); in; in.next() )
          {
            if ( str::hasPrefix( *in, "flags" ) )
            {
              if (    in->find( "cx8" ) == std::string::npos
                   || in->find( "cmov" ) == std::string::npos )
              {
                architecture = Arch_i586;
                WAR << "CPU lacks 'cx8' or 'cmov': architecture downgraded to '" << architecture << "'" << endl;
              }
              break;
            }
          }
        }
        else
        {
          ERR << "Cant open " << PathInfo("/proc/cpuinfo") << endl;
        }
      }
      else if ( architecture == Arch_sparc || architecture == Arch_sparc64 )
      {
        // Check for sun4[vum] to get the real arch. (bug #566291)
        std::ifstream cpuinfo( "/proc/cpuinfo" );
        if ( cpuinfo )
        {
          for( iostr::EachLine in( cpuinfo ); in; in.next() )
          {
            if ( str::hasPrefix( *in, "type" ) )
            {
              if ( in->find( "sun4v" ) != std::string::npos )
              {
                architecture = ( architecture == Arch_sparc64 ? Arch_sparc64v : Arch_sparcv9v );
                WAR << "CPU has 'sun4v': architecture upgraded to '" << architecture << "'" << endl;
              }
              else if ( in->find( "sun4u" ) != std::string::npos )
              {
                architecture = ( architecture == Arch_sparc64 ? Arch_sparc64 : Arch_sparcv9 );
                WAR << "CPU has 'sun4u': architecture upgraded to '" << architecture << "'" << endl;
              }
              else if ( in->find( "sun4m" ) != std::string::npos )
              {
                architecture = Arch_sparcv8;
                WAR << "CPU has 'sun4m': architecture upgraded to '" << architecture << "'" << endl;
              }
              break;
            }
          }
        }
        else
        {
          ERR << "Cant open " << PathInfo("/proc/cpuinfo") << endl;
        }
      }
      else if ( architecture == Arch_armv8l || architecture == Arch_armv7l || architecture == Arch_armv6l )
      {
        std::ifstream platform( "/etc/rpm/platform" );
        if (platform)
        {
          for( iostr::EachLine in( platform ); in; in.next() )
          {
            if ( str::hasPrefix( *in, "armv8hl-" ) )
            {
              architecture = Arch_armv8hl;
              WAR << "/etc/rpm/platform contains armv8hl-: architecture upgraded to '" << architecture << "'" << endl;
              break;
            }
            if ( str::hasPrefix( *in, "armv7hl-" ) )
            {
              architecture = Arch_armv7hl;
              WAR << "/etc/rpm/platform contains armv7hl-: architecture upgraded to '" << architecture << "'" << endl;
              break;
            }
            if ( str::hasPrefix( *in, "armv6hl-" ) )
            {
              architecture = Arch_armv6hl;
              WAR << "/etc/rpm/platform contains armv6hl-: architecture upgraded to '" << architecture << "'" << endl;
              break;
            }
          }
        }
      }
  #if __GLIBC_PREREQ (2,16)
      else if ( architecture == Arch_ppc64 )
      {
        const char * platform = (const char *)getauxval( AT_PLATFORM );
        int powerlvl = 0;
        if ( platform && sscanf( platform, "power%d", &powerlvl ) == 1 && powerlvl > 6 )
          architecture = Arch_ppc64p7;
      }
  #endif
      return architecture;
    }

  }



  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Arch::CompatEntry
  //
  /** Holds an architecture ID and its compatible relation.
   * An architecture is compatibleWith, if its _idBit is set in
   * _compatBits. noarch has ID 0, non-builtin archs have ID 1 and
   * have to be treated specially.
  */
  struct Arch::CompatEntry
  {
    /** Bitfield for architecture IDs and compatBits relation.
     * \note Need one bit for each builtin Arch.
     * \todo Migrate to some infinite BitField
    */
    using CompatBits = bit::BitField<uint64_t>;

    CompatEntry( const std::string & archStr_r,
                 CompatBits::IntT idBit_r = 1 )
    : _idStr( archStr_r )
    , _archStr( archStr_r )
    , _idBit( idBit_r )
    , _compatBits( idBit_r )
    {}

    CompatEntry( IdString archStr_r,
                 CompatBits::IntT idBit_r = 1 )
    : _idStr( archStr_r )
    , _archStr( archStr_r.asString() )
    , _idBit( idBit_r )
    , _compatBits( idBit_r )
    {}

    void addCompatBit( const CompatBits & idBit_r ) const
    {
      if ( idBit_r && ! (_compatBits & idBit_r) )
        {
          _compatBits |= idBit_r;
        }
    }

    /** Return whether \c this is compatible with \a targetEntry_r.*/
    bool compatibleWith( const CompatEntry & targetEntry_r ) const
    {
      switch ( _idBit.value() )
        {
        case 0:
          // this is noarch and always comatible
          return true;
          break;
        case 1:
          // this is a non builtin: self compatible only
          return _archStr == targetEntry_r._archStr;
          break;
        }
      // This is a builtin: compatible if mentioned in targetEntry_r
      return bool( targetEntry_r._compatBits & _idBit );
    }

    /** compare by score, then archStr. */
    int compare( const CompatEntry & rhs ) const
    {
      if ( _idBit.value() != rhs. _idBit.value() )
        return( _idBit.value() < rhs. _idBit.value() ? -1 : 1 );
      return _archStr.compare( rhs._archStr ); // Id 1: non builtin
    }

    bool isBuiltIn() const
    { return( _idBit != CompatBits(1) ); }

    IdString::IdType id() const
    { return _idStr.id(); }

    IdString            _idStr;
    std::string         _archStr; // frequently used by the UI so we keep a reference
    CompatBits          _idBit;
    mutable CompatBits  _compatBits;
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates Arch::CompatEntry Stream output */
  inline std::ostream & operator<<( std::ostream & str, const Arch::CompatEntry & obj )
  {
    Arch::CompatEntry::CompatBits bit( obj._idBit );
    unsigned bitnum = 0;
    while ( bit )
    {
      ++bitnum;
      bit >>= 1;
    }
    return str << str::form( "%-15s ", obj._archStr.c_str() ) << str::numstring(bitnum,2) << ' '
               << obj._compatBits << ' ' << obj._compatBits.value();
  }

  /** \relates Arch::CompatEntry */
  inline bool operator==( const Arch::CompatEntry & lhs, const Arch::CompatEntry & rhs )
  { return lhs._idStr == rhs._idStr; }
  /** \relates Arch::CompatEntry */
  inline bool operator!=( const Arch::CompatEntry & lhs, const Arch::CompatEntry & rhs )
  { return ! ( lhs == rhs ); }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////

ZYPP_DEFINE_ID_HASHABLE( zypp::Arch::CompatEntry );

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  // Builtin architecture STRING VALUES to be
  // used in defCompatibleWith below!
  //
  // const IdString  _foo( "foo" );
  // const Arch Arch_foo( _foo() );
  //
  // NOTE: Builtin CLASS Arch CONSTANTS are defined below.
  //       You have to change them accordingly in Arch.h.
  //
  // NOTE: Thake care CompatBits::IntT is able to provide one
  //       bit for each architecture.
  //
  #define DEF_BUILTIN(A) \
  namespace { static inline const IdString & a_##A () { static IdString _str(#A); return _str; } } \
  const Arch Arch_##A( a_##A() )

  DEF_BUILTIN( noarch );

  DEF_BUILTIN( i386 );
  DEF_BUILTIN( i486 );
  DEF_BUILTIN( i586 );
  DEF_BUILTIN( i686 );
  DEF_BUILTIN( athlon );
  DEF_BUILTIN( x86_64 );
  DEF_BUILTIN( x86_64_v2 );
  DEF_BUILTIN( x86_64_v3 );
  DEF_BUILTIN( x86_64_v4 );

  DEF_BUILTIN( pentium3 );
  DEF_BUILTIN( pentium4 );

  DEF_BUILTIN( s390 );
  DEF_BUILTIN( s390x );

  DEF_BUILTIN( ppc );
  DEF_BUILTIN( ppc64 );
  DEF_BUILTIN( ppc64p7 );

  DEF_BUILTIN( ppc64le );

  DEF_BUILTIN( ia64 );

  DEF_BUILTIN( alphaev67 );
  DEF_BUILTIN( alphaev6 );
  DEF_BUILTIN( alphapca56 );
  DEF_BUILTIN( alphaev56 );
  DEF_BUILTIN( alphaev5 );
  DEF_BUILTIN( alpha );

  DEF_BUILTIN( sparc64v );
  DEF_BUILTIN( sparcv9v );
  DEF_BUILTIN( sparc64 );
  DEF_BUILTIN( sparcv9 );
  DEF_BUILTIN( sparcv8 );
  DEF_BUILTIN( sparc );

  DEF_BUILTIN( aarch64 );

  DEF_BUILTIN( armv7tnhl );	/* exists? */
  DEF_BUILTIN( armv7thl );	/* exists? */

  DEF_BUILTIN( armv7hnl );	/* legacy: */DEF_BUILTIN( armv7nhl );
  DEF_BUILTIN( armv8hl );
  DEF_BUILTIN( armv7hl );
  DEF_BUILTIN( armv6hl );

  DEF_BUILTIN( armv8l );
  DEF_BUILTIN( armv7l );
  DEF_BUILTIN( armv6l );
  DEF_BUILTIN( armv5tejl );
  DEF_BUILTIN( armv5tel );
  DEF_BUILTIN( armv5tl );
  DEF_BUILTIN( armv5l );
  DEF_BUILTIN( armv4tl );
  DEF_BUILTIN( armv4l );
  DEF_BUILTIN( armv3l );

  DEF_BUILTIN( riscv64 );

  DEF_BUILTIN( sh3 );

  DEF_BUILTIN( sh4 );
  DEF_BUILTIN( sh4a );

  DEF_BUILTIN( m68k );

  DEF_BUILTIN( mips );
  DEF_BUILTIN( mipsel );
  DEF_BUILTIN( mips64 );
  DEF_BUILTIN( mips64el );

  DEF_BUILTIN( loong64 );
#undef DEF_BUILTIN

  ///////////////////////////////////////////////////////////////////
  namespace
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : CompatSet
    //
    /** Maintain architecture compatibility (Singleton by the way it is used).
     *
     * Povides \ref Arch::CompatEntry for \ref Arch. Defines the
     * compatibleWith relation.
     * \li \c noarch has _idBit 0
     * \li \c nonbuiltin archs have _idBit 1
    */
    struct ArchCompatSet : private base::NonCopyable
    {
      using CompatEntry = Arch::CompatEntry;
      using CompatBits = CompatEntry::CompatBits;

      using Set = std::unordered_set<CompatEntry>;
      using iterator = Set::iterator;
      using const_iterator = Set::const_iterator;

      /** Singleton access. */
      static ArchCompatSet & instance()
      {
        static ArchCompatSet _instance;
        return _instance;
      }

      /** Return the entry related to \a archStr_r.
       * Creates an entry for nonbuiltin archs.
      */
      const Arch::CompatEntry & assertDef( const std::string & archStr_r )
      { return *_compatSet.insert( Arch::CompatEntry( archStr_r ) ).first; }
      /** \overload */
      const Arch::CompatEntry & assertDef( IdString archStr_r )
      { return *_compatSet.insert( Arch::CompatEntry( archStr_r ) ).first; }

      const_iterator begin() const
      { return _compatSet.begin(); }

      const_iterator end() const
      { return _compatSet.end(); }

      struct DumpOnCompare
      {
        int operator()( const CompatEntry & lhs,  const CompatEntry & rhs ) const
        { return lhs._idBit.value() < rhs._idBit.value(); }
      };

      std::ostream & dumpOn( std::ostream & str ) const
      {
        str << "ArchCompatSet:";
        std::list<CompatEntry> ov( _compatSet.begin(), _compatSet.end() );
        ov.sort( DumpOnCompare() );
        for_( it, ov.begin(), ov.end() )
          {
            str << endl << ' ' << *it;
          }
        return str;
      }

    private:
      /** Singleton ctor. */
      ArchCompatSet()
      {
        // _noarch must have _idBit 0.
        // Other builtins have 1-bit set
        // and are initialized done on the fly.
        _compatSet.insert( Arch::CompatEntry( a_noarch(), 0 ) );
        ///////////////////////////////////////////////////////////////////
        // Define the CompatibleWith relation:
        //
        // NOTE: Order of definition is significant! (Arch::compare)
        //       - define compatible (less) architectures first!
        //
        defCompatibleWith( a_i386(),		a_noarch() );
        defCompatibleWith( a_i486(),		a_noarch(),a_i386() );
        defCompatibleWith( a_i586(),		a_noarch(),a_i386(),a_i486() );
        defCompatibleWith( a_i686(),		a_noarch(),a_i386(),a_i486(),a_i586() );
        defCompatibleWith( a_athlon(),		a_noarch(),a_i386(),a_i486(),a_i586(),a_i686() );
        defCompatibleWith( a_x86_64(),		a_noarch(),a_i386(),a_i486(),a_i586(),a_i686(),a_athlon() );
        defCompatibleWith( a_x86_64_v2(),	a_noarch(),a_i386(),a_i486(),a_i586(),a_i686(),a_athlon(),a_x86_64() );
        defCompatibleWith( a_x86_64_v3(),	a_noarch(),a_i386(),a_i486(),a_i586(),a_i686(),a_athlon(),a_x86_64(),a_x86_64_v2() );
        defCompatibleWith( a_x86_64_v4(),	a_noarch(),a_i386(),a_i486(),a_i586(),a_i686(),a_athlon(),a_x86_64(),a_x86_64_v2(),a_x86_64_v3() );

        defCompatibleWith( a_pentium3(),	a_noarch(),a_i386(),a_i486(),a_i586(),a_i686() );
        defCompatibleWith( a_pentium4(),	a_noarch(),a_i386(),a_i486(),a_i586(),a_i686(),a_pentium3() );

        defCompatibleWith( a_ia64(),		a_noarch(),a_i386(),a_i486(),a_i586(),a_i686() );
        //
        defCompatibleWith( a_s390(),		a_noarch() );
        defCompatibleWith( a_s390x(),		a_noarch(),a_s390() );
        //
        defCompatibleWith( a_ppc(),		a_noarch() );
        defCompatibleWith( a_ppc64(),		a_noarch(),a_ppc() );
        defCompatibleWith( a_ppc64p7(),		a_noarch(),a_ppc(),a_ppc64() );
        //
        defCompatibleWith( a_ppc64le(),		a_noarch() );
        //
        defCompatibleWith( a_alpha(),		a_noarch() );
        defCompatibleWith( a_alphaev5(),	a_noarch(),a_alpha() );
        defCompatibleWith( a_alphaev56(),	a_noarch(),a_alpha(),a_alphaev5() );
        defCompatibleWith( a_alphapca56(),	a_noarch(),a_alpha(),a_alphaev5(),a_alphaev56() );
        defCompatibleWith( a_alphaev6(),	a_noarch(),a_alpha(),a_alphaev5(),a_alphaev56(),a_alphapca56() );
        defCompatibleWith( a_alphaev67(),	a_noarch(),a_alpha(),a_alphaev5(),a_alphaev56(),a_alphapca56(),a_alphaev6() );
        //
        defCompatibleWith( a_sparc(),		a_noarch() );
        defCompatibleWith( a_sparcv8(),		a_noarch(),a_sparc() );
        defCompatibleWith( a_sparcv9(),		a_noarch(),a_sparc(),a_sparcv8() );
        defCompatibleWith( a_sparcv9v(),	a_noarch(),a_sparc(),a_sparcv8(),a_sparcv9() );
        //
        defCompatibleWith( a_sparc64(),		a_noarch(),a_sparc(),a_sparcv8(),a_sparcv9() );
        defCompatibleWith( a_sparc64v(),	a_noarch(),a_sparc(),a_sparcv8(),a_sparcv9(),a_sparcv9v(),a_sparc64() );
        //
        defCompatibleWith( a_armv3l(),		a_noarch() );
        defCompatibleWith( a_armv4l(),		a_noarch(),a_armv3l() );
        defCompatibleWith( a_armv4tl(),		a_noarch(),a_armv3l(),a_armv4l() );
        defCompatibleWith( a_armv5l(),		a_noarch(),a_armv3l(),a_armv4l(),a_armv4tl() );
        defCompatibleWith( a_armv5tl(),		a_noarch(),a_armv3l(),a_armv4l(),a_armv4tl(),a_armv5l() );
        defCompatibleWith( a_armv5tel(),	a_noarch(),a_armv3l(),a_armv4l(),a_armv4tl(),a_armv5l(),a_armv5tl() );
        defCompatibleWith( a_armv5tejl(),	a_noarch(),a_armv3l(),a_armv4l(),a_armv4tl(),a_armv5l(),a_armv5tl(),a_armv5tel() );
        defCompatibleWith( a_armv6l(),		a_noarch(),a_armv3l(),a_armv4l(),a_armv4tl(),a_armv5l(),a_armv5tl(),a_armv5tel(),a_armv5tejl() );
        defCompatibleWith( a_armv7l(),		a_noarch(),a_armv3l(),a_armv4l(),a_armv4tl(),a_armv5l(),a_armv5tl(),a_armv5tel(),a_armv5tejl(),a_armv6l() );
        defCompatibleWith( a_armv8l(),		a_noarch(),a_armv3l(),a_armv4l(),a_armv4tl(),a_armv5l(),a_armv5tl(),a_armv5tel(),a_armv5tejl(),a_armv6l(),a_armv7l() );

        defCompatibleWith( a_armv6hl(),		a_noarch() );
        defCompatibleWith( a_armv7hl(),		a_noarch(),a_armv6hl() );
        defCompatibleWith( a_armv8hl(),		a_noarch(),a_armv7hl() );
        defCompatibleWith( a_armv7hnl(),	a_noarch(),a_armv7hl(),a_armv6hl() );
        /*legacy: rpm uses v7hnl */
        defCompatibleWith( a_armv7nhl(),	a_noarch(),a_armv7hnl(),a_armv7hl(),a_armv6hl() );

        /*?*/defCompatibleWith( a_armv7thl(),	a_noarch(),a_armv7hl() );
        /*?*/defCompatibleWith( a_armv7tnhl(),	a_noarch(),a_armv7hl(),a_armv7nhl(),a_armv7thl() );

        defCompatibleWith( a_aarch64(),		a_noarch() );
        //
        defCompatibleWith( a_riscv64(),		a_noarch() );
        //
        defCompatibleWith( a_sh3(),		a_noarch() );
        //
        defCompatibleWith( a_sh4(),		a_noarch() );
        defCompatibleWith( a_sh4a(),		a_noarch(),a_sh4() );

        defCompatibleWith( a_m68k(),		a_noarch() );

        defCompatibleWith( a_mips(),		a_noarch() );
        defCompatibleWith( a_mipsel(),		a_noarch() );
        defCompatibleWith( a_mips64(),		a_noarch() );
        defCompatibleWith( a_mips64el(),	a_noarch() );

        defCompatibleWith( a_loong64(),		a_noarch() );
        //
        ///////////////////////////////////////////////////////////////////
        // dumpOn( USR ) << endl;
      }

    private:
      /** Return the next avialable _idBit.
       * Ctor injects _noarch into the _compatSet, 1 is for
       * nonbuiltin archs, so we can use <tt>size</tt> for
       * buitin archs.
      */
      CompatBits::IntT nextIdBit() const
      {
        if ( CompatBits::size == _compatSet.size() )
        {
          // Provide more bits in CompatBits::IntT
          INT << "Need more than " << CompatBits::size << " bits to encode architectures." << endl;
          ZYPP_THROW( Exception("Need more bits to encode architectures.") );
        }
        CompatBits::IntT nextBit = CompatBits::IntT(1) << (_compatSet.size());
        return nextBit;
      }

      /** Assert each builtin Arch gets an unique _idBit when
       *  inserted into the _compatSet.
      */
      const CompatEntry & assertCompatSetEntry( IdString archStr_r )
      { return *_compatSet.insert( Arch::CompatEntry( archStr_r, nextIdBit() ) ).first; }

      /** Initialize builtin Archs and set _compatBits.
      */
      void defCompatibleWith( IdString targetArch_r,
                              IdString arch0_r,
                              IdString arch1_r = IdString(),
                              IdString arch2_r = IdString(),
                              IdString arch3_r = IdString(),
                              IdString arch4_r = IdString(),
                              IdString arch5_r = IdString(),
                              IdString arch6_r = IdString(),
                              IdString arch7_r = IdString(),
                              IdString arch8_r = IdString(),
                              IdString arch9_r = IdString() )
      {
        const CompatEntry & target( assertCompatSetEntry( targetArch_r ) );
        target.addCompatBit( assertCompatSetEntry( arch0_r )._idBit );
#define SETARG(N) if ( arch##N##_r.empty() ) return; target.addCompatBit( assertCompatSetEntry( arch##N##_r )._idBit )
        SETARG(1); SETARG(2); SETARG(3); SETARG(4);
        SETARG(5); SETARG(6); SETARG(7); SETARG(8); SETARG(9);
#undef SETARG
      }

    private:
      Set _compatSet;
    };

    /////////////////////////////////////////////////////////////////
  } // namespace
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Arch
  //
  ///////////////////////////////////////////////////////////////////

  const Arch Arch_empty ( IdString::Empty );
  // remaining Arch_* constants are defined by DEF_BUILTIN above.

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Arch::Arch
  //	METHOD TYPE : Ctor
  //
  Arch::Arch()
  : _entry( &ArchCompatSet::instance().assertDef( a_noarch() ) )
  {}

  Arch::Arch( IdString::IdType id_r )
  : _entry( &ArchCompatSet::instance().assertDef( IdString(id_r) ) )
  {}

  Arch::Arch( const IdString & idstr_r )
  : _entry( &ArchCompatSet::instance().assertDef( idstr_r ) )
  {}

  Arch::Arch( const std::string & str_r )
  : _entry( &ArchCompatSet::instance().assertDef( str_r ) )
  {}

  Arch::Arch( const char * cstr_r )
  : _entry( &ArchCompatSet::instance().assertDef( cstr_r ) )
  {}

  Arch Arch::detectSystemArchitecture()
  {
    static Arch _val( _autoDetectSystemArchitecture() );
    return _val;
  }

  Arch::Arch( const CompatEntry & rhs )
  : _entry( &rhs )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Arch::idStr
  //	METHOD TYPE : IdString
  //
  IdString Arch::idStr() const
  { return _entry->_idStr; }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Arch::asString
  //	METHOD TYPE : const std::string &
  //
  const std::string & Arch::asString() const
  { return _entry->_archStr; }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Arch::isBuiltIn
  //	METHOD TYPE : bool
  //
  bool Arch::isBuiltIn() const
  { return _entry->isBuiltIn(); }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Arch::compatibleWith
  //	METHOD TYPE : bool
  //
  bool Arch::compatibleWith( const Arch & targetArch_r ) const
  { return _entry->compatibleWith( *targetArch_r._entry ); }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Arch::baseArch
  //	METHOD TYPE : Arch
  //
  Arch Arch::baseArch( ) const
  {
    // check the multilib archs:
    if (Arch_x86_64.compatibleWith(*this))
    {
      return Arch_x86_64;
    }
    if (Arch_sparc64v.compatibleWith(*this))
    {
      return Arch_sparc64v;
    }
    if (Arch_sparc64.compatibleWith(*this))
    {
      return Arch_sparc64;
    }
    if (Arch_ppc64.compatibleWith(*this))
    {
      return Arch_ppc64;
    }
    if (Arch_s390x.compatibleWith(*this))
    {
      return Arch_s390x;
    }
    // Here: no multilib; return arch before noarch
    CompatSet cset( compatSet( *this ) );
    if ( cset.size() > 2 )	// systemArchitecture, ..., basearch, noarch
    {
      return *(++cset.rbegin());
    }
    return *this;
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Arch::compare
  //	METHOD TYPE : bool
  //
  int Arch::compare( const Arch & rhs ) const
  { return _entry->compare( *rhs._entry ); }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Arch::compatSet
  //	METHOD TYPE : Arch::CompatSet
  //
  Arch::CompatSet Arch::compatSet( const Arch & targetArch_r )
  {
    Arch::CompatSet ret;

    for ( ArchCompatSet::const_iterator it = ArchCompatSet::instance().begin();
          it != ArchCompatSet::instance().end(); ++it )
      {
        if ( it->compatibleWith( *targetArch_r._entry ) )
          {
            ret.insert( Arch(*it) );
          }
      }

    return ret;
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
