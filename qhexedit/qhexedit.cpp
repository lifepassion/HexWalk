#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QMenu>

#include "qhexedit.h"
#include <algorithm>


// ********************************************************************** Constructor, destructor

QHexEdit::QHexEdit(QWidget *parent) : QAbstractScrollArea(parent)
    , _addressArea(true)
    , _addressWidth(4)
    , _asciiArea(true)
    , _bytesPerLine(16)
    , _hexCharsInLine(47)
    , _highlighting(true)
    , _overwriteMode(true)
    , _readOnly(true)
    , _hexCaps(false)
    , _dynamicBytesPerLine(false)
    , _editAreaIsAscii(false)
    , _chunks(new Chunks(this))
    , _cursorPosition(0)
    , _lastEventSize(0)
    , _undoStack(new UndoStack(_chunks, this))
{
#ifdef Q_OS_WIN32
    setFont(QFont("Courier", 10));
#else
    setFont(QFont("Monospace", 10));
#endif
    setAddressAreaColor(this->palette().alternateBase().color());
    setHighlightingColor(QColor(0xff, 0xff, 0x99, 0xff));
    setSelectionColor(this->palette().highlight().color());
    setAddressFontColor(QPalette::WindowText);
    setAsciiAreaColor(this->palette().alternateBase().color());
    setAsciiFontColor(QPalette::WindowText);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QAbstractScrollArea::customContextMenuRequested, this, &QHexEdit::showContextMenu);
    connect(&_cursorTimer, SIGNAL(timeout()), this, SLOT(updateCursor()));
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(adjust()));
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(adjust()));
    connect(_undoStack, SIGNAL(indexChanged(int)), this, SLOT(dataChangedPrivate(int)));

    _cursorTimer.setInterval(500);
    _cursorTimer.start();
    _scrollMult = 1;
    setAddressWidth(4);
    setAddressArea(true);
    setAsciiArea(true);
    setOverwriteMode(true);
    setHighlighting(true);
    setReadOnly(true);

    init();

}

QHexEdit::~QHexEdit()
{
}

// ********************************************************************** Properties

void QHexEdit::setAddressArea(bool addressArea)
{
    _addressArea = addressArea;
    adjust();
    setCursorPosition(_cursorPosition);
    viewport()->update();
}

bool QHexEdit::addressArea()
{
    return _addressArea;
}

void QHexEdit::setAddressAreaColor(const QColor &color)
{
    _addressAreaColor = color;
    viewport()->update();
}

QColor QHexEdit::addressAreaColor()
{
    return _addressAreaColor;
}

void QHexEdit::setAddressFontColor(const QColor &color)
{
    _addressFontColor = color;
    viewport()->update();
}

QColor QHexEdit::addressFontColor()
{
    return _addressFontColor;
}

void QHexEdit::setAsciiAreaColor(const QColor &color)
{
    _asciiAreaColor = color;
    viewport()->update();
}

QColor QHexEdit::asciiAreaColor()
{
    return _asciiAreaColor;
}

void QHexEdit::setAsciiFontColor(const QColor &color)
{
    _asciiFontColor = color;
    viewport()->update();
}

QColor QHexEdit::asciiFontColor()
{
    return _asciiFontColor;
}

void QHexEdit::setHexFontColor(const QColor &color)
{
    _hexFontColor = color;
    viewport()->update();
}

QColor QHexEdit::hexFontColor()
{
    return _hexFontColor;
}

void QHexEdit::setAddressOffset(qint64 addressOffset)
{
    _addressOffset = addressOffset;
    adjust();
    setCursorPosition(_cursorPosition);
    viewport()->update();
}

qint64 QHexEdit::addressOffset()
{
    return _addressOffset;
}

void QHexEdit::setAddressWidth(int addressWidth)
{
    _addressWidth = addressWidth;
    adjust();
    setCursorPosition(_cursorPosition);
    viewport()->update();
}

qint64 QHexEdit::getSize()
{
    return _chunks->size();
}
int QHexEdit::addressWidth()
{
    const qint64 lastRow = _bytesPerLine > 0 && _chunks->size() > 0
        ? (_chunks->size() - 1) / _bytesPerLine
        : 0;
    const int digits = QString::number(lastRow, 10).length();
    return std::max(digits, _addressWidth);
}

void QHexEdit::setAsciiArea(bool asciiArea)
{
    if (!asciiArea)
        _editAreaIsAscii = false;
    _asciiArea = asciiArea;
    adjust();
    setCursorPosition(_cursorPosition);
    viewport()->update();
}

bool QHexEdit::asciiArea()
{
    return _asciiArea;
}

void QHexEdit::setBytesPerLine(int count)
{
    _bytesPerLine = count;
    _hexCharsInLine = count * 3 - 1;

    adjust();
    setCursorPosition(_cursorPosition);
    viewport()->update();
}

int QHexEdit::bytesPerLine()
{
    return _bytesPerLine;
}

qint64 QHexEdit::firstByteAddress()
{
    return _bPosFirst;
}

