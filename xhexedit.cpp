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
    g_pDevice=nullptr;
    g_nDataSize=0;
    g_nBytesProLine=16;
    g_nDataBlockSize=0;
    g_nAddressWidth=8;
    g_nCursorHeight=2;

    addColumn(tr("Offset"));
    addColumn(tr("Hex"));

    setTextFont(getMonoFont(10));
}

void XHexEdit::setData(QIODevice *pDevice)
{
    g_pDevice=pDevice;

    g_nDataSize=pDevice->size();

    resetCursorData();

    adjustColumns();

    qint64 nTotalLineCount=g_nDataSize/g_nBytesProLine;

    if(g_nDataSize%g_nBytesProLine==0)
    {
        nTotalLineCount--;
    }

    setTotalLineCount(nTotalLineCount);

    reload(true);
}

XAbstractTableView::OS XHexEdit::cursorPositionToOS(XAbstractTableView::CURSOR_POSITION cursorPosition)
{
    OS osResult={};

    osResult.nOffset=-1;

    if((cursorPosition.bIsValid)&&(cursorPosition.ptype==PT_CELL))
    {
        qint64 nBlockOffset=getViewStart()+(cursorPosition.nRow*g_nBytesProLine);

        if(cursorPosition.nColumn==COLUMN_ADDRESS)
        {
            osResult.nOffset=nBlockOffset;
            osResult.nSize=1;
            osResult.varData=BYTEPOS_HIGH;
        }
        else if(cursorPosition.nColumn==COLUMN_HEX)
        {
            qint32 nOffset=(cursorPosition.nCellLeft-getLineDelta())/(getCharWidth()*2+getLineDelta());
            qint32 nPos=(cursorPosition.nCellLeft-getLineDelta())%(getCharWidth()*2+getLineDelta());

            osResult.nOffset=nBlockOffset+nOffset;
            osResult.nSize=1;

            if(nPos>getCharWidth())
            {
                osResult.varData=BYTEPOS_LOW;
            }
            else
            {
                osResult.varData=BYTEPOS_HIGH;
            }
        }

        if(!isOffsetValid(osResult.nOffset))
        {
            osResult.nOffset=g_nDataSize; // TODO Check
            osResult.nSize=0;
            osResult.varData=BYTEPOS_HIGH;
        }
    }

    return osResult;
}

void XHexEdit::updateData()
{
    if(g_pDevice)
    {
        // Update cursor position !!!
        qint64 nBlockOffset=getViewStart();
        qint64 nCursorOffset=nBlockOffset+getCursorDelta();

        if(nCursorOffset>=g_nDataSize)
        {
            nCursorOffset=g_nDataSize-1;
        }

        setCursorOffset(nCursorOffset);

        g_listAddresses.clear();

        if(g_pDevice->seek(nBlockOffset))
        {
            qint32 nDataBlockSize=g_nBytesProLine*getLinesProPage();

            QByteArray baDataBuffer;
            baDataBuffer.resize(nDataBlockSize);
            g_nDataBlockSize=(int)g_pDevice->read(baDataBuffer.data(),nDataBlockSize);
            baDataBuffer.resize(g_nDataBlockSize);
            g_baDataHexBuffer=QByteArray(baDataBuffer.toHex());

            for(qint32 i=0;i<g_nDataBlockSize;i+=g_nBytesProLine)
            {
                QString sAddress=QString("%1").arg(i+nBlockOffset,g_nAddressWidth,16,QChar('0'));

                g_listAddresses.append(sAddress);
            }
        }
        else
        {
            g_baDataHexBuffer.clear();
        }
    }
}

