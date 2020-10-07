/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/**
 * \file	zypp/ui/UserWantedPackages.cc
 *
 *  \author	Stefan Hundhammer <sh@suse.de>
 *
 */
#include <iostream>
#include <zypp/base/Logger.h>

#include <zypp/ui/UserWantedPackages.h>

#include <zypp/base/PtrTypes.h>
#include <zypp/ui/Selectable.h>

#include <zypp/ResObjects.h>
#include <zypp/ZYppFactory.h>
#include <zypp/ResPoolProxy.h>


using std::string;
using std::set;
using std::endl;


namespace zypp
{
    namespace ui
    {
	typedef ResPoolProxy::const_iterator	PoolProxyIterator;

        static inline ResPoolProxy		poolProxy()	{ return getZYpp()->poolProxy();	}

	template<class T> PoolProxyIterator poolProxyBegin()	{ return poolProxy().byKindBegin<T>();	}
	template<class T> PoolProxyIterator poolProxyEnd()	{ return poolProxy().byKindEnd<T>();	}

	static inline PoolProxyIterator pkgBegin()		{ return poolProxyBegin<Package>();	}
	static inline PoolProxyIterator pkgEnd()		{ return poolProxyEnd<Package>();	}

	static inline PoolProxyIterator patchesBegin()		{ return poolProxyBegin<Patch>();	}
	static inline PoolProxyIterator patchesEnd()		{ return poolProxyEnd<Patch>();		}

	template<typename T> bool contains( const std::set<T> & container, T search )
	{
	    return container.find( search ) != container.end();
	}



	static void addDirectlySelectedPackages	( set<string> & pkgNames );
        template<class PkgSet_T> void addPkgSetPackages( set<string> & pkgNames );

	static void addPatchPackages		( set<string> & pkgNames );



	set<string> userWantedPackageNames()
	{
	    set<string> pkgNames;

	    DBG << "Collecting packages the user explicitly asked for" << endl;

	    addDirectlySelectedPackages	( pkgNames );
	    addPatchPackages		( pkgNames );

	    return pkgNames;
	}



	static void addDirectlySelectedPackages( set<string> & pkgNames )
	{
	    for ( PoolProxyIterator it = pkgBegin();
		  it != pkgEnd();
		  ++it )
	    {
		// Add all packages the user wanted to transact directly,
		// no matter what the transaction is (install, update, delete)

		if ( (*it)->toModify() && (*it)->modifiedBy() == ResStatus::USER )
		{
		    DBG << "Explicit user transaction on pkg \"" << (*it)->name() << "\"" << endl;

		    pkgNames.insert( (*it)->name() );
		}
	    }
	}


        static void addPatchPackages( set<string> & pkgNames )
        {
            for ( PoolProxyIterator patch_it = patchesBegin();
                  patch_it != patchesEnd();
                  ++patch_it )
            {
                Patch::constPtr patch = dynamic_pointer_cast<const Patch>( (*patch_it)->theObj() ? (*patch_it)->theObj().resolvable() : 0 );

                if ( patch && (*patch_it)->toModify() )
		{
		    DBG << "Patch will be transacted: \"" << patch->name()
			<< "\" - \"" << patch->summary() << "\"" << endl;

                    Patch::Contents contents( patch->contents() );
                    for_( it, contents.begin(), contents.end() )
                    {
                      pkgNames.insert( it->name() );
                    }
		}
	    }
	}

    } // namespace ui
} // namespace zypp
