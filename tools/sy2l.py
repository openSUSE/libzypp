#! /usr/bin/python3

import sys, os, re, time
import threading, traceback

from PyQt5.QtCore    import *
from PyQt5.QtGui     import *
from PyQt5.QtWidgets import *

#======================================================================
class Color:
    ''' '''
    Default     = QColor( 240, 240, 240 )
    LightDark   = QColor( 200, 200, 200 )
    Dark        = QColor( 160, 160, 160 )
    DarkDark    = QColor(  80,  80,  80 )
    Black       = QColor(   0,   0,   0 )

    LightGreen  = QColor( 0x99, 0xb4, 0x33 ) #99b433
    Green       = QColor( 0x00, 0xa3, 0x00 ) #00a300
    DarkGreen   = QColor( 0x1e, 0x71, 0x45 ) #1e7145
    Magenta     = QColor( 0xff, 0x00, 0x97 ) #ff0097
    LightPurple = QColor( 0x9f, 0x00, 0xa7 ) #9f00a7
    Purple      = QColor( 0x7e, 0x38, 0x78 ) #7e3878
    DarkPurple  = QColor( 0x60, 0x3c, 0xba ) #603cba
    Darken      = QColor( 0x1d, 0x1d, 0x1d ) #1d1d1d
    Teal        = QColor( 0x00, 0xab, 0xa9 ) #00aba9
    LightBlue   = QColor( 0xef, 0xf4, 0xff ) #eff4ff
    Blue        = QColor( 0x2d, 0x89, 0xef ) #2d89ef
    DarkBlue    = QColor( 0x2b, 0x57, 0x97 ) #2b5797
    Yellow      = QColor( 0xff, 0xc4, 0x0d ) #ffc40d
    Orange      = QColor( 0xe3, 0xa2, 0x1a ) #e3a21a
    DarkOrange  = QColor( 0xda, 0x53, 0x2c ) #da532c
    Red         = QColor( 0xee, 0x11, 0x11 ) #ee1111
    DarkRed     = QColor( 0xb9, 0x1d, 0x47 ) #b91d47

    fDbg = Teal
    fMil = Black
    fWar = Orange
    fErr = Red
    fSec = Magenta
    fInt = Green
    fUsr = Blue
    fSep = LightGreen


    fglevel = ( fDbg, fMil, fWar, fErr, fSec, fInt, fUsr, None )
    bglevel = ( None, None, None, None, None, None, None, fSep )

    matchFg = DarkRed
    matchBg = QColor( 235, 230, 230 )

#======================================================================
class WorkerSignals( QObject ):
    ''' Signals available from a running worker thread. '''
    started     = pyqtSignal()          # No data
    result      = pyqtSignal(object)    # data returned from processing, anything
    error       = pyqtSignal(tuple)     # ( exctype, value, traceback.format_exc() )
    finished    = pyqtSignal()          # No data

    progress    = pyqtSignal(int)       # Wfn: indicating % progress
    data        = pyqtSignal(object)    # Wfn: data produced while processing, anything

class Worker( QRunnable ):
    '''
    Worker thread handling thread setup, signals and wrap-up.

    fn:     The function to run on this worker thread.
    args:   Arguments to pass to the callback function.
    kwargs: Keywords to pass to the callback function.

    callbacks: Injected into kwargs, a list [ progress, data ]

    '''
    def __init__( self, fn, *args, **kwargs ):
        super().__init__()

        # Store constructor arguments (re-used for processing)
        self.fn = fn
        self.args = args
        self.kwargs = kwargs
        self.signals = WorkerSignals()
        # Add the callbacks to our kwargs
        self.kwargs['callbacks'] = [ self.signals.progress, self.signals.data ]

    @pyqtSlot()
    def run( self ):
        self.signals.started.emit()  # Go...
        try:
            result = self.fn( *self.args, **self.kwargs )
        except:
            traceback.print_exc()
            exctype, value = sys.exc_info()[:2]
            self.signals.error.emit( (exctype, value, traceback.format_exc()) )
        else:
            self.signals.result.emit( result )  # Return the result of the processing
        finally:
            self.signals.finished.emit()  # Done