void QHexEdit::setCursorPosition(qint64 position)
{
    // 1. delete old cursor
    _blink = false;
    viewport()->update(_cursorRect);
    viewport()->update(_cursorRect2);

    // 2. Check, if cursor in range?
    if (position > (_chunks->size() * 2 - 1))
        position = _chunks->size() * 2  - (_overwriteMode ? 1 : 0);

    if (position < 0)
        position = 0;

    // 3. Calc new position of cursor
    _bPosCurrent = position / 2;
    _pxCursorY = ((position / 2 - _bPosFirst) / _bytesPerLine + 2) * _pxCharHeight;
    int x = (position % (2 * _bytesPerLine));
    if (_editAreaIsAscii)
    {
        _pxCursorX = x / 2 * _pxCharWidth + _pxPosAsciiX;
        _pxCursorX2 = (((x / 2) * 3) + (x % 2)) * _pxCharWidth + _pxPosHexX;
        _cursorPosition = position & 0xFFFFFFFFFFFFFFFE;
    } else {
        _pxCursorX = (((x / 2) * 3) + (x % 2)) * _pxCharWidth + _pxPosHexX;
        _pxCursorX2 = x / 2 * _pxCharWidth + _pxPosAsciiX;
        _cursorPosition = position;
    }

    if (_overwriteMode)
    {
        _cursorRect = QRect(_pxCursorX - horizontalScrollBar()->value(), _pxCursorY + _pxCursorWidth, _pxCharWidth, _pxCursorWidth);
        _cursorRect2 = QRect(_pxCursorX2 - horizontalScrollBar()->value(), _pxCursorY + _pxCursorWidth, _pxCharWidth, _pxCursorWidth);
    }
    else
    {
        _cursorRect = QRect(_pxCursorX - horizontalScrollBar()->value(), _pxCursorY - _pxCharHeight + 4, _pxCursorWidth, _pxCharHeight);
        _cursorRect2 = QRect(_pxCursorX2 - horizontalScrollBar()->value(), _pxCursorY + _pxCursorWidth, _pxCharWidth, _pxCursorWidth);
    }

    // 4. Immediately draw new cursor
    _blink = true;
    viewport()->update(_cursorRect);
    viewport()->update(_cursorRect2);
    emit currentAddressChanged(_bPosCurrent);
}

qint64 QHexEdit::cursorPosition(QPoint pos)
{
    // Calc cursor position depending on a graphical position
    qint64 result = -1;
    int posX = pos.x() + horizontalScrollBar()->value();
    int posY = pos.y() - _pxCharHeight - 3;
    if (posY < 0)
        return result;
    if ((posX >= _pxPosHexX) && (posX < (_pxPosHexX + (1 + _hexCharsInLine) * _pxCharWidth)))
    {
        _editAreaIsAscii = false;
        int x = (posX - _pxPosHexX) / _pxCharWidth;
        x = (x / 3) * 2 + std::min(x % 3, 1);
        int y = (posY / _pxCharHeight) * 2 * _bytesPerLine;
        result = _bPosFirst * 2 + x + y;
    }
    else
        if (_asciiArea && (posX >= _pxPosAsciiX) && (posX < (_pxPosAsciiX + (1 + _bytesPerLine) * _pxCharWidth)))
        {
            _editAreaIsAscii = true;
            int x = 2 * (posX - _pxPosAsciiX) / _pxCharWidth;
            int y = (posY / _pxCharHeight) * 2 * _bytesPerLine;
            result = _bPosFirst * 2 + x + y;
        }
    return result;
}

qint64 QHexEdit::cursorPosition()
{
    return _cursorPosition;
}

void QHexEdit::setData(const QByteArray &ba)
{
    _data = ba;
    _bData.setData(_data);
    setData(_bData);
}

QByteArray QHexEdit::data()
{
    return _chunks->data(0, -1);
}

void QHexEdit::setHighlighting(bool highlighting)
{
    _highlighting = highlighting;
    viewport()->update();
}

bool QHexEdit::highlighting()
{
    return _highlighting;
}

void QHexEdit::setHighlightingColor(const QColor &color)
{
    _brushHighlighted = QBrush(color);
    _penHighlighted = QPen(viewport()->palette().color(QPalette::WindowText));
    viewport()->update();
}

QColor QHexEdit::highlightingColor()
{
    return _brushHighlighted.color();
}

void QHexEdit::setOverwriteMode(bool overwriteMode)
{
    _overwriteMode = overwriteMode;
    emit overwriteModeChanged(overwriteMode);
}

bool QHexEdit::overwriteMode()
{
    return _overwriteMode;
}

void QHexEdit::setSelectionColor(const QColor &color)
{
    _brushSelection = QBrush(color);
    _penSelection = QPen(Qt::white);
    viewport()->update();
}

QColor QHexEdit::selectionColor()
{
    return _brushSelection.color();
}

bool QHexEdit::isReadOnly()
{
    return _readOnly;
}

void QHexEdit::setReadOnly(bool readOnly)
{
    Q_UNUSED(readOnly);
    _readOnly = true;
    viewport()->update();
}

void QHexEdit::setHexCaps(const bool isCaps)
{
    if (_hexCaps != isCaps)
    {
        _hexCaps = isCaps;
        viewport()->update();
    }
}

bool QHexEdit::hexCaps()
{
    return _hexCaps;
}

void QHexEdit::setDynamicBytesPerLine(const bool isDynamic)
{
    _dynamicBytesPerLine = isDynamic;
    resizeEvent(NULL);
}

bool QHexEdit::dynamicBytesPerLine()
{
    return _dynamicBytesPerLine;
}

// ********************************************************************** Access to data of qhexedit
bool QHexEdit::setData(QIODevice &iODevice)
{
    bool ok = _chunks->setIODevice(iODevice);
    init();
    dataChangedPrivate();
    return ok;
}

QByteArray QHexEdit::dataAt(qint64 pos, qint64 count)
{
    return _chunks->data(pos, count);
}

bool QHexEdit::write(QIODevice &iODevice, qint64 pos, qint64 count)
{
    return _chunks->write(iODevice, pos, count);
}

// ********************************************************************** Char handling
void QHexEdit::insert(qint64 index, char ch)
{
    if (_readOnly)
        return;

    _undoStack->insert(index, ch);
    refresh();
}

void QHexEdit::remove(qint64 index, qint64 len)
{
    if (_readOnly)
        return;

    _undoStack->removeAt(index, len);
    refresh();
}

void QHexEdit::replace(qint64 index, char ch)
{
    if (_readOnly)
        return;

    _undoStack->overwrite(index, ch);
    refresh();
}

// ********************************************************************** ByteArray handling
void QHexEdit::insert(qint64 pos, const QByteArray &ba)
{
    if (_readOnly)
        return;

    _undoStack->insert(pos, ba);
    refresh();
}

void QHexEdit::replace(qint64 pos, qint64 len, const QByteArray &ba)
{
    if (_readOnly)
        return;

    _undoStack->overwrite(pos, len, ba);
    refresh();
}

