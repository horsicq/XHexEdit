/* Copyright (c) 2021-2022 hors<horsicq@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "xhexedit.h"

XHexEdit::XHexEdit(QWidget *pParent) : XDeviceTableView(pParent)
{
    g_nBytesProLine = 16;  // TODO Set/Get !!!
    g_nAddressWidth = 8;   // TODO Set/Get !!!
    g_nCursorHeight = 2;   // TODO Set/Get !!!
    g_nDataBlockSize = 0;
    g_nStartOffset = 0;
    g_bIsAddressColon = false;

    addColumn(tr("Offset"));
    addColumn(tr("Hex"));
    setSelectionEnable(false);     // TODO Set/Get
    setTextFont(getMonoFont(10));  // TODO const !!! mb move to XDeviceTableView
    setBlinkingCursorEnable(true);
}

void XHexEdit::_adjustView()
{
    setTextFontFromOptions(XOptions::ID_HEX_FONT);

    g_bIsAddressColon = getGlobalOptions()->getValue(XOptions::ID_HEX_ADDRESSCOLON).toBool();
}

void XHexEdit::adjustView()
{
    _adjustView();

    if (getDevice()) {
        reload(true);
    }
}

void XHexEdit::setData(QIODevice *pDevice, quint64 nStartOffset)
{
    // mb TODO options
    setDevice(pDevice);
    g_nStartOffset = nStartOffset;

    resetCursorData();

    adjustColumns();

    qint64 nTotalLineCount = getDataSize() / g_nBytesProLine;

    if (getDataSize() % g_nBytesProLine == 0) {
        nTotalLineCount--;
    }

    setTotalLineCount(nTotalLineCount);

    setCursorOffset(nStartOffset, COLUMN_HEX);

    reload(true);
}

bool XHexEdit::writeHexKey(qint64 nOffset, BYTEPOS bytePos, qint32 nKey)
{
    bool bResult = false;

    QByteArray baByte = read_array(nOffset, 1);

    if (baByte.size()) {
        quint8 nByte = XBinary::_read_uint8(baByte.data());

        quint8 nValue = 0;

        if ((nKey >= Qt::Key_A) && (nKey <= Qt::Key_F)) {
            nValue = 10 + (nKey - Qt::Key_A);
        } else if ((nKey >= Qt::Key_0) && (nKey <= Qt::Key_9)) {
            nValue = (nKey - Qt::Key_0);
        }

        if (bytePos == BYTEPOS_LOW) {
            nByte &= 0xF0;
            nByte += nValue;
        } else if (bytePos == BYTEPOS_HIGH) {
            nByte &= 0x0F;
            nByte += (nValue << 4);
        }

        if (write_array(nOffset, (char *)(&nByte), 1) == 1) {
            bResult = true;
        }
    }

    return bResult;
}

XAbstractTableView::OS XHexEdit::cursorPositionToOS(XAbstractTableView::CURSOR_POSITION cursorPosition)
{
    OS osResult = {};

    osResult.nOffset = -1;

    if ((cursorPosition.bIsValid) && (cursorPosition.ptype == PT_CELL)) {
        qint64 nBlockOffset = getViewStart() + (cursorPosition.nRow * g_nBytesProLine);

        if (cursorPosition.nColumn == COLUMN_ADDRESS) {
            osResult.nOffset = nBlockOffset;
            osResult.nSize = 1;
            osResult.varData = BYTEPOS_HIGH;
        } else if (cursorPosition.nColumn == COLUMN_HEX) {
            qint32 nOffset = (cursorPosition.nCellLeft - getSideDelta()) / (getCharWidth() * 2 + getSideDelta());
            qint32 nPos = (cursorPosition.nCellLeft - getSideDelta()) % (getCharWidth() * 2 + getSideDelta());

            osResult.nOffset = nBlockOffset + nOffset;
            osResult.nSize = 1;

            if (nPos > getCharWidth()) {
                osResult.varData = BYTEPOS_LOW;
            } else {
                osResult.varData = BYTEPOS_HIGH;
            }
        }

        if (!isOffsetValid(osResult.nOffset)) {
            osResult.nOffset = getDataSize();  // TODO Check !!!
            osResult.nSize = 0;
            osResult.varData = BYTEPOS_HIGH;
        }
    }

    return osResult;
}

void XHexEdit::updateData()
{
    if (getDevice()) {
        if (getXInfoDB()) {
            QList<XBinary::MEMORY_REPLACE> listMR = getXInfoDB()->getMemoryReplaces(getMemoryMap()->nModuleAddress, getMemoryMap()->nImageSize);

            setMemoryReplaces(listMR);
        }
        // Update cursor position
        qint64 nBlockOffset = getViewStart();
        qint64 nCursorOffset = nBlockOffset + getCursorDelta();

        if (nCursorOffset >= getDataSize()) {
            nCursorOffset = getDataSize() - 1;
        }

        //        STATE state=getState();

        //        setCursorOffset(nCursorOffset,-1,state.varCursorExtraInfo.toInt());
        setCursorOffset(nCursorOffset);

        XBinary::MODE mode = XBinary::getWidthModeFromByteSize(g_nAddressWidth);

        g_listAddresses.clear();

        qint32 nDataBlockSize = g_nBytesProLine * getLinesProPage();

        QByteArray baDataBuffer = read_array(nBlockOffset, nDataBlockSize);

        g_nDataBlockSize = baDataBuffer.size();

        if (g_nDataBlockSize) {
            g_baDataHexBuffer = QByteArray(baDataBuffer.toHex());

            for (qint32 i = 0; i < g_nDataBlockSize; i += g_nBytesProLine) {
                XADDR nCurrentAddress = g_nStartOffset + i + nBlockOffset;

                QString sAddress;

                if (g_bIsAddressColon) {
                    sAddress = XBinary::valueToHexColon(mode, nCurrentAddress);
                } else {
                    sAddress = XBinary::valueToHex(mode, nCurrentAddress);
                }

                g_listAddresses.append(sAddress);
            }
        } else {
            g_baDataHexBuffer.clear();
        }

        setCurrentBlock(nBlockOffset, g_nDataBlockSize);
    }
}

void XHexEdit::paintCell(QPainter *pPainter, qint32 nRow, qint32 nColumn, qint32 nLeft, qint32 nTop, qint32 nWidth, qint32 nHeight)
{
    Q_UNUSED(nWidth)

    if (nColumn == COLUMN_ADDRESS) {
        if (nRow < g_listAddresses.count()) {
            pPainter->drawText(nLeft + getCharWidth(), nTop + nHeight, g_listAddresses.at(nRow));  // TODO Text Optional
        }
    } else if (nColumn == COLUMN_HEX) {
        STATE state = getState();

        if (nRow * g_nBytesProLine < g_nDataBlockSize) {
            qint64 nDataBlockStartOffset = getViewStart();
            qint64 nDataBlockSize = qMin(g_nDataBlockSize - nRow * g_nBytesProLine, g_nBytesProLine);

            for (qint32 i = 0; i < nDataBlockSize; i++) {
                qint32 nIndex = nRow * g_nBytesProLine + i;

                QString sHex = g_baDataHexBuffer.mid(nIndex * 2, 2);
                QString sSymbol;

                bool bSelected = isOffsetSelected(nDataBlockStartOffset + nIndex);
                bool bCursor = (state.nCursorOffset == (nDataBlockStartOffset + nIndex));  // TODO

                QRect rectSymbol;

                if (nColumn == COLUMN_HEX) {
                    rectSymbol.setRect(nLeft + getCharWidth() + (i * 2) * getCharWidth() + i * getSideDelta(), nTop, 2 * getCharWidth() + getSideDelta(), nHeight);
                    sSymbol = sHex;
                }

                if (bSelected | bCursor) {
                    QRect rectSelected;
                    rectSelected.setRect(rectSymbol.x(), rectSymbol.y() + getLineDelta(), rectSymbol.width(), rectSymbol.height());

                    if (bSelected) {
                        pPainter->fillRect(rectSelected, viewport()->palette().color(QPalette::Highlight));
                    }

                    if (bCursor) {
                        if (nColumn == state.cursorPosition.nColumn) {
                            qint32 nX = rectSymbol.x();

                            if (state.varCursorExtraInfo.toInt() == BYTEPOS_LOW) {
                                nX += getCharWidth();
                            }

                            QRect rectCursor;
                            rectCursor.setRect(nX, rectSymbol.y() + getLineDelta() + rectSymbol.height(), getCharWidth(), g_nCursorHeight);

                            setCursorData(rectCursor, QRect(), sSymbol, nIndex);
                        }
                    }
                }

                pPainter->drawText(rectSymbol.x(), rectSymbol.y() + nHeight, sSymbol);
            }
        }
    }
}

void XHexEdit::keyPressEvent(QKeyEvent *pEvent)
{
    if (pEvent->matches(QKeySequence::MoveToNextChar) || pEvent->matches(QKeySequence::MoveToPreviousChar) || pEvent->matches(QKeySequence::MoveToNextLine) ||
        pEvent->matches(QKeySequence::MoveToPreviousLine) || pEvent->matches(QKeySequence::MoveToStartOfLine) || pEvent->matches(QKeySequence::MoveToEndOfLine) ||
        pEvent->matches(QKeySequence::MoveToNextPage) || pEvent->matches(QKeySequence::MoveToPreviousPage) || pEvent->matches(QKeySequence::MoveToStartOfDocument) ||
        pEvent->matches(QKeySequence::MoveToEndOfDocument) || ((pEvent->key() >= Qt::Key_A) && (pEvent->key() <= Qt::Key_F)) ||
        ((pEvent->key() >= Qt::Key_0) && (pEvent->key() <= Qt::Key_9)) || (pEvent->key() == Qt::Key_Delete) || (pEvent->key() == Qt::Key_Backspace)) {
        qint64 nViewStart = getViewStart();

        if (pEvent->matches(QKeySequence::MoveToNextChar) || ((pEvent->key() >= Qt::Key_A) && (pEvent->key() <= Qt::Key_F)) ||
            ((pEvent->key() >= Qt::Key_0) && (pEvent->key() <= Qt::Key_9)) || (pEvent->key() == Qt::Key_Delete)) {
            STATE state = getState();

            if (((pEvent->key() >= Qt::Key_A) && (pEvent->key() <= Qt::Key_F)) || ((pEvent->key() >= Qt::Key_0) && (pEvent->key() <= Qt::Key_9)) ||
                (pEvent->key() == Qt::Key_Delete)) {
                qint32 nKey = pEvent->key();

                if (pEvent->key() == Qt::Key_Delete) {
                    nKey = Qt::Key_0;
                }

                if (writeHexKey(state.nCursorOffset, (BYTEPOS)(state.varCursorExtraInfo.toInt()), nKey)) {
                    setEdited();

                    emit dataChanged();
                }
            }

            if (state.varCursorExtraInfo.toInt() == BYTEPOS_HIGH) {
                state.varCursorExtraInfo = BYTEPOS_LOW;
            } else {
                state.varCursorExtraInfo = BYTEPOS_HIGH;
                state.nCursorOffset++;
            }

            setCursorOffset(state.nCursorOffset, -1, state.varCursorExtraInfo);
        } else if (pEvent->matches(QKeySequence::MoveToPreviousChar) || (pEvent->key() == Qt::Key_Backspace)) {
            STATE state = getState();

            if ((pEvent->key() == Qt::Key_Backspace)) {
                if (writeHexKey(state.nCursorOffset, (BYTEPOS)(state.varCursorExtraInfo.toInt()), Qt::Key_0)) {
                    setEdited();

                    emit dataChanged();
                }
            }

            if (state.varCursorExtraInfo.toInt() == BYTEPOS_LOW) {
                state.varCursorExtraInfo = BYTEPOS_HIGH;
            } else {
                state.varCursorExtraInfo = BYTEPOS_LOW;
                state.nCursorOffset--;
            }

            setCursorOffset(state.nCursorOffset, -1, state.varCursorExtraInfo);
        } else if (pEvent->matches(QKeySequence::MoveToNextLine)) {
            setCursorOffset(getCursorOffset() + g_nBytesProLine);
        } else if (pEvent->matches(QKeySequence::MoveToPreviousLine)) {
            setCursorOffset(getCursorOffset() - g_nBytesProLine);
        } else if (pEvent->matches(QKeySequence::MoveToStartOfLine)) {
            setCursorOffset(getCursorOffset() - (getCursorDelta() % g_nBytesProLine));
        } else if (pEvent->matches(QKeySequence::MoveToEndOfLine)) {
            setCursorOffset(getCursorOffset() - (getCursorDelta() % g_nBytesProLine) + g_nBytesProLine - 1);
        }

        if ((getCursorOffset() < 0) || (pEvent->matches(QKeySequence::MoveToStartOfDocument))) {
            setCursorOffset(0, -1, BYTEPOS_HIGH);
        }

        if ((getCursorOffset() >= getDataSize()) || (pEvent->matches(QKeySequence::MoveToEndOfDocument))) {
            setCursorOffset(getDataSize() - 1, -1, BYTEPOS_LOW);
        }

        if (pEvent->matches(QKeySequence::MoveToNextChar) || pEvent->matches(QKeySequence::MoveToPreviousChar) || pEvent->matches(QKeySequence::MoveToNextLine) ||
            pEvent->matches(QKeySequence::MoveToPreviousLine) || ((pEvent->key() >= Qt::Key_A) && (pEvent->key() <= Qt::Key_F)) ||
            ((pEvent->key() >= Qt::Key_0) && (pEvent->key() <= Qt::Key_9)) || (pEvent->key() == Qt::Key_Delete) || (pEvent->key() == Qt::Key_Backspace)) {
            qint64 nRelOffset = getCursorOffset() - nViewStart;

            if (nRelOffset >= g_nBytesProLine * getLinesProPage()) {
                _goToOffset(nViewStart + g_nBytesProLine, true);
            } else if (nRelOffset < 0) {
                if (!_goToOffset(nViewStart - g_nBytesProLine, true)) {
                    _goToOffset(0);
                }
            }
        } else if (pEvent->matches(QKeySequence::MoveToNextPage) || pEvent->matches(QKeySequence::MoveToPreviousPage)) {
            if (pEvent->matches(QKeySequence::MoveToNextPage)) {
                _goToOffset(nViewStart + g_nBytesProLine * getLinesProPage());
            } else if (pEvent->matches(QKeySequence::MoveToPreviousPage)) {
                _goToOffset(nViewStart - g_nBytesProLine * getLinesProPage());
            }
        } else if (pEvent->matches(QKeySequence::MoveToStartOfDocument) || pEvent->matches(QKeySequence::MoveToEndOfDocument))  // TODO
        {
            _goToOffset(getCursorOffset());
        }

        viewport()->update();

        adjust();
    }
    //    else if(pEvent->matches(QKeySequence::SelectAll))
    //    {
    //        _selectAllSlot();
    //    }
    else {
        XAbstractTableView::keyPressEvent(pEvent);
    }
}

qint64 XHexEdit::getScrollValue()
{
    qint64 nResult = 0;

    qint32 nValue = verticalScrollBar()->value();

    qint64 nMaxValue = getMaxScrollValue() * g_nBytesProLine;

    if (getDataSize() > nMaxValue) {
        if (nValue == getMaxScrollValue()) {
            nResult = getDataSize() - g_nBytesProLine;
        } else {
            nResult = ((double)nValue / (double)getMaxScrollValue()) * getDataSize();
        }
    } else {
        nResult = (qint64)nValue * g_nBytesProLine;
    }

    return nResult;
}

void XHexEdit::setScrollValue(qint64 nOffset)
{
    setViewStart(nOffset);

    qint32 nValue = 0;

    if (getDataSize() > (getMaxScrollValue() * g_nBytesProLine)) {
        if (nOffset == getDataSize() - g_nBytesProLine) {
            nValue = getMaxScrollValue();
        } else {
            nValue = ((double)(nOffset) / ((double)getDataSize())) * (double)getMaxScrollValue();
        }
    } else {
        nValue = (nOffset) / g_nBytesProLine;
    }

    verticalScrollBar()->setValue(nValue);

    adjust(true);
}

void XHexEdit::adjustColumns()
{
    const QFontMetricsF fm(getTextFont());

    if (XBinary::getWidthModeFromSize(getDataSize()) == XBinary::MODE_64) {
        g_nAddressWidth = 16;
        setColumnWidth(COLUMN_ADDRESS, 2 * getCharWidth() + fm.boundingRect("00000000:00000000").width());
    } else {
        g_nAddressWidth = 8;
        setColumnWidth(COLUMN_ADDRESS, 2 * getCharWidth() + fm.boundingRect("0000:0000").width());
    }

    setColumnWidth(COLUMN_HEX, g_nBytesProLine * 2 * getCharWidth() + 2 * getCharWidth() + getSideDelta() * g_nBytesProLine);
}

void XHexEdit::registerShortcuts(bool bState)
{
    Q_UNUSED(bState)
}