#======================================================================
class LogLine( object ):
    ''' 2020-04-14 10:58:37 <1> hobbes(1198) [zypper++] main.cc(main):97 {T:Zypp-main} ===== Hi, me zypper 1.14.36 '''
    rxDate   = '(?P<dateFld>[0-9]{4}-[0-9]{2}-[0-9]{2})'                             # 2020-04-14
    rxTime   = '(?P<timeFld>[0-9]{2}:[0-9]{2}:[0-9]{2})'                             # 10:58:37
    rxLevel  = '(?P<levelFld><(?P<level>[0-9]+)>)'                                   # <1>
    rxHost   = '(?P<hostFld>(?P<host>[^(]*)\((?P<pid>[0-9]+)\))'                     # hobbes(1198)
    rxGroup  = '(?P<groupFld>\[(?P<group>[^]+]*)(?P<zlv>\+\+)?\])'                   # [zypper++]        (trailing ++ mapped to level 0)
    rxFile   = '(?P<fileFld>(?P<file>[^(:]*)(\((?P<fnc>[^ ]*)\))?:(?P<lne>[0-9]+))'  # main.cc(main):97
    rxThread = '(?P<threadFld>{T:(?P<thread>[^}]+)})'                                # {T:Zypp-main}
    rxText   = '(?P<textFld>.*)'                                                     # ===== Hi, me zypper 1.14.36
    rxData   = re.compile( '^%s +%s +%s +%s +%s +%s(?: +%s)? +%s$' % ( rxDate, rxTime, rxLevel, rxHost, rxGroup, rxFile, rxThread, rxText ) )

    def __init__( self, datadict ):
        self.__dict__ = datadict

    @property
    def dateFld( self ):    return self._date
    @property
    def timeFld( self ):    return self._time
    @property
    def levelFld( self ):   return self._levelFld
    @property
    def hostFld( self ):    return self._hostFld
    @property
    def groupFld( self ):   return self._groupFld
    @property
    def fileFld( self ):    return self._fileFld
    @property
    def threadFld( self ):  return self._threadFld
    @property
    def textFld( self ):    return self._text

    @property
    def date( self ):       return self._date
    @property
    def time( self ):       return self._time
    @property
    def level( self ):      return self._level
    @property
    def host( self ):       return self._host
    @property
    def pid( self ):        return self._pid
    @property
    def group( self ):      return self._group
    @property
    def file( self ):       return self._file
    @property
    def fnc( self ):        return self._fnc
    @property
    def lne( self ):        return self._lne
    @property
    def thread( self ):     return self._thread
    @property
    def text( self ):       return self._text

    @property
    def textlines( self ):  return self._textlines