// ********************************************************************** Utility functions
void QHexEdit::ensureVisible()
{
    const qint64 lastVisiblePosition =
        (_bPosFirst + qint64(std::max(0, _rowsShown - 1)) * _bytesPerLine) * 2;

    if (_cursorPosition < (_bPosFirst * 2))
        verticalScrollBar()->setValue((int)(_cursorPosition / 2 / _bytesPerLine/_scrollMult));
    if (_cursorPosition > lastVisiblePosition)
        verticalScrollBar()->setValue((int)(((_cursorPosition / 2 / _bytesPerLine) - _rowsShown + 1)/_scrollMult));
    if (_pxCursorX < horizontalScrollBar()->value())
        horizontalScrollBar()->setValue(_pxCursorX);
    if ((_pxCursorX + _pxCharWidth) > (horizontalScrollBar()->value() + viewport()->width()))
        horizontalScrollBar()->setValue(_pxCursorX + _pxCharWidth - viewport()->width());
    viewport()->update();
}

qint64 QHexEdit::indexOf(const QByteArray &ba, qint64 from, bool isRegex,bool isCaseInsensitive, bool invertMatch)
{
    qint64 pos = _chunks->indexOf(ba, from,isRegex,isCaseInsensitive, invertMatch);
    if (pos > -1)
        selectSearchResult(pos, _chunks->matchSize);
    return pos;
}

bool QHexEdit::isModified()
{
    return _modified;
}

qint64 QHexEdit::lastIndexOf(const QByteArray &ba, qint64 from)
{
    qint64 pos = _chunks->lastIndexOf(ba, from);
    if (pos > -1)
        selectSearchResult(pos, ba.length());
    return pos;
}

void QHexEdit::selectSearchResult(qint64 bytePosition, qint64 byteLength)
{
    const qint64 selectionStart = bytePosition * 2;
    const qint64 selectionEnd = selectionStart + byteLength * 2;

    resetSelection(selectionStart);
    setSelection(selectionEnd);
    setCursorPosition(byteLength > 0 ? selectionEnd - 1 : selectionStart);
    ensureVisible();
}

void QHexEdit::setMouseSelection(qint64 cursorPosition)
{
    const qint64 currentByte = std::max<qint64>(0, std::min(cursorPosition / 2, _chunks->size()));
    _bSelectionBegin = std::min(_bSelectionInit, currentByte);
    _bSelectionEnd = std::min(_chunks->size(), std::max(_bSelectionInit, currentByte) + 1);
}

void QHexEdit::redo()
{
    if (_readOnly)
        return;

    _undoStack->redo();
    setCursorPosition(_chunks->pos()*2);
    refresh();
}

QString QHexEdit::selectionToReadableString()
{
    QByteArray ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin());
    return toReadable(ba);
}

QString QHexEdit::selectedData()
{
    QByteArray ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin()).toHex();
    return ba;
}
QByteArray QHexEdit::selectedDataBa()
{
    QByteArray ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin());
    return ba;
}


void QHexEdit::setFont(const QFont &font)
{
    QFont theFont(font);
    theFont.setStyleHint(QFont::Monospace);
    QWidget::setFont(theFont);
    QFontMetrics metrics = fontMetrics();
#if QT_VERSION > QT_VERSION_CHECK(5, 11, 0)
    _pxCharWidth = metrics.horizontalAdvance(QLatin1Char('A'))+1;
#else
    _pxCharWidth = metrics.width(QLatin1Char('A'))+1;
#endif
    _pxCharHeight = metrics.height();
    _pxGapAdr = _pxCharWidth / 2;
    _pxGapAdrHex = _pxCharWidth;
    _pxGapHexAscii = 2 * _pxCharWidth;
    _pxCursorWidth = _pxCharHeight / 7;
    _pxSelectionSub = _pxCharHeight / 5;
    viewport()->update();
}

QString QHexEdit::toReadableString()
{
    QByteArray ba = _chunks->data();
    return toReadable(ba);
}

void QHexEdit::undo()
{
    if (_readOnly)
        return;

    _undoStack->undo();
    setCursorPosition(_chunks->pos()*2);
    refresh();
}

