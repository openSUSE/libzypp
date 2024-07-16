#!/usr/bin/env python3

import sys
import gi.repository
# Set the search path to use the newly generated introspection files
gi.require_version('GIRepository', '2.0')
gi.require_version('GLib', '2.0')
gi.require_version('Gio', '2.0')

from gi.repository import GIRepository
from gi.repository import GLib
from gi.repository import Gio
GIRepository.Repository.prepend_search_path('/home/zbenjamin/build/libzypp/zyppng')

mainloop = GLib.MainLoop()

gi.require_version('Zypp', '1.0')
from gi.repository import Zypp

context = Zypp.Context( glibContext=mainloop.get_context())

if not context.load_system("/"):
    print ("Failed to load system")
    exit(1)


repomgr = context.get_repo_manager()
known_repos = repomgr.get_known_repos()
for i in known_repos:
  print ( i.name() )

print("\n\n")

def on_progress( tracker, param ):
    print("New progress %r" % tracker.get_property(param.name))

def on_change( tracker, param ):
    print("New value for param: %r %r" % (param.name, tracker.get_property(param.name)) )

def on_finished( tracker ):
  print("Tracker finished: %s " % tracker.get_label())

def on_new_subtask( tracker, subtask_param ):
  print("Got new subtask: %s" % subtask_param.get_label())
  subtask_param.connect("finished", on_finished)

tracker = Zypp.TaskStatus( label="Refreshing" )
tracker.connect("notify::value", on_change)
tracker.connect("notify::steps", on_change)
tracker.connect("notify::progress", on_progress)
tracker.connect("finished", on_finished)
tracker.connect("new-subtask", on_new_subtask)
refresh_results = repomgr.refresh_repos( known_repos, True, tracker )
tracker.set_finished()
for res in refresh_results:
  try:
      result1 = res.get_value()
  except GLib.GError as e:
      print("Error: " + e.message)
  else:
      print("No error with res: ")
      print(result1)

cancellable = Gio.Cancellable()
downloader = context.get_downloader()

list = GLib.List()

l = [i for i in range(10)]
l.append( (sys.maxsize*2)+1 )
print("Maxsize is: "+str( (sys.maxsize*2)+1 )+"\n" )
repomgr.test_list(l)

for i in l:
    print("Elem is: "+str(i)+"\n")


resi = None

def on_ready_callback( source_object, result, user_data ):
    print("Yay it's ready")
    try:
        p = source_object.get_file_finish( result )
    except GLib.GError as e:
        print("Error: " + e.message)
    else:
        resi = Zypp.ManagedFile( p.get_path(), True )
        p.set_dispose_enabled(False)
        print("File was downloaded to " + p.get_path() )
    mainloop.quit()


#downloader.get_file_async( "https://download.opensuse.org/repositories/server:/mail/15.4/server:mail.repo", "/tmp", cancellable, on_ready_callback, None)
#mainloop.run()

#if resi:
    #print("File was downloaded to " + resi.get_path() )