#======================================================================
class LogParser( Worker ):
    """ """
    def __init__( self, args ):
        super().__init__( self._workhorse, args )

    @classmethod
    def _workhorse( cls, args, callbacks ):
        cdat = []
        dat = None
        for file in args:
            cache = {}
            lno = 0
            try:
                with open( file, 'r' ) as f:
                    for line in f:
                        lno += 1
                        m = re.match( LogLine.rxData, line )
                        if m:
                            # finish previous line
                            if dat is not None:
                                cdat.append( LogLine(dat) )
                                if len(cdat) > 500:
                                    callbacks[1].emit( cdat )
                                    cdat = []
                            dat = {
                                '_date'      : cls.getFomCacheP( cache, m, 'dateFld' ),
                                '_time'      : cls.getFomCacheP( cache, m, 'timeFld' ),
                                '_levelFld'  : cls.getFomCacheA( cache, m, 'levelFld' ),
                                '_level'     : cls.getFomCacheN( cache, m, 'level' ),
                                '_hostFld'   : cls.getFomCacheP( cache, m, 'hostFld' ),
                                '_host'      : cls.getFomCacheP( cache, m, 'host' ),
                                '_pid'       : cls.getFomCacheN( cache, m, 'pid' ),
                                '_groupFld'  : cls.getFomCacheA( cache, m, 'groupFld' ),
                                '_group'     : cls.getFomCacheA( cache, m, 'group' ),
                                '_fileFld'   : cls.getFomCacheA( cache, m, 'fileFld' ),
                                '_file'      : cls.getFomCacheA( cache, m, 'file' ),
                                '_fnc'       : cls.getFomCacheA( cache, m, 'fnc' ),
                                '_lne'       : cls.getFomCacheN( cache, m, 'lne' ),
                                '_threadFld' : cls.getFomCacheA( cache, m, 'threadFld' ),
                                '_thread'    : cls.getFomCacheA( cache, m, 'thread' ),
                                '_text'      : m.group( 'textFld' ),
                                '_textlines' : 1,
                                }
                            if m.group('zlv') is not None:
                                dat['_level'] = 0
                        elif dat is not None:
                            # multiline text
                            dat['_text'] += '\n'
                            dat['_text'] += line.rstrip('\n')
                            dat['_textlines'] += 1
                        else:
                            print( 'LogParser: %s:%d: leading garbage: %s' % ( file, lno, line.rstrip('\n') ) )
            except IOError as e:
                cls._raiseError( file, lno, "I/O error(%d): %s" % ( e.errno, e.strerror ) )
            except:
                cls._raiseError( file, lno, "Unexpected error: %s" % sys.exc_info()[0] )

        if dat is not None:
            cdat.append( LogLine(dat) )
            callbacks[1].emit( cdat )

        ret = {}
        for tag in ( 'groupFld', 'fileFld', 'threadFld' ):
            max = 0
            for v in cache[tag]:
                if v is not None and len(v) > max:
                    max = len(v)
            ret[tag] = max
        return ret

    @classmethod
    def getFomCacheA( cls, cache, m, tag ):
        # # compare and remember (A)ll value
        val = m.group(tag)
        if not tag in cache:
            cache[tag] = { val : val }
        else:
            val = cache[tag].setdefault( val, val )

        return val

    @classmethod
    def getFomCacheN( cls, cache, m, tag ):
        # # compare and remember (N)umeric value
        val = m.group(tag)
        return int(val)
        if not tag in cache:
            ival = int(val)
            cache[tag] = { val : ival }
        else:
            try:
                ival = cache[tag][val]
            except KeyError:
                ival = cache[tag].setdefault( val, int(val) )
        return ival

    @classmethod
    def getFomCacheP( cls, cache, m, tag ):
        # compare and remember (P)revious value
        val = m.group(tag)
        if not tag in cache:
            cache[tag] = val
        else:
            prev = cache[tag]
            if prev == val:
                val = prev
            else:
                cache[tag] = val
        return val

    @classmethod
    def _raiseError( cls, file, lno, msg ):
        raise Exception( 'LogParser: %s:%d: %s' % ( file, lno, msg ) ) from None

#======================================================================
class LogDataCmd( QObject ):
    """ Properties for one Command """
    propertyChange = pyqtSignal()

    def __init__( self, pid ):
        super().__init__()
        self._dat = None
        self._tim = None
        self._pid = pid
        self._ver = None
        self._cmd = None
        self._numLines = 0

    @property
    def date( self ):
        return self._dat
    @property
    def time( self ):
        return self._tim
    @property
    def pid( self ):
        return self._pid
    @property
    def version( self ):
        return self._ver
    @property
    def command( self ):
        return self._cmd

    @property
    def numLines( self ):
        return self._numLines


    def _seeLines( self, lines ):
        if len(lines):
            self._numLines += len(lines)
            if self._propcheck is not None:
                self._propcheck( lines )

    def _propcheck( self, lines ):
        miss = 0
        if self._dat is None: miss += 1
        if self._tim is None: miss += 1
        if self._ver is None: miss += 1
        if self._cmd is None: miss += 1
        omiss = miss

        if self._dat is None:
            self._dat = lines[0].date
            self._tim = lines[0].time

        for l in lines:
            if l.file == "main.cc":
                m = re.match( '===== Hi, me zypper ([0-9.]*)', l.text )
                if m:
                    self._ver = m[1]
                    l._level = 7
                m = re.match( '===== ([^A-Z].*) =====', l.text )
                if m:
                    self._cmd = m[1].replace("'","")
                    l._level = 7


        miss = 0
        if self._dat is None: miss += 1
        if self._tim is None: miss += 1
        if self._ver is None: miss += 1
        if self._cmd is None: miss += 1
        if miss != omiss:
            self.propertyChange.emit()
            if miss == 0:
                self._propcheck = None