// ********************************************************************** Handle events
void QHexEdit::keyPressEvent(QKeyEvent *event)
{
    // Cursor movements
    if (event->matches(QKeySequence::MoveToNextChar))
    {
        qint64 pos = _cursorPosition + 1;
        if (_editAreaIsAscii)
            pos += 1;
        setCursorPosition(pos);
        resetSelection(pos);
    }
    if (event->matches(QKeySequence::MoveToPreviousChar))
    {
        qint64 pos = _cursorPosition - 1;
        if (_editAreaIsAscii)
            pos -= 1;
        setCursorPosition(pos);
        resetSelection(pos);
    }
    if (event->matches(QKeySequence::MoveToEndOfLine))
    {
        qint64 pos = _cursorPosition - (_cursorPosition % (2 * _bytesPerLine)) + (2 * _bytesPerLine) - 1;
        setCursorPosition(pos);
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToStartOfLine))
    {
        qint64 pos = _cursorPosition - (_cursorPosition % (2 * _bytesPerLine));
        setCursorPosition(pos);
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToPreviousLine))
    {
        setCursorPosition(_cursorPosition - (2 * _bytesPerLine));
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToNextLine))
    {
        setCursorPosition(_cursorPosition + (2 * _bytesPerLine));
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToNextPage))
    {
        setCursorPosition(_cursorPosition + (((_rowsShown - 1) * 2 * _bytesPerLine)));
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToPreviousPage))
    {
        setCursorPosition(_cursorPosition - (((_rowsShown - 1) * 2 * _bytesPerLine)));
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToEndOfDocument))
    {
        setCursorPosition(_chunks->size() * 2 );
        resetSelection(_cursorPosition);
    }
    if (event->matches(QKeySequence::MoveToStartOfDocument))
    {
        setCursorPosition(0);
        resetSelection(_cursorPosition);
    }

    // Select commands
    if (event->matches(QKeySequence::SelectAll))
    {
        resetSelection(0);
        setSelection(2 * _chunks->size() + 1);
    }
    if (event->matches(QKeySequence::SelectNextChar))
    {
        qint64 pos = _cursorPosition + 1;
        if (_editAreaIsAscii)
            pos += 1;
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectPreviousChar))
    {
        qint64 pos = _cursorPosition - 1;
        if (_editAreaIsAscii)
            pos -= 1;
        setSelection(pos);
        setCursorPosition(pos);
    }
    if (event->matches(QKeySequence::SelectEndOfLine))
    {
        qint64 pos = _cursorPosition - (_cursorPosition % (2 * _bytesPerLine)) + (2 * _bytesPerLine) - 1;
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectStartOfLine))
    {
        qint64 pos = _cursorPosition - (_cursorPosition % (2 * _bytesPerLine));
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectPreviousLine))
    {
        qint64 pos = _cursorPosition - (2 * _bytesPerLine);
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectNextLine))
    {
        qint64 pos = _cursorPosition + (2 * _bytesPerLine);
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectNextPage))
    {
        qint64 pos = _cursorPosition + (((viewport()->height() / _pxCharHeight) - 1) * 2 * _bytesPerLine);
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectPreviousPage))
    {
        qint64 pos = _cursorPosition - (((viewport()->height() / _pxCharHeight) - 1) * 2 * _bytesPerLine);
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectEndOfDocument))
    {
        qint64 pos = _chunks->size() * 2;
        setCursorPosition(pos);
        setSelection(pos);
    }
    if (event->matches(QKeySequence::SelectStartOfDocument))
    {
        qint64 pos = 0;
        setCursorPosition(pos);
        setSelection(pos);
    }

    // Edit Commands
    if (!_readOnly)
    {
        /* Cut */
        if (event->matches(QKeySequence::Cut))
        {
            QByteArray ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin()).toHex();
            for (qint64 idx = 32; idx < ba.size(); idx +=33)
                ba.insert(idx, "\n");
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(ba);
            if (_overwriteMode)
            {
                qint64 len = getSelectionEnd() - getSelectionBegin();
                replace(getSelectionBegin(), (int)len, QByteArray((int)len, char(0)));
            }
            else
            {
                remove(getSelectionBegin(), getSelectionEnd() - getSelectionBegin());
            }
            setCursorPosition(2 * getSelectionBegin());
            resetSelection(2 * getSelectionBegin());
        } else

        /* Paste */
        if (event->matches(QKeySequence::Paste))
        {
            QClipboard *clipboard = QApplication::clipboard();
            QByteArray ba = QByteArray().fromHex(clipboard->text().toLatin1());
            if (_overwriteMode)
            {
                ba = ba.left(std::min<qint64>(ba.size(), (_chunks->size() - _bPosCurrent)));
                replace(_bPosCurrent, ba.size(), ba);
            }
            else
                insert(_bPosCurrent, ba);
            setCursorPosition(_cursorPosition + 2 * ba.size());
            resetSelection(getSelectionBegin());
        } else

        /* Delete char */
        if (event->matches(QKeySequence::Delete))
        {
            if (getSelectionBegin() != getSelectionEnd())
            {
                _bPosCurrent = getSelectionBegin();
                if (_overwriteMode)
                {
                    QByteArray ba = QByteArray(getSelectionEnd() - getSelectionBegin(), char(0));
                    replace(_bPosCurrent, ba.size(), ba);
                }
                else
                {
                    remove(_bPosCurrent, getSelectionEnd() - getSelectionBegin());
                }
            }
            else
            {
                if (_overwriteMode)
                    replace(_bPosCurrent, char(0));
                else
                    remove(_bPosCurrent, 1);
            }
            setCursorPosition(2 * _bPosCurrent);
            resetSelection(2 * _bPosCurrent);
        } else

        /* Backspace */
        if ((event->key() == Qt::Key_Backspace) && (event->modifiers() == Qt::NoModifier))
        {
            if (getSelectionBegin() != getSelectionEnd())
            {
                _bPosCurrent = getSelectionBegin();
                setCursorPosition(2 * _bPosCurrent);
                if (_overwriteMode)
                {
                    QByteArray ba = QByteArray(getSelectionEnd() - getSelectionBegin(), char(0));
                    replace(_bPosCurrent, ba.size(), ba);
                }
                else
                {
                    remove(_bPosCurrent, getSelectionEnd() - getSelectionBegin());
                }
                resetSelection(2 * _bPosCurrent);
            }
            else
            {
                bool behindLastByte = false;
                if ((_cursorPosition / 2) == _chunks->size())
                    behindLastByte = true;

                _bPosCurrent -= 1;
                if (_overwriteMode)
                    replace(_bPosCurrent, char(0));
                else
                    remove(_bPosCurrent, 1);

                if (!behindLastByte)
                    _bPosCurrent -= 1;

                setCursorPosition(2 * _bPosCurrent);
                resetSelection(2 * _bPosCurrent);
            }
        } else

        /* undo */
        if (event->matches(QKeySequence::Undo))
        {
            undo();
        } else

        /* redo */
        if (event->matches(QKeySequence::Redo))
        {
            redo();
        } else

        if ((QApplication::keyboardModifiers() == Qt::NoModifier) ||
            (QApplication::keyboardModifiers() == Qt::KeypadModifier) ||
            (QApplication::keyboardModifiers() == Qt::ShiftModifier) ||
            (QApplication::keyboardModifiers() == (Qt::AltModifier | Qt::ControlModifier)) ||
            (QApplication::keyboardModifiers() == Qt::GroupSwitchModifier))
        {
            /* Hex and ascii input */
            int key = 0;
            QString text = event->text();
            if (!text.isEmpty())
            {
                if (_editAreaIsAscii)
                    key = (uchar)text.at(0).toLatin1();
                else
                    key = int(text.at(0).toLower().toLatin1());
            }

            if ((((key >= '0' && key <= '9') || (key >= 'a' && key <= 'f')) && _editAreaIsAscii == false)
                || (key >= ' ' && _editAreaIsAscii))
            {
                if (getSelectionBegin() != getSelectionEnd())
                {
                    if (_overwriteMode)
                    {
                        qint64 len = getSelectionEnd() - getSelectionBegin();
                        replace(getSelectionBegin(), (int)len, QByteArray((int)len, char(0)));
                    } else
                    {
                        remove(getSelectionBegin(), getSelectionEnd() - getSelectionBegin());
                        _bPosCurrent = getSelectionBegin();
                    }
                    setCursorPosition(2 * _bPosCurrent);
                    resetSelection(2 * _bPosCurrent);
                }

                // If insert mode, then insert a byte
                if (_overwriteMode == false)
                    if ((_cursorPosition % 2) == 0)
                        insert(_bPosCurrent, char(0));

                // Change content
                if (_chunks->size() > 0)
                {
                    char ch = key;
                    if (!_editAreaIsAscii){
                        QByteArray hexValue = _chunks->data(_bPosCurrent, 1).toHex();
                        if ((_cursorPosition % 2) == 0)
                            hexValue[0] = key;
                        else
                            hexValue[1] = key;
                        ch = QByteArray().fromHex(hexValue)[0];
                    }
                    replace(_bPosCurrent, ch);
                    if (_editAreaIsAscii)
                        setCursorPosition(_cursorPosition + 2);
                    else
                        setCursorPosition(_cursorPosition + 1);
                    resetSelection(_cursorPosition);
                }
            }
        }


    }

    /* Copy */
    if (event->matches(QKeySequence::Copy))
    {
        QByteArray ba;
        if(!_editAreaIsAscii)
        {
        ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin()).toHex();
        for (qint64 idx = 32; idx < ba.size(); idx +=33)
        ba.insert(idx, "\n");
        }
        else
        {
            ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin());
            for (int i = 0; i < ba.length(); i++) {
                if(ba.at(i) < 32 || ba.at(i) > 126)
                {
                    ba[i] = '.';
                }
            }
        }


        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(ba);
    }

    // Switch between insert/overwrite mode
    if ((event->key() == Qt::Key_Insert) && (event->modifiers() == Qt::NoModifier))
    {
        setOverwriteMode(!overwriteMode());
        setCursorPosition(_cursorPosition);
    }

    // switch from hex to ascii edit
    if (event->key() == Qt::Key_Tab && !_editAreaIsAscii){
        _editAreaIsAscii = true;
        setCursorPosition(_cursorPosition);
    }

    // switch from ascii to hex edit
    if (event->key() == Qt::Key_Backtab  && _editAreaIsAscii){
        _editAreaIsAscii = false;
        setCursorPosition(_cursorPosition);
    }

    refresh();
    QAbstractScrollArea::keyPressEvent(event);
}

