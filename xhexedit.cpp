/* Copyright (c) 2021-2025 hors<horsicq@gmail.com>
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
    m_nBytesProLine = 16;  // TODO Set/Get !!!
    m_nLocationWidth = 8;  // TODO Set/Get !!!
    m_nCursorHeight = 2;   // TODO Set/Get !!!
    m_nDataBlockSize = 0;
    m_nStartViewPos = 0;
    m_bIsLocationColon = false;

    addColumn(tr("Offset"));
    addColumn(tr("Hex"));
    setSelectionEnable(true);                // TODO Set/Get
    setTextFont(XOptions::getMonoFont(10));  // TODO const Check/ mb move to XDeviceTableView
                                             //    setBlinkingCursorEnable(true);
    setMaxSelectionViewSize(1);

    setVerticalLinesVisible(false);
}

void XHexEdit::adjustView()
{
    setTextFontFromOptions(XOptions::ID_HEX_FONT);

    m_bIsLocationColon = getGlobalOptions()->getValue(XOptions::ID_HEX_LOCATIONCOLON).toBool();

    viewport()->update();
}

void XHexEdit::_adjustView()
{
    adjustView();

    if (getDevice()) {
        reload(true);
    }
}

void XHexEdit::setData(QIODevice *pDevice, qint64 nStartOffset, qint64 nTotalSize)
{
    // mb TODO options !!!
    setDevice(pDevice, nStartOffset, nTotalSize);
    m_nStartViewPos = deviceOffsetToViewPos(nStartOffset);

    //    resetCursorData();

    adjustView();

    adjustColumns();

    qint64 nTotalLineCount = getViewSize() / m_nBytesProLine;

    if (getViewSize() % m_nBytesProLine == 0) {
        nTotalLineCount--;
    }

    setTotalScrollCount(nTotalLineCount);

    STATE state = getState();

    state.varCursorExtraInfo = BYTEPOS_HIGH;
    state.nSelectionViewPos = m_nStartViewPos;
    state.nSelectionViewSize = 1;

    setState(state);

    reload(true);
}

bool XHexEdit::writeHexKey(XVPOS nViewPos, BYTEPOS bytePos, qint32 nKey)
{
    // TODO delete/backspace
    bool bResult = false;

    qint64 nDeviceOffset = viewPosToDeviceOffset(nViewPos);

    if (nDeviceOffset != -1) {
        QByteArray baByte = read_array(nDeviceOffset, 1);

        if (baByte.size()) {
            quint8 nByte = XBinary::_read_uint8(baByte.data());

            quint8 nValue = 0;

            if ((nKey >= Qt::Key_A) && (nKey <= Qt::Key_F)) {
                nValue = 10 + (nKey - Qt::Key_A);
            } else if ((nKey >= Qt::Key_0) && (nKey <= Qt::Key_9)) {
                nValue = (nKey - Qt::Key_0);
            } else if ((nKey == Qt::Key_Backspace) || (nKey == Qt::Key_Delete)) {
                nValue = 0;
            }

            if (bytePos == BYTEPOS_LOW) {
                nByte &= 0xF0;
                nByte += nValue;
            } else if (bytePos == BYTEPOS_HIGH) {
                nByte &= 0x0F;
                nByte += (nValue << 4);
            }

            if (write_array(nDeviceOffset, (char *)(&nByte), 1) == 1) {
                bResult = true;
            }
        }
    }

    return bResult;
}

XAbstractTableView::OS XHexEdit::cursorPositionToOS(const XAbstractTableView::CURSOR_POSITION &cursorPosition)
{
    OS osResult = {};

    osResult.nViewPos = -1;

    if ((cursorPosition.bIsValid) && (cursorPosition.ptype == PT_CELL)) {
        XVPOS nBlockOffset = getViewPosStart() + (cursorPosition.nRow * m_nBytesProLine);

        if (cursorPosition.nColumn == COLUMN_ADDRESS) {
            osResult.nViewPos = nBlockOffset;
            osResult.nSize = 1;
            osResult.varData = BYTEPOS_HIGH;
        } else if (cursorPosition.nColumn == COLUMN_HEX) {
            qint32 nOffset = (cursorPosition.nAreaLeft - getSideDelta()) / (getCharWidth() * 2 + getSideDelta());
            qint32 nPos = (cursorPosition.nAreaLeft - getSideDelta()) % (int)((getCharWidth() * 2 + getSideDelta()));

            osResult.nViewPos = nBlockOffset + nOffset;
            osResult.nSize = 1;

            if (nPos > getCharWidth()) {
                osResult.varData = BYTEPOS_LOW;
            } else {
                osResult.varData = BYTEPOS_HIGH;
            }
        }

        if (!isViewPosValid(osResult.nViewPos)) {
            osResult.nViewPos = getViewSize();  // TODO Check !!!
            osResult.nSize = 0;
            osResult.varData = BYTEPOS_HIGH;
        }
    }

    return osResult;
}

void XHexEdit::updateData()
{
    if (getDevice()) {
        // Update cursor position
        XVPOS nBlockViewPos = getViewPosStart();
        //        qint64 nCursorOffset = nBlockOffset + getCursorDelta();

        //        if (nCursorOffset >= getViewSize()) {
        //            nCursorOffset = getViewSize() - 1;
        //        }

        //        STATE state=getState();

        //        setCursorOffset(nCursorOffset,-1,state.varCursorExtraInfo.toInt());
        //        setCursorViewPos(nCursorOffset);

        XBinary::MODE mode = XBinary::getWidthModeFromByteSize(m_nLocationWidth);

        m_listLocations.clear();

        qint32 nDataBlockSize = m_nBytesProLine * getLinesProPage();

        nDataBlockSize = qMin(nDataBlockSize, (qint32)(getViewSize() - nBlockViewPos));

        qint64 nDeviceOffset = viewPosToDeviceOffset(nBlockViewPos);

        QByteArray baDataBuffer = read_array(nDeviceOffset, nDataBlockSize);

        m_nDataBlockSize = baDataBuffer.size();

        if (m_nDataBlockSize) {
            m_baDataHexBuffer = QByteArray(baDataBuffer.toHex());

            for (qint32 i = 0; i < m_nDataBlockSize; i += m_nBytesProLine) {
                XADDR nCurrentAddress = m_nStartViewPos + i + nBlockViewPos;

                QString sAddress;

                if (m_bIsLocationColon) {
                    sAddress = XBinary::valueToHexColon(mode, nCurrentAddress);
                } else {
                    sAddress = XBinary::valueToHex(mode, nCurrentAddress);
                }

                m_listLocations.append(sAddress);
            }
        } else {
            m_baDataHexBuffer.clear();
        }

        setCurrentBlock(nBlockViewPos, m_nDataBlockSize);
    }
}

void XHexEdit::paintCell(QPainter *pPainter, qint32 nRow, qint32 nColumn, qint32 nLeft, qint32 nTop, qint32 nWidth, qint32 nHeight)
{
    Q_UNUSED(nWidth)

    if (nColumn == COLUMN_ADDRESS) {
        if (nRow < m_listLocations.count()) {
            QRectF rectSymbol;

            rectSymbol.setLeft(nLeft + getCharWidth());
            rectSymbol.setTop(nTop + getLineDelta());
            rectSymbol.setWidth(nWidth);
            rectSymbol.setHeight(nHeight - getLineDelta());

            pPainter->drawText(rectSymbol, m_listLocations.at(nRow));
        }
    } else if (nColumn == COLUMN_HEX) {
        STATE state = getState();

        if (nRow * m_nBytesProLine < m_nDataBlockSize) {
            XVPOS nDataBlockStartOffset = getViewPosStart();
            qint64 nDataBlockSize = qMin(m_nDataBlockSize - nRow * m_nBytesProLine, m_nBytesProLine);

            for (qint32 i = 0; i < nDataBlockSize; i++) {
                qint32 nIndex = nRow * m_nBytesProLine + i;

                QString sHex = m_baDataHexBuffer.mid(nIndex * 2, 2);
                QString sSymbol;

                bool bSelected = isViewPosSelected(nDataBlockStartOffset + nIndex);
                //                bool bCursor = (state.nCursorViewPos == (nDataBlockStartOffset + nIndex));  // TODO

                QRectF rectSymbol;

                if (nColumn == COLUMN_HEX) {
                    rectSymbol.setRect(nLeft + getCharWidth() + (i * 2) * getCharWidth() + i * getSideDelta(), nTop, 2 * getCharWidth() + getSideDelta(), nHeight);
                    sSymbol = sHex;
                }

                if (nColumn == COLUMN_HEX) {
                    if (bSelected) {
                        QRectF _rectSymbol = rectSymbol;

                        if (state.varCursorExtraInfo.toInt() == BYTEPOS_LOW) {
                            _rectSymbol.setLeft(rectSymbol.left() + getCharWidth());
                            _rectSymbol.setWidth(getCharWidth());
                        } else if (state.varCursorExtraInfo.toInt() == BYTEPOS_HIGH) {
                            _rectSymbol.setWidth(getCharWidth());
                        }

                        pPainter->fillRect(_rectSymbol, getColor(TCLOLOR_SELECTED));

                        //                    if (bCursor) {
                        //                        if (nColumn == state.cursorPosition.nColumn) {
                        //                            qint32 nX = rectSymbol.x();

                        //                            if (state.varCursorExtraInfo.toInt() == BYTEPOS_LOW) {
                        //                                nX += getCharWidth();
                        //                            }

                        //                            QRect rectCursor;
                        //                            rectCursor.setRect(nX, rectSymbol.y() + getLineDelta() + rectSymbol.height(), getCharWidth(), m_nCursorHeight);

                        //                            setCursorData(rectCursor, QRect(), sSymbol, nIndex);
                        //                        }
                        //                    }
                    }
                }

                pPainter->drawText(rectSymbol, sSymbol);
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
        STATE state = getState();
        XVPOS nViewStart = getViewPosStart();

        if (pEvent->matches(QKeySequence::MoveToNextChar) || ((pEvent->key() >= Qt::Key_A) && (pEvent->key() <= Qt::Key_F)) ||
            ((pEvent->key() >= Qt::Key_0) && (pEvent->key() <= Qt::Key_9)) || (pEvent->key() == Qt::Key_Delete)) {
            if (((pEvent->key() >= Qt::Key_A) && (pEvent->key() <= Qt::Key_F)) || ((pEvent->key() >= Qt::Key_0) && (pEvent->key() <= Qt::Key_9)) ||
                (pEvent->key() == Qt::Key_Delete)) {
                if (writeHexKey(state.nSelectionViewPos, (BYTEPOS)(state.varCursorExtraInfo.toInt()), pEvent->key())) {
                    setEdited(state.nSelectionViewPos, 1);  // TODO Check mb Global

                    emit dataChanged(state.nSelectionViewPos, 1);
                }
            }

            if (state.varCursorExtraInfo.toInt() == BYTEPOS_HIGH) {
                state.varCursorExtraInfo = BYTEPOS_LOW;
            } else {
                state.varCursorExtraInfo = BYTEPOS_HIGH;
                state.nSelectionViewPos++;
            }
        } else if (pEvent->matches(QKeySequence::MoveToPreviousChar) || (pEvent->key() == Qt::Key_Backspace)) {
            if ((pEvent->key() == Qt::Key_Backspace)) {
                if (writeHexKey(state.nSelectionViewPos, (BYTEPOS)(state.varCursorExtraInfo.toInt()), pEvent->key())) {
                    setEdited(state.nSelectionViewPos, 1);  // TODO Check mb Global

                    emit dataChanged(state.nSelectionViewPos, 1);
                }
            }

            if (state.varCursorExtraInfo.toInt() == BYTEPOS_LOW) {
                state.varCursorExtraInfo = BYTEPOS_HIGH;
            } else {
                state.varCursorExtraInfo = BYTEPOS_LOW;
                state.nSelectionViewPos--;
            }
        } else if (pEvent->matches(QKeySequence::MoveToNextLine)) {
            state.nSelectionViewPos = state.nSelectionViewPos + m_nBytesProLine;
        } else if (pEvent->matches(QKeySequence::MoveToPreviousLine)) {
            state.nSelectionViewPos = state.nSelectionViewPos - m_nBytesProLine;
        } else if (pEvent->matches(QKeySequence::MoveToStartOfLine)) {
            // TODO
        } else if (pEvent->matches(QKeySequence::MoveToEndOfLine)) {
            // TODO
        }

        if ((state.nSelectionViewPos < 0) || (pEvent->matches(QKeySequence::MoveToStartOfDocument))) {
            state.varCursorExtraInfo = BYTEPOS_HIGH;
            state.nSelectionViewPos = 0;
        }

        if ((state.nSelectionViewPos >= getViewSize()) || (pEvent->matches(QKeySequence::MoveToEndOfDocument))) {
            state.varCursorExtraInfo = BYTEPOS_LOW;
            state.nSelectionViewPos = getViewSize() - 1;
        }

        if (pEvent->matches(QKeySequence::MoveToNextChar) || pEvent->matches(QKeySequence::MoveToPreviousChar) || pEvent->matches(QKeySequence::MoveToNextLine) ||
            pEvent->matches(QKeySequence::MoveToPreviousLine) || ((pEvent->key() >= Qt::Key_A) && (pEvent->key() <= Qt::Key_F)) ||
            ((pEvent->key() >= Qt::Key_0) && (pEvent->key() <= Qt::Key_9)) || (pEvent->key() == Qt::Key_Delete) || (pEvent->key() == Qt::Key_Backspace)) {
            qint64 nRelOffset = state.nSelectionViewPos - nViewStart;

            if (nRelOffset >= m_nBytesProLine * getLinesProPage()) {
                _goToViewPos(nViewStart + m_nBytesProLine, true);
            } else if (nRelOffset < 0) {
                if (!_goToViewPos(nViewStart - m_nBytesProLine, true)) {
                    _goToViewPos(0);
                }
            }
        } else if (pEvent->matches(QKeySequence::MoveToNextPage) || pEvent->matches(QKeySequence::MoveToPreviousPage)) {
            if (pEvent->matches(QKeySequence::MoveToNextPage)) {
                _goToViewPos(nViewStart + m_nBytesProLine * getLinesProPage());
            } else if (pEvent->matches(QKeySequence::MoveToPreviousPage)) {
                _goToViewPos(nViewStart - m_nBytesProLine * getLinesProPage());
            }
        } else if (pEvent->matches(QKeySequence::MoveToStartOfDocument) || pEvent->matches(QKeySequence::MoveToEndOfDocument))  // TODO
        {
            _goToViewPos(state.nSelectionViewPos);
        }

        setState(state);

        viewport()->update();

        adjust();
    }
    //    else if(pEvent->matches(QKeySequence::SelectAll))
    //    {
    //        // TODO
    //        _selectAllSlot();
    //    }
    else {
        XAbstractTableView::keyPressEvent(pEvent);
    }
}

XVPOS XHexEdit::getCurrentViewPosFromScroll()
{
    XVPOS nResult = 0;

    qint32 nValue = verticalScrollBar()->value();

    qint64 nMaxValue = getMaxScrollValue() * m_nBytesProLine;

    if (getViewSize() > nMaxValue) {
        if (nValue == getMaxScrollValue()) {
            nResult = getViewSize() - m_nBytesProLine;
        } else {
            nResult = ((double)nValue / (double)getMaxScrollValue()) * getViewSize();
        }
    } else {
        nResult = (XVPOS)nValue * m_nBytesProLine;
    }

    return nResult;
}

void XHexEdit::setCurrentViewPosToScroll(XVPOS nOffset)
{
    setViewPosStart(nOffset);

    qint32 nValue = 0;

    if (getViewSize() > (getMaxScrollValue() * m_nBytesProLine)) {
        if (nOffset == getViewSize() - m_nBytesProLine) {
            nValue = getMaxScrollValue();
        } else {
            nValue = ((double)(nOffset) / ((double)getViewSize())) * (double)getMaxScrollValue();
        }
    } else {
        nValue = (nOffset) / m_nBytesProLine;
    }

    verticalScrollBar()->setValue(nValue);

    adjust(true);
}

void XHexEdit::adjustColumns()
{
    const QFontMetricsF fm(getTextFont());

    if (XBinary::getWidthModeFromSize(getViewSize()) == XBinary::MODE_64) {
        m_nLocationWidth = 16;
        setColumnWidth(COLUMN_ADDRESS, 2 * getCharWidth() + fm.boundingRect("00000000:00000000").width());
    } else {
        m_nLocationWidth = 8;
        setColumnWidth(COLUMN_ADDRESS, 2 * getCharWidth() + fm.boundingRect("0000:0000").width());
    }

    setColumnWidth(COLUMN_HEX, m_nBytesProLine * 2 * getCharWidth() + 2 * getCharWidth() + getSideDelta() * m_nBytesProLine);
}

void XHexEdit::registerShortcuts(bool bState)
{
    Q_UNUSED(bState)
}
