#!/usr/bin/env python3

import asyncio
import gi.repository

# Set the search path to use the newly generated introspection files
gi.require_version('GIRepository', '2.0')
gi.require_version('GLib', '2.0')
gi.require_version('Gio', '2.0')

from gi.events import GLibEventLoopPolicy

from gi.repository import GIRepository
from gi.repository import GLib
from gi.repository import Gio
GIRepository.Repository.prepend_search_path('/home/zbenjamin/build/zypp-stack/Desktop-Debug/libzypp/zypp-glib')

gi.require_version('Zypp', '1.0')
from gi.repository import Zypp

from . import Tracker

policy = gi.events.GLibEventLoopPolicy() 
asyncio.set_event_loop_policy( policy )

class ZyppApp:
  def __init__(self, glibCtx) -> None:
    self._application = Zypp.Application( eventDispatcher=glib_ctx )
    self._contexts    = dict()
  
  # creates a context for the given rootPath if none was created yet, 
  # otherewise returns the existing one
  def openContext( self, rootPath ) -> Zypp.Context:
    try:
      ctx = self._contexts[rootPath]
      return ctx
    except KeyError:
      ctx = Zypp.Context()
      if not ctx.load_system("/tmp/fake"):
        print ("Failed to load system")
        return None
      self._contexts[ ctx.sysroot()] = ctx
      return ctx
  
  def closeContext( self, rootPath ) -> bool:
    try:
      ctx = self._contexts[rootPath]
      del self._contexts[rootPath]
      return True
    except KeyError:
      return False

# small wrapper around calling async glib funcs, seems calling them directly breaks with asyncio.gather
async def g_async( func, *args ):
  res = await func(*args)
  return res

async def main():
  # todo find a better way to get to the context
  glib_ctx = asyncio.get_running_loop()._context

  if glib_ctx is None:
    print("We need a context")
    exit(1)

  zyppApp = ZyppApp( glib_ctx )


  repomgr = None
  try:
    repomgr = Zypp.RepoManager.new_initialized( context )
  except GLib.GError as e:
    print("Error: " + e.message)
    print("History: " + str(Zypp.Error.get_history(e)))
    exit(1)

  known_repos = repomgr.get_known_repos()
  if len(known_repos) == 0:
    print("Nothing to do")
    exit(0)

  print ("Knows repos: " + str(len(known_repos) ))

  coros = [ g_async( repomgr.refresh_repo, i , True, None ) for i in known_repos ]
  results = await asyncio.gather( *coros, return_exceptions=True ) 
  for i in results:
    if isinstance(i, GLib.Error):
      print("Error: " + i.message)
      print("History: " + str(Zypp.Error.get_history(i)))
    else:
      print("Yay refresh done for: " + i.alias() )

loop = policy.get_event_loop()
loop.run_until_complete(main())