void QHexEdit::mouseMoveEvent(QMouseEvent * event)
{
    _blink = false;
    viewport()->update();
    qint64 actPos = cursorPosition(event->pos());
    if (actPos >= 0)
    {
        setCursorPosition(actPos);
        setMouseSelection(actPos);
    }
}

void QHexEdit::mousePressEvent(QMouseEvent * event)
{
    _blink = false;
    viewport()->update();
    qint64 cPos = cursorPosition(event->pos());
    if (cPos >= 0)
    {
        if (event->button() != Qt::RightButton)
            resetSelection(cPos);
        setCursorPosition(cPos);
    }
}

void QHexEdit::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    int pxOfsX = horizontalScrollBar()->value();
    const QRect clipRect = event->rect();
    const int logicalLeft = pxOfsX + clipRect.left();
    const int logicalRight = pxOfsX + clipRect.right();

    if (event->rect() != _cursorRect && event->rect() != _cursorRect2)
    {
        int pxPosStartY = 2 * _pxCharHeight;

        // draw some patterns if needed
        painter.fillRect(clipRect, viewport()->palette().color(QPalette::Base));
        if (_addressArea)
        {
            painter.fillRect(QRect(-pxOfsX, clipRect.top(), _pxPosHexX - _pxGapAdrHex/2, height()), _addressAreaColor);
        }
        if (_asciiArea)
        {
            int linePos = _pxPosAsciiX - (_pxGapHexAscii / 2);
            painter.setPen(Qt::gray);
            painter.drawLine(linePos - pxOfsX, clipRect.top(), linePos - pxOfsX, height());
        }
        const int rulerStep = _bytesPerLine >= 8 ? 8 :
                              (_bytesPerLine >= 4 ? 4 :
                              (_bytesPerLine >= 2 ? 2 : 1));

        if (clipRect.top() < _pxCharHeight)
        {
            painter.fillRect(QRect(clipRect.left(), 0, clipRect.width(), _pxCharHeight), QColor("#f8fafc"));
            painter.setPen(QColor("#d4dce6"));
            painter.drawLine(clipRect.left(), _pxCharHeight - 1, clipRect.right(), _pxCharHeight - 1);
        }

        painter.setPen(viewport()->palette().color(QPalette::WindowText));

        int firstVisibleCol = _bytesPerLine;
        int lastVisibleCol = -1;
        const int hexAreaLeft = _pxPosHexX - _pxCharWidth;
        const int hexAreaRight = _pxPosHexX + (_bytesPerLine - 1) * 3 * _pxCharWidth + 2 * _pxCharWidth;
        if (logicalRight >= hexAreaLeft && logicalLeft <= hexAreaRight)
        {
            int firstHexCol = std::max(0, (logicalLeft - _pxPosHexX - 2 * _pxCharWidth) / (3 * _pxCharWidth));
            int lastHexCol = std::min(_bytesPerLine - 1, (logicalRight - _pxPosHexX + _pxCharWidth) / (3 * _pxCharWidth));
            firstVisibleCol = std::min(firstVisibleCol, firstHexCol);
            lastVisibleCol = std::max(lastVisibleCol, lastHexCol);
        }

        if (_asciiArea)
        {
            const int asciiAreaRight = _pxPosAsciiX + _bytesPerLine * _pxCharWidth;
            if (logicalRight >= _pxPosAsciiX && logicalLeft <= asciiAreaRight)
            {
                int firstAsciiCol = std::max(0, (logicalLeft - _pxPosAsciiX) / _pxCharWidth);
                int lastAsciiCol = std::min(_bytesPerLine - 1, (logicalRight - _pxPosAsciiX) / _pxCharWidth);
                firstVisibleCol = std::min(firstVisibleCol, firstAsciiCol);
                lastVisibleCol = std::max(lastVisibleCol, lastAsciiCol);
            }
        }

        if (clipRect.top() < _pxCharHeight && firstVisibleCol <= lastVisibleCol)
        {
            const QFont originalFont = painter.font();
            QFont rulerFont = originalFont;
            rulerFont.setBold(true);
            painter.setFont(rulerFont);
            painter.setPen(QPen(_addressFontColor));
            for (int colIdx = firstVisibleCol; colIdx <= lastVisibleCol; colIdx++)
            {
                int pxPosX = _pxPosHexX + colIdx * 3 * _pxCharWidth - pxOfsX;
                const int byteCenterX = pxPosX + _pxCharWidth;
                const bool isMajorTick = colIdx % rulerStep == 0;
                const int tickHeight = isMajorTick ? 5 : 3;
                painter.drawLine(byteCenterX, _pxCharHeight - tickHeight,
                                 byteCenterX, _pxCharHeight - 1);

                if (isMajorTick)
                {
                    QRect textRect(byteCenterX - 2 * _pxCharWidth, 0,
                                   4 * _pxCharWidth, _pxCharHeight - tickHeight);
                    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter,
                                     QString::number(colIdx, 10));
                }
            }
            painter.setFont(originalFont);
        }

        const int firstVisibleRow = std::max(0, (clipRect.top() - _pxCharHeight - _pxSelectionSub) / _pxCharHeight);
        const int lastVisibleRow = std::min(_rowsShown, (clipRect.bottom() - _pxCharHeight + _pxCharHeight) / _pxCharHeight);

        // paint address area
        if (_addressArea)
        {
            QString address;
            const int lastAddressRow = std::min(lastVisibleRow, _dataShown.size() / _bytesPerLine);
            for (int row = firstVisibleRow; row <= lastAddressRow; row++)
            {
                int pxPosY = pxPosStartY + row * _pxCharHeight;
                const qint64 rowNumber = _bPosFirst / _bytesPerLine + row;
                address = QString::number(rowNumber, 10);
                painter.setPen(QPen(_addressFontColor));
                QRect addressRect(-pxOfsX,
                                  pxPosY - _pxCharHeight + _pxSelectionSub,
                                  _pxPosHexX - _pxGapAdrHex / 2,
                                  _pxCharHeight);
                painter.drawText(addressRect.adjusted(0, 0, -_pxGapAdr, 0),
                                 Qt::AlignRight | Qt::AlignVCenter, address);
            }
        }

        // paint hex and ascii area
        QPen colStandard = QPen(viewport()->palette().color(QPalette::WindowText));

        painter.setBackgroundMode(Qt::TransparentMode);

        for (int row = firstVisibleRow; row <= lastVisibleRow; row++)
        {
            QByteArray hex;
            int pxPosY = pxPosStartY + row * _pxCharHeight;
            qint64 bPosLine = row * _bytesPerLine;
            for (int colIdx = firstVisibleCol; ((bPosLine + colIdx) < _dataShown.size() && (colIdx <= lastVisibleCol)); colIdx++)
            {
                QColor c = viewport()->palette().color(QPalette::Base);
                painter.setPen(QPen(_hexFontColor));

                qint64 posBa = _bPosFirst + bPosLine + colIdx;
                int pxPosX = _pxPosHexX + colIdx * 3 * _pxCharWidth - pxOfsX;
                int pxPosAsciiX2 = _pxPosAsciiX + colIdx * _pxCharWidth - pxOfsX;
                const bool isSelected = (getSelectionBegin() <= posBa) && (getSelectionEnd() > posBa);
                if (isSelected)
                {
                    c = _brushSelection.color();
                    painter.setPen(_penSelection);
                }
                else
                {
                    if (_highlighting)
                        if (_markedShown.at((int)(posBa - _bPosFirst)))
                        {
                            c = _brushHighlighted.color();
                            painter.setPen(_penHighlighted);
                        }
                }

                // render hex value
                QRect r;
                if (isSelected)
                    r.setRect(pxPosX - 1, pxPosY - _pxCharHeight + _pxSelectionSub,
                              2 * _pxCharWidth + 2, _pxCharHeight);
                else if (colIdx == 0)
                    r.setRect(pxPosX, pxPosY - _pxCharHeight + _pxSelectionSub, 2*_pxCharWidth, _pxCharHeight);
                else
                    r.setRect(pxPosX - _pxCharWidth, pxPosY - _pxCharHeight + _pxSelectionSub, 3*_pxCharWidth, _pxCharHeight);
                painter.fillRect(r,c);

                QRect tagrect;
                if(colorTag)
                {
                    for(int i=0;i<colorTag->size();i++)
                    {
                        const ColorTag &tag0 = colorTag->at(i);
                        if(posBa >= tag0.pos && posBa < (tag0.pos + tag0.size))
                        {
                            if (colIdx == 0)
                                tagrect.setRect(pxPosX, pxPosY - _pxCharHeight + _pxSelectionSub, 2*_pxCharWidth, _pxCharHeight);
                            else
                                tagrect.setRect(pxPosX - _pxCharWidth, pxPosY - _pxCharHeight + _pxSelectionSub, 3*_pxCharWidth, _pxCharHeight);
                            QColor tempColor = QColor(QString::fromStdString(tag0.color));
                            tempColor.setAlpha(80);
                            painter.fillRect(tagrect, QBrush(tempColor));
                        }
                    }
                }

                hex = _hexDataShown.mid((bPosLine + colIdx) * 2, 2);
                painter.drawText(pxPosX, pxPosY, hexCaps()?hex.toUpper():hex);

                // render ascii value
                if (_asciiArea)
                {
                    if (c == viewport()->palette().color(QPalette::Base))
                        c = _asciiAreaColor;
                    int ch = (uchar)_dataShown.at(bPosLine + colIdx);
                    if ( ch < ' ' || ch > '~' )
                        ch = '.';
                    r.setRect(pxPosAsciiX2, pxPosY - _pxCharHeight + _pxSelectionSub, _pxCharWidth, _pxCharHeight);
                    painter.fillRect(r, c);
                    painter.setPen(QPen(_asciiFontColor));
                    painter.drawText(pxPosAsciiX2, pxPosY, QChar(ch));
                    pxPosAsciiX2 += _pxCharWidth;
                }
            }
        }

        painter.setPen(QPen(QColor("#dfe5ec")));
        for (int colIdx = rulerStep; colIdx < _bytesPerLine; colIdx += rulerStep)
        {
            const int separatorX = _pxPosHexX + colIdx * 3 * _pxCharWidth
                                   - _pxCharWidth / 2 - pxOfsX;
            if (separatorX >= clipRect.left() && separatorX <= clipRect.right())
                painter.drawLine(separatorX, _pxCharHeight, separatorX, viewport()->height());
        }
        painter.setBackgroundMode(Qt::TransparentMode);
        painter.setPen(viewport()->palette().color(QPalette::WindowText));
    }

    // _cursorPosition counts in 2, _bPosFirst counts in 1
    int hexPositionInShowData = _cursorPosition - 2 * _bPosFirst;
    const bool hideReadOnlyCursor = _readOnly && getSelectionBegin() < getSelectionEnd();

    // due to scrolling the cursor can go out of the currently displayed data
    if (!hideReadOnlyCursor && (hexPositionInShowData >= 0) &&
        (hexPositionInShowData <= _hexDataShown.size()))
    {
        // paint cursor
        if (_readOnly)
        {
            QColor color = viewport()->palette().dark().color();
            painter.fillRect(QRect(_pxCursorX - pxOfsX, _pxCursorY - _pxCharHeight + _pxSelectionSub, _pxCharWidth, _pxCharHeight), color);
        }
        else
        {
            if (_blink && hasFocus())
            {

                painter.fillRect(_cursorRect, this->palette().color(QPalette::WindowText));
                painter.fillRect(_cursorRect2, this->palette().color(QPalette::WindowText));
            }
        }
            if (_editAreaIsAscii)
            {
                // every 2 hex there is 1 ascii
                int asciiPositionInShowData = hexPositionInShowData / 2;
                if(asciiPositionInShowData <_dataShown.size())
                {
                    int ch = (uchar)_dataShown.at(asciiPositionInShowData);

                    if (ch < ' ' || ch > '~')
                        ch = '.';

                    painter.drawText(_pxCursorX - pxOfsX, _pxCursorY, QChar(ch));
                }
            }
            else
            {
                painter.drawText(_pxCursorX - pxOfsX, _pxCursorY, hexCaps() ? _hexDataShown.mid(hexPositionInShowData, 1).toUpper() : _hexDataShown.mid(hexPositionInShowData, 1));
            }
    }

    // emit event, if size has changed
    if (_lastEventSize != _chunks->size())
    {
        _lastEventSize = _chunks->size();
        emit currentSizeChanged(_lastEventSize);
    }
}

