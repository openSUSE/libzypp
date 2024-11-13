#!/usr/bin/env python3

import sys
import re
import inquirer
import gi.repository
# Set the search path to use the newly generated introspection files
gi.require_version('GIRepository', '2.0')
gi.require_version('GLib', '2.0')
gi.require_version('Gio', '2.0')

from tqdm import tqdm

from gi.repository import GIRepository
from gi.repository import GLib
from gi.repository import Gio
GIRepository.Repository.prepend_search_path('/home/zbenjamin/build/zypp-stack/Desktop-Debug/libzypp/zypp-glib')

gi.require_version('Zypp', '1.0')
from gi.repository import Zypp


context = Zypp.Context()

class Tracker:
  def __init__(self, tracker = None, nesting = 0 ):
    if tracker is None:
      self.zyppTracker = Zypp.ProgressObserver()
    else:
      self.zyppTracker = tracker

    self.childBars   = [] #where we store our children
    self.progressBar = None
    self.nesting     = 0

    self.zyppTracker.connect("notify::label", self.on_label_change )
    self.zyppTracker.connect("notify::progress", self.on_progress )
    self.zyppTracker.connect("start", self.on_start )
    self.zyppTracker.connect("finished", self.on_finished )
    self.zyppTracker.connect("new-subtask", self.on_new_subtask )
    self.zyppTracker.connect("event", self.on_user_request )

    self.progressBar = tqdm( total=100, position=self.nesting, desc=self.zyppTracker.get_label() )

  def on_start( self, sender ):
    self.progressBar.update()
    self.lastProgress = 0

  def on_finished( self, sender ):
    self.progressBar.close()
    del self.progressBar
    self.progressBar = None

  def on_progress( self, sender, param ):
    newProgress = sender.get_property(param.name)
    self.progressBar.update( n=(newProgress - self.lastProgress) )
    self.lastProgress = newProgress

  def on_label_change( self, sender, param ):
    self.progressBar.set_description( desc=sender.get_property(param.name), refresh=True )

  def on_child_finished( self, child ):
    try:
      for child in self.childBars:
        if child.zyppTracker == child:
          self.childBars.remove(child)
          return
    except ValueError:
      pass

  def on_new_subtask( self, sender, subtask_param ):
    self.childBars.append( Tracker(subtask_param, self.nesting+1) )
    subtask_param.connect("finished", self.on_child_finished )

  def on_user_request( self, sender, event ):
    match event:
      case Zypp.BooleanChoiceRequest() as boolReq:

        # we are going to handle the event
        boolReq.set_accepted()

        questions = [
          inquirer.List('choice',
                        message=boolReq.get_label(),
                        choices=[('yes', True), ('no', False)],
                        default=['no']
                    ),
        ]
        answer = inquirer.prompt(questions)["choice"]
        print("User selected: %s" % str(answer))
        boolReq.set_choice(answer)

      case Zypp.ListChoiceRequest() as listReq:

        # we are going to handle the event
        listReq.set_accepted()

        # build a list of tuples for our choices,
        # where each tuple is the label for the option, and the value returned if the corresponding option is chosen.
        # Here it's simply the index of the option in the options list
        choices = []
        for idx, opt in enumerate(listReq.get_options()):
          choices.append( ("%s(%s)" % (opt.get_label(), opt.get_detail()), idx) );

        print ("Default for upcoming question is: %s" % choices[listReq.get_default()][0])

        questions = [
          inquirer.List('choice',
                        message=listReq.get_label(),
                        choices=choices,
                        default=choices[listReq.get_default()][0]
                    ),
        ]
        answer = inquirer.prompt(questions)["choice"]
        print("User selected: %s" % str(answer))
        listReq.set_choice(answer)

      case Zypp.ShowMsgRequest() as msgReq:

        # we are going to handle the event
        msgReq.set_accepted()

        print(msgReq.get_message())


progBar = Tracker()
context.set_progress_observer(progBar.zyppTracker)


print ("Load system")

if not context.load_system("/tmp/fake"):
    print ("Failed to load system")
    exit(1)

print ("Loaded system")

repomgr = None
try:
  repomgr = Zypp.RepoManager.new_initialized( context )
except GLib.GError as e:
  print("Error: " + e.message)
  print("History: " + str(Zypp.Error.get_history(e)))
  exit(1)

known_repos = repomgr.get_known_repos()
print ("Knows repos: " + str(len(known_repos) ))
for i in known_repos:
  print ( "Repo: "+i.name() )

print ("Refreshing")

#refreshing the repos might change properties in the RepoInfos in our list
refresh_results = repomgr.refresh_repos( known_repos, True)
for res in refresh_results:
  try:
      result1 = res.get_value()
  except GLib.GError as e:
      print("Error: " + e.message)
      print("History: " + str(Zypp.Error.get_history(e)))
  else:
      print("No error with res: ")
      print(str(result1))