class LogData( QObject ):
    """ """
    beginInsertCmds    = pyqtSignal( int, int ) # fst lst
    endInsertCmds      = pyqtSignal()
    cmdPropertyChanged = pyqtSignal( int )      # cidx

    beginInsertLines   = pyqtSignal( int, int ) # fst lst
    endInsertLines     = pyqtSignal()

    def __init__( self ):
        super().__init__()
        self._cmds = []
        self._lines = []

    @property
    def numCmds( self ):
        return len(self._cmds)

    @property
    def cmds( self ):
        for c in self._cmds:
            yield c

    def getCmd( self, cidx ):
        return self._cmds[cidx]

    def getCmds( self, filter=None ):
        i = -1
        for c in self._cmds:
            i += 1
            if filter and filter( i, c ):
                yield (i,c)


    @property
    def numLines( self ):
        return len(self._lines)

    @property
    def lines( self ):
        for l in self._lines:
            yield l

    def getLine( self, lidx ):
        return self._lines[lidx]

    def getLines( self, filter=None ):
        i = -1
        for l in self._lines:
            i += 1
            if filter and filter( i, l ):
                yield (i,l)


    def getMultilines( self, fst, lst ):
        for i in range( fst, lst+1 ):
            n = self.getLine(i).textlines
            if n > 1:
                yield ( i, n )


    def _fstOf( self, cidx ):
        lidx = 0
        for c in range( 0, cidx ):
            lidx += self.getCmd(c).numLines
        return lidx


    @pyqtSlot(object)
    def addLogLines( self, lines ):
        pids = {}
        for l in lines: # sort per pid
            pids.setdefault( l.pid, [] ).append( l )
        for pid in pids: # and insert
            cidx = self._assertCmd( pid )
            fst = self._fstOf(cidx+1)
            lst = fst + len(pids[pid]) - 1
            self.beginInsertLines.emit( fst, lst )
            self.getCmd(cidx)._seeLines( pids[pid] )
            self._lines[fst:fst] = pids[pid]
            self.endInsertLines.emit()

    def _assertCmd( self, pid ):
        for cidx in range( self.numCmds-1, -1, -1 ):
            if self.getCmd(cidx).pid == pid:
                return cidx
        # a new command...
        cidx = self.numCmds
        self.beginInsertCmds.emit( cidx, cidx )
        cmd = LogDataCmd( pid )
        cmd.propertyChange.connect( lambda idx=cidx: self.cmdPropertyChanged.emit( idx ) )
        self._cmds.append( cmd )
        self.endInsertCmds.emit()
        return cidx

#======================================================================
class CmdModel( QAbstractTableModel ):
    """ """
    _cfg = [
        [ LogDataCmd.date,    "Date",    Qt.AlignVCenter|Qt.AlignLeft,  11 ],
        [ LogDataCmd.time,    "Time",    Qt.AlignVCenter|Qt.AlignLeft,   9 ],
        [ LogDataCmd.pid,     "PID",     Qt.AlignVCenter|Qt.AlignRight,  8 ],
        [ LogDataCmd.version, "Version", Qt.AlignCenter,                10 ],
        [ LogDataCmd.command, "Command", Qt.AlignVCenter|Qt.AlignLeft,   0 ],
        ]
    def _cell( self, r, c=None ):
        if c is None:  # type(r) == QModelIndex:
            r,c = ( r.row(), r.column() )
        return self._cfg[c][0].fget( self._data.getCmd(r) )


    def __init__( self, data ):
        super().__init__()
        self._data = data
        data.beginInsertCmds.connect( lambda fst, lst: self.beginInsertRows( QModelIndex(), fst, lst ) )
        data.endInsertCmds.connect( self.endInsertRows )
        data.cmdPropertyChanged.connect( lambda r: self.dataChanged.emit( self.createIndex(r,0), self.createIndex(r,self.columnCount()) ) )


    def rowCount( self, midx=None ):
        return self._data.numCmds

    def columnCount( self, midx=None ):
        return len(self._cfg)

    def headerData( self, section, orientation, role ):
        if orientation == Qt.Horizontal:
            if role == Qt.DisplayRole:
                return self._cfg[section][1]
            elif role == Qt.TextAlignmentRole:
                return self._cfg[section][2]
        else:
            if role == Qt.DisplayRole:
                return section+1

    def data( self, midx, role ):

        if role == Qt.DisplayRole:
            return self._cell( midx )
        elif role == Qt.DecorationRole:
            pass
        elif role == Qt.FontRole:
            pass
        elif role == Qt.TextAlignmentRole:
            return self._cfg[midx.column()][2]
        elif role == Qt.BackgroundRole:
            pass
        elif role == Qt.ForegroundRole:
            pass
        elif role == Qt.CheckStateRole:
            pass
        else:
            pass #print( "data", midx, role )
        #return super().data( midx, role )