void QHexEdit::resizeEvent(QResizeEvent *)
{
    if (_dynamicBytesPerLine)
    {
        int pxFixGaps = 0;
        if (_addressArea)
            pxFixGaps = addressWidth() * _pxCharWidth + _pxGapAdr;
        pxFixGaps += _pxGapAdrHex;
        if (_asciiArea)
            pxFixGaps += _pxGapHexAscii;

        // +1 because the last hex value do not have space. so it is effective one char more
        int charWidth = (viewport()->width() - pxFixGaps ) / _pxCharWidth + 1;

        // 2 hex alfa-digits 1 space 1 ascii per byte = 4; if ascii is disabled then 3
        // to prevent devision by zero use the min value 1
        setBytesPerLine(std::max(charWidth / (_asciiArea ? 4 : 3),1));
    }
    adjust();
}

bool QHexEdit::focusNextPrevChild(bool next)
{
    if (_addressArea)
    {
        if ( (next && _editAreaIsAscii) || (!next && !_editAreaIsAscii ))
            return QWidget::focusNextPrevChild(next);
        else
            return false;
    }
    else
    {
        return QWidget::focusNextPrevChild(next);
    }
}

// ********************************************************************** Handle selections
void QHexEdit::resetSelection()
{
    _bSelectionBegin = _bSelectionInit;
    _bSelectionEnd = _bSelectionInit;
}

