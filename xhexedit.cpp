// copyright (c) 2021 hors<horsicq@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include "xhexedit.h"

XHexEdit::XHexEdit(QWidget *pParent) : XAbstractTableView(pParent)
{
    g_pDevice=nullptr;
    g_nDataSize=0;
    g_nBytesProLine=16;
    g_nDataBlockSize=0;
    g_nAddressWidth=8;

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

bool XHexEdit::isOffsetValid(qint64 nOffset)
{
    bool bResult=false;

    if((nOffset>=0)&&(nOffset<g_nDataSize))
    {
        bResult=true;
    }

    return bResult;
}

bool XHexEdit::isEnd(qint64 nOffset)
{
    return (nOffset==g_nDataSize);
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
        }
        else if(cursorPosition.nColumn==COLUMN_HEX)
        {
            osResult.nOffset=nBlockOffset+(cursorPosition.nCellLeft)/(getCharWidth()*2+getLineDelta());
            osResult.nSize=1;
        }

        if(!isOffsetValid(osResult.nOffset))
        {
            osResult.nOffset=g_nDataSize; // TODO Check
            osResult.nSize=0;
        }
    }

    return osResult;
}

void XHexEdit::updateData()
{
    if(g_pDevice)
    {
        // Update cursor position
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

            for(int i=0;i<g_nDataBlockSize;i+=g_nBytesProLine)
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

                for(int i=0;i<nDataBlockSize;i++)
                {
                    qint32 nIndex=nRow*g_nBytesProLine+i;

                    QString sHex=g_baDataHexBuffer.mid(nIndex*2,2);
                    QString sSymbol;

                    bool bBold=(sHex!="00");
                    bool bSelected=isOffsetSelected(nDataBlockStartOffset+nIndex);
                    bool bCursor=(state.nCursorOffset==(nDataBlockStartOffset+nIndex)); // TODO

                    if(bBold)
                    {
                        pPainter->save();
                        QFont font=pPainter->font();
                        font.setBold(true);
                        pPainter->setFont(font);
                    }

                    QRect rectSymbol;

                    if(nColumn==COLUMN_HEX)
                    {
                        rectSymbol.setRect(nLeft+getCharWidth()+(i*2)*getCharWidth()+i*getLineDelta(),nTop,2*getCharWidth()+getLineDelta(),nHeight);
                        sSymbol=sHex;
                    }

                    if(bSelected||bCursor)
                    {
                        QRect rectSelected;
                        rectSelected.setRect(rectSymbol.x(),rectSymbol.y()+getLineDelta(),rectSymbol.width(),rectSymbol.height());

                        if(bCursor)
                        {
                            if(nColumn==state.cursorPosition.nColumn)
                            {
                                setCursorData(rectSelected,sSymbol,nIndex);
                            }
                        }

                        if(bSelected)
                        {
                            pPainter->fillRect(rectSelected,viewport()->palette().color(QPalette::Highlight));
                        }
                    }

                    pPainter->drawText(rectSymbol.x(),rectSymbol.y()+nHeight,sSymbol);

                    if(bBold)
                    {
                        pPainter->restore();
                    }
                }
            }
        }
}

void XHexEdit::keyPressEvent(QKeyEvent *pEvent)
{

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

    if(XBinary::getModeFromSize(g_nDataSize)==XBinary::MODE_64) // TODO Check adjust start address
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
