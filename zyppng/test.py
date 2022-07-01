#!/usr/bin/env python3

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
for i in repomgr.get_repos():
    print ( i.get_name() )

cancellable = Gio.Cancellable()
downloader = context.get_downloader()

def on_ready_callback( source_object, result, user_data ):
    print("Yay it's ready")
    try:
        p = source_object.get_file_finish( result )

    except GLib.GError as e:
        print("Error: " + e.message)
    else:
        print("File was downloaded to " + p )
    mainloop.quit()

downloader.get_file_async( "https://download.opensuse.org/repositories/server:/mail/15.4/server:mail.repo", "/tmp", cancellable, on_ready_callback, None)
mainloop.run()