void QHexEdit::resetSelection(qint64 pos)
{
    pos = pos / 2 ;
    if (pos < 0)
        pos = 0;
    if (pos > _chunks->size())
        pos = _chunks->size();

    _bSelectionInit = pos;
    _bSelectionBegin = pos;
    _bSelectionEnd = pos;
}

void QHexEdit::setSelection(qint64 pos)
{
    pos = pos / 2;
    if (pos < 0)
        pos = 0;
    if (pos > _chunks->size())
        pos = _chunks->size();

    if (pos >= _bSelectionInit)
    {
        _bSelectionEnd = pos;
        _bSelectionBegin = _bSelectionInit;
    }
    else
    {
        _bSelectionBegin = pos;
        _bSelectionEnd = _bSelectionInit;
    }
}

qint64 QHexEdit::getSelectionBegin()
{
    return _bSelectionBegin;
}

qint64 QHexEdit::getSelectionEnd()
{
    return _bSelectionEnd;
}

// ********************************************************************** Private utility functions
void QHexEdit::init()
{
    _undoStack->clear();
    setAddressOffset(0);
    resetSelection(0);
    setCursorPosition(0);
    verticalScrollBar()->setValue(0);

    _modified = false;
}

void QHexEdit::adjust()
{
    // recalc Graphics
    if (_addressArea)
    {
        _addrDigits = addressWidth();
        _pxPosHexX = _pxGapAdr + _addrDigits*_pxCharWidth + _pxGapAdrHex;
    }
    else
        _pxPosHexX = _pxGapAdrHex;
    _pxPosAdrX = _pxGapAdr;
    _pxPosAsciiX = _pxPosHexX + _hexCharsInLine * _pxCharWidth + _pxGapHexAscii;

    // set horizontalScrollBar()
    int pxWidth = _pxPosAsciiX;
    if (_asciiArea)
        pxWidth += _bytesPerLine*_pxCharWidth;
    horizontalScrollBar()->setRange(0, pxWidth - viewport()->width());
    horizontalScrollBar()->setPageStep(viewport()->width());

    // set verticalScrollbar()
    _rowsShown = std::max(0, ((viewport()->height()-4)/_pxCharHeight) - 1);
    qint64 lineCount = _chunks->size() / (qint64)_bytesPerLine + 1;
    if(lineCount >= 1024*1024*1024){
        _scrollMult = ceil(lineCount/(1024*1024*1024));
    }
    else
    {
        _scrollMult = 1;
    }
    verticalScrollBar()->setRange(0, (lineCount - _rowsShown)/_scrollMult);
    verticalScrollBar()->setPageStep(_rowsShown);

    qint64 value = (qint64)verticalScrollBar()->value()*_scrollMult;
    _bPosFirst = value * _bytesPerLine;
    _bPosLast = _bPosFirst + (qint64)(_rowsShown * _bytesPerLine) - 1;
    if (_bPosLast >= _chunks->size())
        _bPosLast = _chunks->size() - 1;
    readBuffers();
    setCursorPosition(_cursorPosition);
}

