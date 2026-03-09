/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/detail/PoolDefines.h
 *
*/
#ifndef ZYPP_SAT_DETAIL_POOLDEFINES_H
#define ZYPP_SAT_DETAIL_POOLDEFINES_H

extern "C"
{
  // Those s_Type names are exposed as sat::detail::CType below!
  struct s_Dataiterator;
  struct s_Datamatcher;
  struct s_Map;
  struct s_Pool;
  struct s_Queue;
  struct s_Repo;
  struct s_Solvable;
  struct s_Solver;
  struct s_Transaction;
}

namespace zypp::sat::detail {

  using CDataiterator = ::s_Dataiterator;	///< Wrapped libsolv C data type exposed as backdoor
  using CDatamatcher = ::s_Datamatcher;	///< Wrapped libsolv C data type exposed as backdoor
  using CMap = ::s_Map;		///< Wrapped libsolv C data type exposed as backdoor
  using CPool = ::s_Pool;		///< Wrapped libsolv C data type exposed as backdoor
  using CQueue = ::s_Queue;		///< Wrapped libsolv C data type exposed as backdoor
  using CRepo = ::s_Repo;		///< Wrapped libsolv C data type exposed as backdoor
  using CSolvable = ::s_Solvable;	///< Wrapped libsolv C data type exposed as backdoor
  using CSolver = ::s_Solver;	///< Wrapped libsolv C data type exposed as backdoor
  using CTransaction = ::s_Transaction;	///< Wrapped libsolv C data type exposed as backdoor

  /** Generic Id type. */
  using IdType = int;
  static const IdType noId( 0 );
  static const IdType emptyId( 1 );

  /** Internal ids satlib includes in dependencies.
   * MPL check in PoolImpl.cc
  */
  static const IdType solvablePrereqMarker( 15 );
  static const IdType solvableFileMarker  ( 16 );

  static const IdType namespaceModalias	( 18 );
  static const IdType namespaceLanguage	( 20 );
  static const IdType namespaceFilesystem	( 21 );

  /** Test for internal ids satlib includes in dependencies. */
  inline bool isDepMarkerId( IdType id_r )
  { return( id_r == solvablePrereqMarker || id_r == solvableFileMarker ); }

  /** Id type to connect \ref Solvable and sat-solvable.
   * Indext into solvable array.
  */
  using SolvableIdType = unsigned int;
  using size_type = SolvableIdType;
  /** Id to denote \ref Solvable::noSolvable. */
  static const SolvableIdType noSolvableId( 0 );
  /** Id to denote the usually hidden \ref Solvable::systemSolvable. */
  static const SolvableIdType systemSolvableId( 1 );

  /** Id type to connect \ref Repo and sat-repo. */
  using RepoIdType = CRepo *;
  /** Id to denote \ref Repo::noRepository. */
  static const RepoIdType noRepoId( 0 );

}

#endif
