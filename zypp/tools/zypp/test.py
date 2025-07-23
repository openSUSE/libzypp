from textual.app import App, ComposeResult
from textual.widgets import Footer, Label, ListItem, ListView, ProgressBar, RichLog, Input, Collapsible, Tree
from textual.containers import HorizontalGroup, VerticalScroll, Vertical

from rich.text import Text


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

    def log( self, text ):
        self.query_one( RichLog ).write(text)
        self.query_one( ProgressList ).append( ListItem(ProgressWithLog()))

class ProgressWithLogItem( ListItem ):
    pass


class ProgressList( ListView ):
    #DEFAULT_CSS = """
    #    ProgressList {
    #        ProgressWithLog {
    #            height: auto;
    #        }
    #    }
    #"""

    def __init__(self, *children: ListItem, initial_index: int | None = 0, name: str | None = None, id: str | None = None, classes: str | None = None, disabled: bool = False) -> None:
        super().__init__(*children, initial_index=initial_index, name=name, id=id, classes=classes, disabled=disabled)

class ListViewExample(App):
    def compose(self) -> ComposeResult:
        yield ProgressList (
            ListItem(ProgressWithLog()),
            ListItem(ProgressWithLog()),
            ListItem(ProgressWithLog())
        )
        yield Input(placeholder="Enter your name")
        yield Footer()

    def on_input_changed(self, event: Input.Changed) -> None:
        self.query_one(ProgressList).query_one(ProgressWithLog).log( event.value )
        pass

if __name__ == "__main__":
    app = ListViewExample()
    app.run()