void QHexEdit::dataChangedPrivate(int)
{
    _modified = _undoStack->index() != 0;
    adjust();
    emit dataChanged();
}

void QHexEdit::refresh()
{
    ensureVisible();
    readBuffers();
}

void QHexEdit::readBuffers()
{
    _dataShown = _chunks->data(_bPosFirst, _bPosLast - _bPosFirst + _bytesPerLine + 1, &_markedShown);
    _hexDataShown = QByteArray(_dataShown.toHex());
}

QString QHexEdit::toReadable(const QByteArray &ba)
{
    QString result;

    for (int i=0; i < ba.size(); i += 16)
    {
        QString addrStr = QString("%1").arg(_addressOffset + i, addressWidth(), 16, QChar('0'));
        QString hexStr;
        QString ascStr;
        for (int j=0; j<16; j++)
        {
            if ((i + j) < ba.size())
            {
                hexStr.append(" ").append(ba.mid(i+j, 1).toHex());
                char ch = ba[i + j];
                if ((ch < 0x20) || (ch > 0x7e))
                        ch = '.';
                ascStr.append(QChar(ch));
            }
        }
        result += addrStr + " " + QString("%1").arg(hexStr, -48) + "  " + QString("%1").arg(ascStr, -17) + "\n";
    }
    return result;
}

void QHexEdit::updateCursor()
{
    if (_blink)
        _blink = false;
    else
        _blink = true;
    viewport()->update(_cursorRect);
}
void QHexEdit::showContextMenu(const QPoint &pos)
{
    QMenu contextMenu;
    if(!_editAreaIsAscii)
    {
        if(getSelectionEnd() - getSelectionBegin()> 0)
        {


            QAction *copyAction = contextMenu.addAction(tr("Copy"));
            connect(copyAction, &QAction::triggered, this, &QHexEdit::copyText);

            if (!_readOnly)
            {
                QAction *cutAction = contextMenu.addAction(tr("Cut"));
                connect(cutAction, &QAction::triggered, this, &QHexEdit::cutText);
            }

        }
        if (!_readOnly)
        {
            QAction *pasteAction = contextMenu.addAction(tr("Paste"));
            connect(pasteAction, &QAction::triggered, this, &QHexEdit::pasteText);
        }

        contextMenu.exec(mapToGlobal(pos));
    }


}
void QHexEdit::copyText(){
    QByteArray ba;
    if(!_editAreaIsAscii)
    {
        ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin()).toHex();
        for (qint64 idx = 32; idx < ba.size(); idx +=33)
            ba.insert(idx, "\n");
    }
    else
    {
        ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin());
        for (int i = 0; i < ba.length(); i++) {
            if(ba.at(i) < 32 || ba.at(i) > 126)
            {
                ba[i] = '.';
            }
        }
    }


    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(ba);
}
void QHexEdit::pasteText(){
    if (_readOnly)
        return;

    QClipboard *clipboard = QApplication::clipboard();
    QByteArray ba = QByteArray().fromHex(clipboard->text().toLatin1());
    if (_overwriteMode)
    {
        ba = ba.left(std::min<qint64>(ba.size(), (_chunks->size() - _bPosCurrent)));
        replace(_bPosCurrent, ba.size(), ba);
    }
    else
        insert(_bPosCurrent, ba);
    setCursorPosition(_cursorPosition + 2 * ba.size());
    resetSelection(getSelectionBegin());
}
void QHexEdit::cutText(){
    if (_readOnly)
        return;

    QByteArray ba = _chunks->data(getSelectionBegin(), getSelectionEnd() - getSelectionBegin()).toHex();
    for (qint64 idx = 32; idx < ba.size(); idx +=33)
        ba.insert(idx, "\n");
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(ba);
    if (_overwriteMode)
    {
        qint64 len = getSelectionEnd() - getSelectionBegin();
        replace(getSelectionBegin(), (int)len, QByteArray((int)len, char(0)));
    }
    else
    {
        remove(getSelectionBegin(), getSelectionEnd() - getSelectionBegin());
    }
    setCursorPosition(2 * getSelectionBegin());
    resetSelection(2 * getSelectionBegin());
}
