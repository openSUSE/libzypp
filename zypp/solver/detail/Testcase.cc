/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file       zypp/solver/detail/Testcase.cc
 *
*/
#include <iostream>
#include <fstream>
#include <sstream>
#include <streambuf>

#define ZYPP_USE_RESOLVER_INTERNALS

#include "zypp/solver/detail/Testcase.h"
#include "zypp/base/Logger.h"
#include "zypp/base/LogControl.h"
#include "zypp/base/GzStream.h"
#include "zypp/base/String.h"
#include "zypp/base/PtrTypes.h"
#include "zypp/base/NonCopyable.h"
#include "zypp/base/ReferenceCounted.h"

#include "zypp/parser/xml/XmlEscape.h"

#include "zypp/ZConfig.h"
#include "zypp/PathInfo.h"
#include "zypp/ResPool.h"
#include "zypp/Repository.h"
#include "zypp/target/modalias/Modalias.h"

#include "zypp/sat/detail/PoolImpl.h"
#include "zypp/solver/detail/Resolver.h"
#include "zypp/solver/detail/SystemCheck.h"

using std::endl;

/////////////////////////////////////////////////////////////////////////
namespace zypp
{ ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  namespace solver
  { /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
    namespace detail
    { ///////////////////////////////////////////////////////////////////

#define TAB "\t"
#define TAB2 "\t\t"

using namespace zypp::str;

//---------------------------------------------------------------------------

inline std::string xml_escape( const std::string &text )
{
  return zypp::xml::escape(text);
}

inline std::string xml_tag_enclose( const std::string &text, const std::string &tag, bool escape = false )
{
  std::string result;
  result += "<" + tag + ">";

  if ( escape)
   result += xml_escape(text);
  else
   result += text;

  result += "</" + tag + ">";
  return result;
}

template<class T>
std::string helixXML( const T &obj ); //undefined

template<>
std::string helixXML( const Edition &edition )
{
    std::stringstream str;
    str << xml_tag_enclose(edition.version(), "version");
    if (!edition.release().empty())
	str << xml_tag_enclose(edition.release(), "release");
    if (edition.epoch() != Edition::noepoch)
	str << xml_tag_enclose(numstring(edition.epoch()), "epoch");
    return str.str();
}

template<>
std::string helixXML( const Arch &arch )
{
    std::stringstream str;
    str << xml_tag_enclose(arch.asString(), "arch");
    return str.str();
}

template<>
std::string helixXML( const Capability &cap )
{
    std::stringstream str;
    CapDetail detail = cap.detail();
    if (detail.isSimple()) {
	if (detail.isVersioned()) {
	    str << "<dep name='" << xml_escape(detail.name().asString()) << "'"
		<< " op='" << xml_escape(detail.op().asString()) << "'"
		<< " version='" <<  xml_escape(detail.ed().version()) << "'";
	    if (!detail.ed().release().empty())
		str << " release='" << xml_escape(detail.ed().release()) << "'";
	    if (detail.ed().epoch() != Edition::noepoch)
		str << " epoch='" << xml_escape(numstring(detail.ed().epoch())) << "'";
	    str << " />" << endl;
	} else {
	    str << "<dep name='" << xml_escape(cap.asString()) << "' />" << endl;
	}
    } else if (detail.isExpression()) {
	if (detail.capRel() == CapDetail::CAP_AND
	    && detail.lhs().detail().isNamed()
	    && detail.rhs().detail().isNamed()) {
	    // packageand dependency
	    str << "<dep name='packageand("
		<< IdString(detail.lhs().id()) << ":"
		<< IdString(detail.rhs().id()) << ")' />" << endl;
	} else if (detail.capRel() == CapDetail::CAP_NAMESPACE
	    && detail.lhs().id() == NAMESPACE_OTHERPROVIDERS) {
	    str << "<dep name='otherproviders("
		<< IdString(detail.rhs().id()) << ")' />" << endl;
	} else {
	    // modalias ?
	    IdString packageName;
	    if (detail.capRel() == CapDetail::CAP_AND) {
		packageName = IdString(detail.lhs().id());
		detail = detail.rhs().detail();
	    }
	    if (detail.capRel() == CapDetail::CAP_NAMESPACE
		&& detail.lhs().id() == NAMESPACE_MODALIAS) {
		str << "<dep name='modalias(";
		if (!packageName.empty())
		    str << packageName << ":";
		str << IdString(detail.rhs().id()) << ")' />" << endl;
	    } else {
		str << "<!--- ignoring '" << xml_escape(cap.asString()) << "' -->" << endl;
	    }
	}
    } else {
	str << "<!--- ignoring '" << xml_escape(cap.asString()) << "' -->" << endl;
    }

    return str.str();
}

template<>
std::string helixXML( const Capabilities &caps )
{
    std::stringstream str;
    Capabilities::const_iterator it = caps.begin();
    str << endl;
    for ( ; it != caps.end(); ++it)
    {
	str << TAB2 << helixXML((*it));
    }
    str << TAB;
    return str.str();
}

template<>
std::string helixXML( const CapabilitySet &caps )
{
    std::stringstream str;
    CapabilitySet::const_iterator it = caps.begin();
    str << endl;
    for ( ; it != caps.end(); ++it)
    {
	str << TAB2 << helixXML((*it));
    }
    str << TAB;
    return str.str();
}

inline std::string helixXML( const PoolItem & obj, Dep deptag_r )
{
  std::stringstream out;
  Capabilities caps( obj[deptag_r] );
  if ( ! caps.empty() )
    out << TAB << xml_tag_enclose(helixXML(caps), deptag_r.asString()) << endl;
  return out.str();
}

std::string helixXML( const PoolItem & item )
{
  std::stringstream str;
  str << "<" << item.kind() << ">" << endl;
  str << TAB << xml_tag_enclose( item.name(), "name", true ) << endl;
  str << TAB << xml_tag_enclose( item.vendor().asString(), "vendor", true ) << endl;
  str << TAB << xml_tag_enclose( item.buildtime().asSeconds(), "buildtime", true ) << endl;
  if ( isKind<Package>( item ) ) {
      str << TAB << "<history>" << endl << TAB << "<update>" << endl;
      str << TAB2 << helixXML( item.arch() ) << endl;
      str << TAB2 << helixXML( item.edition() ) << endl;
      str << TAB << "</update>" << endl << TAB << "</history>" << endl;
  } else {
      str << TAB << helixXML( item.arch() ) << endl;
      str << TAB << helixXML( item.edition() ) << endl;
  }
  str << helixXML( item, Dep::PROVIDES );
  str << helixXML( item, Dep::PREREQUIRES );
  str << helixXML( item, Dep::CONFLICTS );
  str << helixXML( item, Dep::OBSOLETES );
  str << helixXML( item, Dep::REQUIRES );
  str << helixXML( item, Dep::RECOMMENDS );
  str << helixXML( item, Dep::ENHANCES );
  str << helixXML( item, Dep::SUPPLEMENTS );
  str << helixXML( item, Dep::SUGGESTS );

  str << "</" << item.kind() << ">" << endl;
  return str.str();
}

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : HelixResolvable
/**
 * Creates a file in helix format which includes all available
 * or installed packages,patches,selections.....
 **/
class  HelixResolvable : public base::ReferenceCounted, private base::NonCopyable{

  private:
    std::string dumpFile; // Path of the generated testcase
    ofgzstream *file;

  public:
    HelixResolvable (const std::string & path);
    ~HelixResolvable ();

    void addResolvable (const PoolItem item)
    { *file << helixXML (item); }

    std::string filename ()
    { return dumpFile; }
};

DEFINE_PTR_TYPE(HelixResolvable);
IMPL_PTR_TYPE(HelixResolvable);

typedef std::map<Repository, HelixResolvable_Ptr> RepositoryTable;

HelixResolvable::HelixResolvable(const std::string & path)
    :dumpFile (path)
{
    file = new ofgzstream(path.c_str());
    if (!file) {
	ZYPP_THROW (Exception( "Can't open " + path ) );
    }

    *file << "<channel><subchannel>" << endl;
}

HelixResolvable::~HelixResolvable()
{
    *file << "</subchannel></channel>" << endl;
    delete(file);
}

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : HelixControl
/**
 * Creates a file in helix format which contains all controll
 * action of a testcase ( file is known as *-test.xml)
 **/
class  HelixControl {

  private:
    std::string dumpFile; // Path of the generated testcase
    std::ofstream *file;
    bool _inSetup;

  public:
    HelixControl (const std::string & controlPath,
		  const RepositoryTable & sourceTable,
		  const Arch & systemArchitecture,
		  const target::Modalias::ModaliasList & modaliasList,
		  const std::set<std::string> & multiversionSpec,
		  const std::string & systemPath);
    ~HelixControl ();

    void closeSetup()
    {
      if ( _inSetup )
      {
	*file << "</setup>" << endl << "<trial>" << endl;
	_inSetup = false;
      }
    }

    std::ostream & writeTag()
    { return *file << (_inSetup ? TAB : ""); }

    void addTagIf( const std::string & tag_r, bool yesno_r = true )
    {
      if ( yesno_r )
	writeTag() << "<" << tag_r << "/>" << endl;
    }

    void installResolvable( const PoolItem & pi_r );
    void lockResolvable( const PoolItem & pi_r );
    void keepResolvable( const PoolItem & pi_r );
    void deleteResolvable( const PoolItem & pi_r );
    void addDependencies (const CapabilitySet &capRequire, const CapabilitySet &capConflict);
    void addUpgradeRepos( const std::set<Repository> & upgradeRepos_r );

    std::string filename () { return dumpFile; }
};

HelixControl::HelixControl(const std::string & controlPath,
			   const RepositoryTable & repoTable,
			   const Arch & systemArchitecture,
			   const target::Modalias::ModaliasList & modaliasList,
			   const std::set<std::string> & multiversionSpec,
			   const std::string & systemPath)
    :dumpFile (controlPath)
    ,_inSetup( true )
{
    file = new std::ofstream(controlPath.c_str());
    if (!file) {
	ZYPP_THROW (Exception( "Can't open " + controlPath ) );
    }

    *file << "<?xml version=\"1.0\"?>" << endl
	  << "<!-- libzypp resolver testcase -->" << endl
	  << "<test>" << endl
	  << "<setup arch=\"" << systemArchitecture << "\">" << endl
	  << TAB << "<system file=\"" << systemPath << "\"/>" << endl << endl;
    for ( RepositoryTable::const_iterator it = repoTable.begin();
	  it != repoTable.end(); ++it ) {
	RepoInfo repo = it->first.info();
	*file << TAB << "<!-- " << endl
	      << TAB << "- alias       : " << repo.alias() << endl;
	for ( RepoInfo::urls_const_iterator itUrl = repo.baseUrlsBegin();
	      itUrl != repo.baseUrlsEnd();
	      ++itUrl )
	{
	    *file << TAB << "- url         : " << *itUrl << endl;
	}
	*file << TAB << "- path        : " << repo.path() << endl;
	*file << TAB << "- type        : " << repo.type() << endl;
	*file << TAB << "- generated   : " << (it->first.generatedTimestamp()).form( "%Y-%m-%d %H:%M:%S" ) << endl;
	*file << TAB << "- outdated    : " << (it->first.suggestedExpirationTimestamp()).form( "%Y-%m-%d %H:%M:%S" ) << endl;
	*file << TAB << " -->" << endl;

	*file << TAB << "<channel file=\"" << str::numstring((long)it->first.id())
	      << "-package.xml.gz\" name=\"" << repo.alias() << "\""
	      << " priority=\"" << repo.priority()
	      << "\" />" << endl << endl;
    }

    // HACK: directly access sat::pool
    const sat::Pool & satpool( sat::Pool::instance() );

    // RequestedLocales
    const LocaleSet & addedLocales( satpool.getAddedRequestedLocales() );
    const LocaleSet & removedLocales( satpool.getRemovedRequestedLocales() );
    const LocaleSet & requestedLocales( satpool.getRequestedLocales() );

    for ( Locale l : requestedLocales )
    {
      const char * fate = ( addedLocales.count(l) ? "\" fate=\"added" : "" );
      *file << TAB << "<locale name=\"" << l << fate << "\" />" << endl;
    }
    for ( Locale l : removedLocales )
    {
      *file << TAB << "<locale name=\"" << l << "\" fate=\"removed\" />" << endl;
    }

    // AutoInstalled
    for ( IdString::IdType n : satpool.autoInstalled() )
    {
      *file << TAB << "<autoinst name=\"" << IdString(n) << "\" />" << endl;
    }



    for_( it, modaliasList.begin(), modaliasList.end() ) {
	*file << TAB << "<modalias name=\"" <<  xml_escape(*it)
	      << "\" />" << endl;
    }

    for_( it, multiversionSpec.begin(), multiversionSpec.end() ) {
	*file << TAB << "<multiversion name=\"" <<  *it
	      << "\" />" << endl;
    }

    // setup continued outside....
}

HelixControl::~HelixControl()
{
    closeSetup();	// in case it is still open
    *file << "</trial>" << endl
	  << "</test>" << endl;
    delete(file);
}

void HelixControl::installResolvable( const PoolItem & pi_r )
{
    *file << "<install channel=\"" << pi_r.repoInfo().alias() << "\""
          << " kind=\"" << pi_r.kind() << "\""
	  << " name=\"" << pi_r.name() << "\""
	  << " arch=\"" << pi_r.arch() << "\""
	  << " version=\"" << pi_r.edition().version() << "\""
	  << " release=\"" << pi_r.edition().release() << "\""
	  << " status=\"" << pi_r.status() << "\""
	  << "/>" << endl;
}

void HelixControl::lockResolvable( const PoolItem & pi_r )
{
    *file << "<lock channel=\"" << pi_r.repoInfo().alias() << "\""
          << " kind=\"" << pi_r.kind() << "\""
	  << " name=\"" << pi_r.name() << "\""
	  << " arch=\"" << pi_r.arch() << "\""
	  << " version=\"" << pi_r.edition().version() << "\""
	  << " release=\"" << pi_r.edition().release() << "\""
	  << " status=\"" << pi_r.status() << "\""
	  << "/>" << endl;
}

void HelixControl::keepResolvable( const PoolItem & pi_r )
{
    *file << "<keep channel=\"" << pi_r.repoInfo().alias() << "\""
          << " kind=\"" << pi_r.kind() << "\""
	  << " name=\"" << pi_r.name() << "\""
	  << " arch=\"" << pi_r.arch() << "\""
	  << " version=\"" << pi_r.edition().version() << "\""
	  << " release=\"" << pi_r.edition().release() << "\""
	  << " status=\"" << pi_r.status() << "\""
	  << "/>" << endl;
}

void HelixControl::deleteResolvable( const PoolItem & pi_r )
{
    *file << "<uninstall  kind=\"" << pi_r.kind() << "\""
	  << " name=\"" << pi_r.name() << "\""
	  << " status=\"" << pi_r.status() << "\""
	  << "/>" << endl;
}

void HelixControl::addDependencies (const CapabilitySet & capRequire, const CapabilitySet & capConflict)
{
    for (CapabilitySet::const_iterator iter = capRequire.begin(); iter != capRequire.end(); iter++) {
	*file << "<addRequire " <<  " name=\"" << iter->asString() << "\"" << "/>" << endl;
    }
    for (CapabilitySet::const_iterator iter = capConflict.begin(); iter != capConflict.end(); iter++) {
	*file << "<addConflict " << " name=\"" << iter->asString() << "\"" << "/>" << endl;
    }
}

void HelixControl::addUpgradeRepos( const std::set<Repository> & upgradeRepos_r )
{
  for_( it, upgradeRepos_r.begin(), upgradeRepos_r.end() )
  {
    *file << "<upgradeRepo name=\"" << it->alias() << "\"/>" << endl;
  }
}

//---------------------------------------------------------------------------

Testcase::Testcase()
    :dumpPath("/var/log/YaST2/solverTestcase")
{}

Testcase::Testcase(const std::string & path)
    :dumpPath(path)
{}

Testcase::~Testcase()
{}

bool Testcase::createTestcase(Resolver & resolver, bool dumpPool, bool runSolver)
{
    PathInfo path (dumpPath);

    if ( !path.isExist() ) {
	if (zypp::filesystem::assert_dir (dumpPath)!=0) {
	    ERR << "Cannot create directory " << dumpPath << endl;
	    return false;
	}
    } else {
	if (!path.isDir()) {
	    ERR << dumpPath << " is not a directory." << endl;
	    return false;
	}
	// remove old stuff if pool will be dump
	if (dumpPool)
	    zypp::filesystem::clean_dir (dumpPath);
    }

    if (runSolver) {
        zypp::base::LogControl::TmpLineWriter tempRedirect;
	zypp::base::LogControl::instance().logfile( dumpPath +"/y2log" );
	zypp::base::LogControl::TmpExcessive excessive;

	resolver.resolvePool();
    }

    ResPool pool 	= resolver.pool();
    RepositoryTable	repoTable;
    PoolItemList	items_to_install;
    PoolItemList 	items_to_remove;
    PoolItemList 	items_locked;
    PoolItemList 	items_keep;
    HelixResolvable_Ptr	system = NULL;

    if (dumpPool)
	system = new HelixResolvable(dumpPath + "/solver-system.xml.gz");

    for ( const PoolItem & pi : pool )
    {
	if ( system && pi.status().isInstalled() ) {
	    // system channel
	    system->addResolvable( pi );
	} else {
	    // repo channels
	    Repository repo  = pi.repository();
	    if (dumpPool) {
		if (repoTable.find (repo) == repoTable.end()) {
		    repoTable[repo] = new HelixResolvable(dumpPath + "/"
							  + str::numstring((long)repo.id())
							  + "-package.xml.gz");
		}
		repoTable[repo]->addResolvable( pi );
	    }
	}

	if ( pi.status().isToBeInstalled()
	     && !(pi.status().isBySolver())) {
	    items_to_install.push_back( pi );
	}
	if ( pi.status().isKept()
	     && !(pi.status().isBySolver())) {
	    items_keep.push_back( pi );
	}
	if ( pi.status().isToBeUninstalled()
	     && !(pi.status().isBySolver())) {
	    items_to_remove.push_back( pi );
	}
	if ( pi.status().isLocked()
	     && !(pi.status().isBySolver())) {
	    items_locked.push_back( pi );
	}
    }

    // writing control file "*-test.xml"
    HelixControl control (dumpPath + "/solver-test.xml",
			  repoTable,
			  ZConfig::instance().systemArchitecture(),
			  target::Modalias::instance().modaliasList(),
			  ZConfig::instance().multiversionSpec(),
			  "solver-system.xml.gz");

    // In <setup>: resolver flags,...
    control.writeTag() << "<focus value=\"" << resolver.focus() << "\"/>" << endl;

    control.addTagIf( "ignorealreadyrecommended",	resolver.ignoreAlreadyRecommended() );
    control.addTagIf( "onlyRequires",		resolver.onlyRequires() );
    control.addTagIf( "forceResolve",		resolver.forceResolve() );

    control.addTagIf( "cleandepsOnRemove",	resolver.cleandepsOnRemove() );

    control.addTagIf( "allowDowngrade",		resolver.allowDowngrade() );
    control.addTagIf( "allowNameChange",	resolver.allowNameChange() );
    control.addTagIf( "allowArchChange",	resolver.allowArchChange() );
    control.addTagIf( "allowVendorChange",	resolver.allowVendorChange() );

    control.addTagIf( "dupAllowDowngrade",	resolver.dupAllowDowngrade() );
    control.addTagIf( "dupAllowNameChange",	resolver.dupAllowNameChange() );
    control.addTagIf( "dupAllowArchChange",	resolver.dupAllowArchChange() );
    control.addTagIf( "dupAllowVendorChange",	resolver.dupAllowVendorChange() );

    control.closeSetup();
    // Entering <trial>...

    for ( const PoolItem & pi : items_to_install )
    { control.installResolvable( pi ); }

    for ( const PoolItem & pi : items_locked )
    { control.lockResolvable( pi ); }

    for ( const PoolItem & pi : items_keep )
    { control.keepResolvable( pi ); }

    for ( const PoolItem & pi : items_to_remove )
    { control.deleteResolvable( pi ); }

    control.addDependencies (resolver.extraRequires(), resolver.extraConflicts());
    control.addDependencies (SystemCheck::instance().requiredSystemCap(),
			     SystemCheck::instance().conflictSystemCap());
    control.addUpgradeRepos( resolver.upgradeRepos() );

    control.addTagIf( "distupgrade",	resolver.isUpgradeMode() );
    control.addTagIf( "update",	 	resolver.isUpdateMode() );
    control.addTagIf( "verify",	 	resolver.isVerifyingMode() );

    return true;
}


      ///////////////////////////////////////////////////////////////////
    };// namespace detail
    /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
  };// namespace solver
  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
};// namespace zypp
/////////////////////////////////////////////////////////////////////////