class CmdView( QTableView ):
    """ """
    def __init__( self, model ):
        super().__init__()
        self.setModel( model )

        self.setShowGrid( False )
        self.setWordWrap( False )

        f = self.font()
        f.setFamily( "Monospace" )
        em = QFontMetrics(f).size( Qt.TextSingleLine, "m" )
        self.setFont( f )

        hdr = self.horizontalHeader()
        hdr.setStretchLastSection( True )
        for i in range( 0, len(model._cfg) ):
            w = model._cfg[i][3]
            if w: hdr.resizeSection( i, em.width()*w )

        hdr = self.verticalHeader()
        hdr.setDefaultSectionSize( em.height() )

        self.setSelectionBehavior( QAbstractItemView.SelectRows )

#======================================================================
class LogModel( QAbstractTableModel ):
    """ """
    _cfg = [
        [ LogLine.date,     "Date",           Qt.AlignTop|Qt.AlignLeft, 10 ],
        [ LogLine.time,     "Time",           Qt.AlignTop|Qt.AlignLeft,  8 ],
        [ LogLine.levelFld, "<L>",            Qt.AlignTop|Qt.AlignLeft,  3 ],
        [ LogLine.hostFld,  "Host(Pid)",      Qt.AlignTop|Qt.AlignLeft,  0 ],
        [ LogLine.groupFld, "[Group]",        Qt.AlignTop|Qt.AlignLeft,  0 ],
        [ LogLine.fileFld,  "File(Fnc):Line", Qt.AlignTop|Qt.AlignLeft,  0 ],
        [ LogLine.thread,   "Thread",         Qt.AlignTop|Qt.AlignLeft,  0 ],
        [ LogLine.text,     "Text",           Qt.AlignTop|Qt.AlignLeft,  0 ],
        ]
    _Cda, _Cti, _Cle, _Cho, _Cgr, _Cfi, _Cth, _Ctx = range(0,len(_cfg))

    LogLineRole = Qt.UserRole


    def _cell( self, r, c=None ):
        if c is None:  # type(r) == QModelIndex:
            r,c = ( r.row(), r.column() )
        return self._cfg[c][0].fget( self._data.getLine(r) )

    def _line( self, r, c=None ):
        if c is None:  # type(r) == QModelIndex:
            r,c = ( r.row(), r.column() )
        return self._data.getLine(r)


    def __init__( self, data ):
        super().__init__()
        self._data = data
        data.beginInsertLines.connect( lambda fst, lst: self.beginInsertRows( QModelIndex(), fst, lst ) )
        data.endInsertLines.connect( self.endInsertRows )


    def rowCount( self, midx=None ):
        return self._data.numLines

    def columnCount( self, midx=None ):
        return len(self._cfg)

    def headerData( self, section, orientation, role ):
        if orientation == Qt.Horizontal:
            if role == Qt.DisplayRole:
                return self._cfg[section][1]
            elif role == Qt.TextAlignmentRole:
                return self._cfg[section][2]
        else:
            if role == Qt.DisplayRole:
                return section+1

    def data( self, midx, role ):

        if role == Qt.DisplayRole:
            return self._cell( midx )
        if role == Qt.EditRole:
            return self._cell( midx )
        if role == Qt.ToolTipRole:
            pass
        elif role == Qt.DecorationRole:
            pass
        elif role == Qt.FontRole:
            pass
        elif role == Qt.TextAlignmentRole:
            return self._cfg[midx.column()][2]
        elif role == Qt.BackgroundRole:
            return Color.bglevel[self._line( midx ).level]
        elif role == Qt.ForegroundRole:
            return Color.fglevel[self._line( midx ).level]
        elif role == Qt.CheckStateRole:
            pass
        elif role == Qt.SizeHintRole:
            pass
        else:
            pass #print( "data", midx.row(), midx.column(), role )


    def flags( self, midx ):
        ret = super().flags( midx )
        if midx.column() == self._Ctx:
            ret |= Qt.ItemIsEditable    # not realy; just to allow text selection
        return ret

    def getLines( self, filter=None ):
        return self._data.getLines( filter )

    def getMultilines( self, fst, lst ):
        return self._data.getMultilines( fst, lst )

