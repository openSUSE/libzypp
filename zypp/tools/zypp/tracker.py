import gi.repository

from textual.app import ComposeResult
from textual.widgets import ListView, ProgressBar, RichLog, Collapsible, Select, Label, Button
from textual.containers import Vertical, Grid
from textual.screen import Screen

gi.require_version('Zypp', '1.0')
from gi.repository import Zypp

class ListChoiceDialog(Screen):

  def __init__(self, label, options, name = None, id = None, classes = None):
    super().__init__(name, id, classes)
    self.name = name
    self.options = options

  def compose(self) -> ComposeResult:
    yield Grid(
            Label(self.name, id="question"),
            Select( self.options ),
            id="dialog"
        )
    
  def on_button_pressed(self, event: Button.Pressed) -> None:
    if event.button.id == "quit":
        self.app.exit()
    else:
        self.app.pop_screen()


class ProgressWithLog(Vertical):

    DEFAULT_CSS = """
      ProgressWithLog {
          height: auto;

          RichLog {
              height: auto;
          }
          ProgressBar {
              height: auto;
          }
            ProgressList {
              height: auto;
          }
      }
    """
    
    def compose(self) -> ComposeResult:
        with Collapsible( title="Downloading"):
            yield RichLog( max_lines=10 )
            yield ProgressList()
        yield ProgressBar()

    def log( self, renderable ):
        self.query_one( RichLog ).write(renderable)



class Tracker(ListView):

  DEFAULT_CSS = """
    ProgressList {
        ProgressWithLog {
            height: auto;
        }
    }
  """

  def __init__(self, *children, initial_index = 0, name = None, id = None, classes = None, disabled = False, tracker = None, nesting = 0, label = None ):
    super().__init__(*children, initial_index=initial_index, name=name, id=id, classes=classes, disabled=disabled)

    if tracker is None:
      self.zyppTracker = Zypp.ProgressObserver()
    else:
      self.zyppTracker = tracker

    if label is not None:
      self.zyppTracker.set_label(label)

    self.nesting     = nesting
  
    self.zyppTracker.connect("notify::label", self.on_label_change )
    self.zyppTracker.connect("notify::progress", self.on_progress )
    self.zyppTracker.connect("start", self.on_start )
    self.zyppTracker.connect("finished", self.on_finished )
    self.zyppTracker.connect("new-subtask", self.on_new_subtask )
    self.zyppTracker.connect("event", self.on_user_request )


  def compose(self) -> ComposeResult:
      with Collapsible( title=self.zyppTracker.get_label()):
          yield RichLog( max_lines=10 )
      yield ProgressBar()

  def on_start( self, sender ):
    m = self.query_one(ProgressBar())
    m.update( total=self.zyppTracker.get_steps(), progress=self.zyppTracker.get_current())
    self.lastProgress = 0

  def on_progress( self, sender, param ):
    newProgress = sender.get_property(param.name)
    self.query_one(ProgressBar()).update( progress=self.zyppTracker.get_current() )
    self.lastProgress = newProgress

  def on_finished( self, sender ):
    self.query_one(ProgressBar()).update( progress=self.zyppTracker.get_steps() )

  def on_label_change( self, sender, param ):
    self.query_one(Collapsible()).title = sender.get_property(param.name)

  def on_new_subtask( self, sender, subtask_param ):
    self.query_one(Collapsible()).mount( Tracker(tracker=subtask_param, nesting=self.nesting+1) )
    subtask_param.connect("finished", self.on_child_finished )

  def on_child_finished( self, child ):
    try:
      for child in self.query_children(Tracker()):
        if child.zyppTracker == child:
          child.remove()
          return
    except ValueError:
      pass

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
        self.query_one(RichLog()).write(msgReq.get_message())