void XHexEdit::paintCell(QPainter *pPainter, qint32 nRow, qint32 nColumn, qint32 nLeft, qint32 nTop, qint32 nWidth, qint32 nHeight)
{
    if(nColumn==COLUMN_ADDRESS)
    {
        if(nRow<g_listAddresses.count())
        {
            pPainter->drawText(nLeft+getCharWidth(),nTop+nHeight,g_listAddresses.at(nRow)); // TODO Text Optional
        }
    }
    else if((nColumn==COLUMN_HEX))
    {
        STATE state=getState();

        if(nRow*g_nBytesProLine<g_nDataBlockSize)
        {
            qint64 nDataBlockStartOffset=getViewStart();
            qint64 nDataBlockSize=qMin(g_nDataBlockSize-nRow*g_nBytesProLine,g_nBytesProLine);

            for(qint32 i=0;i<nDataBlockSize;i++)
            {
                qint32 nIndex=nRow*g_nBytesProLine+i;

                QString sHex=g_baDataHexBuffer.mid(nIndex*2,2);
                QString sSymbol;

                bool bSelected=isOffsetSelected(nDataBlockStartOffset+nIndex);
                bool bCursor=(state.nCursorOffset==(nDataBlockStartOffset+nIndex)); // TODO

                QRect rectSymbol;

                if(nColumn==COLUMN_HEX)
                {
                    rectSymbol.setRect(nLeft+getCharWidth()+(i*2)*getCharWidth()+i*getLineDelta(),nTop,2*getCharWidth()+getLineDelta(),nHeight);
                    sSymbol=sHex;
                }

                if(bSelected|bCursor)
                {
                    QRect rectSelected;
                    rectSelected.setRect(rectSymbol.x(),rectSymbol.y()+getLineDelta(),rectSymbol.width(),rectSymbol.height());

                    if(bSelected)
                    {
                        pPainter->fillRect(rectSelected,viewport()->palette().color(QPalette::Highlight));
                    }

                    if(bCursor)
                    {
                        if(nColumn==state.cursorPosition.nColumn)
                        {
                            qint32 nX=rectSymbol.x();

                            if(state.varCursorExtraInfo.toInt()==BYTEPOS_LOW)
                            {
                                nX+=getCharWidth();
                            }

                            QRect rectCursor;
                            rectCursor.setRect(nX,rectSymbol.y()+getLineDelta()+rectSymbol.height(),getCharWidth(),g_nCursorHeight);

                            setCursorData(rectCursor,QRect(),sSymbol,nIndex);
                        }
                    }
                }

                pPainter->drawText(rectSymbol.x(),rectSymbol.y()+nHeight,sSymbol);
            }
        }
    }
}

void XHexEdit::keyPressEvent(QKeyEvent *pEvent)
{
    STATE state=getState();
    // Move commands
    if( pEvent->matches(QKeySequence::MoveToNextChar)||
        pEvent->matches(QKeySequence::MoveToPreviousChar)||
        pEvent->matches(QKeySequence::MoveToNextLine)||
        pEvent->matches(QKeySequence::MoveToPreviousLine)||
        pEvent->matches(QKeySequence::MoveToStartOfLine)||
        pEvent->matches(QKeySequence::MoveToEndOfLine)||
        pEvent->matches(QKeySequence::MoveToNextPage)||
        pEvent->matches(QKeySequence::MoveToPreviousPage)||
        pEvent->matches(QKeySequence::MoveToStartOfDocument)||
        pEvent->matches(QKeySequence::MoveToEndOfDocument))
    {
        qint64 nViewStart=getViewStart();

        if(pEvent->matches(QKeySequence::MoveToNextChar))
        {
            if(state.varCursorExtraInfo.toInt()==BYTEPOS_HIGH)
            {
                state.varCursorExtraInfo=BYTEPOS_LOW;
            }
            else
            {
                state.varCursorExtraInfo=BYTEPOS_HIGH;
                state.nCursorOffset++;
            }

            setCursorOffset(state.nCursorOffset,-1,state.varCursorExtraInfo);
        }
        else if(pEvent->matches(QKeySequence::MoveToPreviousChar))
        {
            STATE state=getState();

            if(state.varCursorExtraInfo.toInt()==BYTEPOS_LOW)
            {
                state.varCursorExtraInfo=BYTEPOS_HIGH;
            }
            else
            {
                state.varCursorExtraInfo=BYTEPOS_LOW;
                state.nCursorOffset--;
            }

            setCursorOffset(state.nCursorOffset,-1,state.varCursorExtraInfo);
        }
        else if(pEvent->matches(QKeySequence::MoveToNextLine))
        {
            setCursorOffset(getCursorOffset()+g_nBytesProLine,-1,state.varCursorExtraInfo);
        }
        else if(pEvent->matches(QKeySequence::MoveToPreviousLine))
        {
            setCursorOffset(getCursorOffset()-g_nBytesProLine,-1,state.varCursorExtraInfo);
        }
        else if(pEvent->matches(QKeySequence::MoveToStartOfLine))
        {
            setCursorOffset(getCursorOffset()-(getCursorDelta()%g_nBytesProLine),-1,state.varCursorExtraInfo);
        }
        else if(pEvent->matches(QKeySequence::MoveToEndOfLine))
        {
            setCursorOffset(getCursorOffset()-(getCursorDelta()%g_nBytesProLine)+g_nBytesProLine-1,-1,state.varCursorExtraInfo);
        }

        if((getCursorOffset()<0)||(pEvent->matches(QKeySequence::MoveToStartOfDocument)))
        {
            state.varCursorExtraInfo=BYTEPOS_HIGH;
            setCursorOffset(0,-1,state.varCursorExtraInfo);
        }

        if((getCursorOffset()>=g_nDataSize)||(pEvent->matches(QKeySequence::MoveToEndOfDocument)))
        {
            state.varCursorExtraInfo=BYTEPOS_LOW;
            setCursorOffset(g_nDataSize-1,-1,state.varCursorExtraInfo);
        }

        if( pEvent->matches(QKeySequence::MoveToNextChar)||
            pEvent->matches(QKeySequence::MoveToPreviousChar)||
            pEvent->matches(QKeySequence::MoveToNextLine)||
            pEvent->matches(QKeySequence::MoveToPreviousLine))
        {
            qint64 nRelOffset=getCursorOffset()-nViewStart;

            if(nRelOffset>=g_nBytesProLine*getLinesProPage())
            {
                _goToOffset(nViewStart+g_nBytesProLine,true);
            }
            else if(nRelOffset<0)
            {
                if(!_goToOffset(nViewStart-g_nBytesProLine,true))
                {
                    _goToOffset(0);
                }
            }
        }
        else if(pEvent->matches(QKeySequence::MoveToNextPage)||
                pEvent->matches(QKeySequence::MoveToPreviousPage))
        {
            if(pEvent->matches(QKeySequence::MoveToNextPage))
            {
                _goToOffset(nViewStart+g_nBytesProLine*getLinesProPage());
            }
            else if(pEvent->matches(QKeySequence::MoveToPreviousPage))
            {
                _goToOffset(nViewStart-g_nBytesProLine*getLinesProPage());
            }
        }
        else if(pEvent->matches(QKeySequence::MoveToStartOfDocument)||
                pEvent->matches(QKeySequence::MoveToEndOfDocument)) // TODO
        {
            _goToOffset(getCursorOffset());
        }

        adjust();
        viewport()->update();
    }
    else if(pEvent->matches(QKeySequence::SelectAll))
    {
        //TODO
        //_selectAllSlot();
    }
    else
    {
        XAbstractTableView::keyPressEvent(pEvent);
    }
}