class LogView( QTableView ):
    """ """
    def _emheight( self, n=1 ):
        return ( self._em.height() * n )

    def __init__( self, model=None ):
        super().__init__()
        if model:
            self.setModel( model )
            model.rowsInserted.connect( self._onRowsInserted )

        self.setShowGrid( False )
        #self.setWordWrap( False )

        f = self.font()
        f.setFamily( "Monospace" )
        self.setFont( f )
        em = self._em = QFontMetrics(f).size( Qt.TextSingleLine, "m" )

        hdr = self.horizontalHeader()
        hdr.setStretchLastSection( True )
        for i in range( 0, len(model._cfg) ):
            w = model._cfg[i][3]
            if w: hdr.resizeSection( i, em.width()*w )

        hdr = self.verticalHeader()
        hdr.setMinimumSectionSize( self._emheight() )
        #hdr.setDefaultSectionSize( self._emheight() )

        txt = self.txtdelegate = LogTextDelegate( self )
        txt.setMinimumRowHeight( hdr.minimumSectionSize() )
        txt.rowHeightHint.connect( self._onRowHeightChange )
        self.setItemDelegateForColumn( self.model()._Ctx , txt )

        self.setSelectionMode( QAbstractItemView.NoSelection )
        self.setSelectionBehavior( QAbstractItemView.SelectRows )


    @pyqtSlot(dict)
    def onParseDone( self, dict ):
        em = self._em.width()
        hdr = self.horizontalHeader()
        hdr.resizeSection( 4, em*dict['groupFld'] )
        hdr.resizeSection( 5, em*min(dict['fileFld'],30) )

    @pyqtSlot(QModelIndex,int,int)
    def _onRowsInserted( self, pidx, fst, lst ):
        if fst == 0:
            self.setColumnHidden( 0, True )
            self.setColumnHidden( 1, True )
            self.setColumnHidden( 2, True )
            self.setColumnHidden( 3, True )

    @pyqtSlot(int,int)
    def _onRowHeightChange( self, row, height ):
        self.verticalHeader().resizeSection( row, height )

    def matchData( self, midx ):
        return None #[(1,6)]

class LogTextDelegate( QStyledItemDelegate ):
    """ """
    rowHeightHint = pyqtSignal( int, int )

    def __init__( self, parent=None ):
        super().__init__( parent )
        self._minimumRowHeight = 0
        self.highFmt = QTextCharFormat()
        if Color.matchFg is not None: self.highFmt.setForeground( Color.matchFg )
        if Color.matchBg is not None: self.highFmt.setBackground( Color.matchBg )

    def minimumRowHeight( self ):
        return self._minimumRowHeight

    def setMinimumRowHeight( self, val ):
        self._minimumRowHeight = val

    def paint( self, painter, option, index ):
        self.initStyleOption( option, index )
        painter.save()

        # draw empty control/BG
        text = option.text
        option.text = ""
        option.widget.style().drawControl( QStyle.CE_ItemViewItem, option, painter )

        painter.translate( option.rect.topLeft() )
        painter.setPen( option.palette.color( QPalette.HighlightedText if option.state & QStyle.State_Selected else QPalette.Text ) )
        clip = QRectF( 0,0, option.rect.width(),option.rect.height() )

        # now render text
        layout = QTextLayout( text, option.font, option.widget )
        layout.setCacheEnabled( True )
        lopt = layout.textOption()
        lopt.setAlignment( option.displayAlignment )
        lopt.setWrapMode( QTextOption.WrapAtWordBoundaryOrAnywhere )
        lopt.setFlags( lopt.flags()|QTextOption.ShowLineAndParagraphSeparators )
        layout.setTextOption( lopt )

        layout.beginLayout()
        y = 0
        dy = QFontMetricsF(option.font).height();
        line = layout.createLine()
        while line.isValid():
            line.setPosition( QPoint(0,y) )
            line.setLineWidth( clip.width() )
            y += dy
            line = layout.createLine()
        layout.endLayout()

        high = []
        matches = self.parent().matchData( index )
        if matches is not None:
            for s,l in matches:
                fr = QTextLayout.FormatRange()
                fr.start = s
                fr.length = l
                fr.format = self.highFmt
                high.append(fr)
        layout.draw( painter, QPoint(2,0), high, clip )

        painter.restore()
        rh = max( int(layout.boundingRect().height()), self._minimumRowHeight )
        if option.rect.height() != rh: # width matches per construction
            self.rowHeightHint.emit( index.row(), rh )

