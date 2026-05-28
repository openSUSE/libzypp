/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file  zypp/ng/sat/solvincludes.h
 *
 * Wrapper header that pulls in all required libsolv C headers inside an
 * extern "C" block.  Must be #include'd from a Global Module Fragment (GMF)
 * in .cppm files — GCC requires GMF content to arrive via preprocessor
 * inclusion, not inline source.
 */
#ifndef ZYPPNG_SAT_SOLVINCLUDES_H
#define ZYPPNG_SAT_SOLVINCLUDES_H

extern "C"
{
#include <solv/pool.h>
#include <solv/repo.h>
#include <solv/solvable.h>
#include <solv/poolarch.h>
#include <solv/repo_solv.h>
#include <solv/pool_parserpmrichdep.h>
#include <solv/knownid.h>
}

#endif