qint64 XHexEdit::getScrollValue()
{
    qint64 nResult=0;

    qint32 nValue=verticalScrollBar()->value();

    qint64 nMaxValue=getMaxScrollValue()*g_nBytesProLine;

    if(g_nDataSize>nMaxValue)
    {
        if(nValue==getMaxScrollValue())
        {
            nResult=g_nDataSize-g_nBytesProLine;
        }
        else
        {
            nResult=((double)nValue/(double)getMaxScrollValue())*g_nDataSize;
        }
    }
    else
    {
        nResult=(qint64)nValue*g_nBytesProLine;
    }

    return nResult;
}

void XHexEdit::setScrollValue(qint64 nOffset)
{
    setViewStart(nOffset);

    qint32 nValue=0;

    if(g_nDataSize>(getMaxScrollValue()*g_nBytesProLine))
    {
        if(nOffset==g_nDataSize-g_nBytesProLine)
        {
            nValue=getMaxScrollValue();
        }
        else
        {
            nValue=((double)(nOffset)/((double)g_nDataSize))*(double)getMaxScrollValue();
        }
    }
    else
    {
        nValue=(nOffset)/g_nBytesProLine;
    }

    verticalScrollBar()->setValue(nValue);

    adjust(true);
}

void XHexEdit::adjustColumns()
{
    const QFontMetricsF fm(getTextFont());

    if(XBinary::getWidthModeFromSize(g_nDataSize)==XBinary::MODE_64) // TODO Check adjust start address
    {
        g_nAddressWidth=16;
        setColumnWidth(COLUMN_ADDRESS,2*getCharWidth()+fm.boundingRect("0000000000000000").width());
    }
    else
    {
        g_nAddressWidth=8;
        setColumnWidth(COLUMN_ADDRESS,2*getCharWidth()+fm.boundingRect("00000000").width());
    }

    setColumnWidth(COLUMN_HEX,g_nBytesProLine*2*getCharWidth()+2*getCharWidth()+getLineDelta()*g_nBytesProLine);
}

void XHexEdit::registerShortcuts(bool bState)
{
    Q_UNUSED(bState)
}