#======================================================================
class MainWindow( QMainWindow ):
    """ """
    def __init__( self ):
        super().__init__()
        self.setFocus()
        self.statusBar()

        self._ld = LogData()
        # List of PIDs in the log
        self._pm = CmdModel( self._ld )
        self._pv = CmdView( self._pm )
        # The loglines
        self._lm = LogModel( self._ld )
        self._lv = LogView( self._lm )

        f = QFrame()
        fl = QVBoxLayout()
        fl.addWidget( self._pv, 1 )
        fl.addWidget( self._lv, 4 )
        f.setLayout( fl )
        self.setCentralWidget( f )


    def start( self, argv ):
        print( "*****", "start...", argv )
        if len(argv) == 0:
            argv.append( "z.log" if os.path.exists( "z.log" ) else "/var/log/zypper.log" )

        l = LogParser( argv )
        l.signals.data.connect( self._ld.addLogLines )
        l.signals.result.connect( self._lv.onParseDone )
        l.signals.finished.connect( lambda t=time.time() : self.dp( t ) )
        QThreadPool.globalInstance().start( l )
        print( "*****", "started" )


    def dp( self, st ):
        print( "*****", "done parsing", self._ld.numCmds, "in", round(time.time()-st) )
        if not self._ld.numCmds: self.quit()

    def testit( self ):
        lv = self._lv
        if lv.isColumnHidden( 0 ):
            for i in range(0,4):
                lv.setColumnHidden( i, False )
        else:
            for i in range(0,4):
                lv.setColumnHidden( i, True )

    def testit2( self ):
        print( "+++resizeRowsToContents" )
        self._lv.resizeRowsToContents()
        print( "+++resizeRowsToContents" )

    def testit3( self ):
        lv = self._lv
        to = None
        for i,l in lv.model().getLines( lambda i,l: l.level == 1 ):
            if to is None: to = not lv.isRowHidden( i )
            lv.setRowHidden( i, to )

    def testit4( self ):
        pass


    def quit( self ):
        self.close()
        QCoreApplication.quit()

    def toggleFullScreen( self ):
        self.showNormal() if self.windowState() & ( Qt.WindowFullScreen | Qt.WindowMaximized ) else self.showFullScreen()

    @property
    def keyPressEventTable( self ):
        try:
            return self._keyPressEventTable
        except AttributeError:
            self._keyPressEventTable = {
                Qt.NoModifier : {
                    Qt.Key_Escape   : self.quit,
                    Qt.Key_F11      : self.toggleFullScreen,
                    Qt.Key_F1       : self.testit,
                    Qt.Key_F2       : self.testit2,
                    Qt.Key_F3       : self.testit3,
                    Qt.Key_F4       : self.testit4,
                    },
                Qt.ControlModifier : {
                    Qt.Key_Q        : self.quit
                    }
                }
            return self._keyPressEventTable

    def keyPressEvent( self, event ):
        if event.isAutoRepeat():
            return
        try:
            self.keyPressEventTable[event.modifiers()][event.key()]()
        except KeyError:
            event.setAccepted( False )

    @pyqtSlot()
    def aboutToQuit( self ):
        print( "*****", "quit..." )
        self.close()
        QThreadPool.globalInstance().clear()
        QThreadPool.globalInstance().waitForDone(0)
        print( "*****", "end" )

#======================================================================
def main():
    app = QApplication( sys.argv )
    appname = os.path.basename( sys.argv[0] )

    main = MainWindow()
    main.setWindowTitle( appname )
    main.setWindowIconText( appname )
    main.setMinimumSize( 1200, 300 )
    main.move( 90, 50 )
    main.setWindowFlags( Qt.Window | Qt.WindowSystemMenuHint | Qt.WindowMinMaxButtonsHint )
    main.show()

    app.aboutToQuit.connect( main.aboutToQuit )
    QTimer.singleShot( 0, lambda : main.start( sys.argv[1:] ) )
    sys.exit( app.exec_() )

if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print( "OOps:", e )
