/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Christian Muehlhaeuser <muesli@tomahawk-player.org>
 *   Copyright 2010-2011, Jeff Mitchell <jeff@tomahawk-player.org>
 *
 *   Tomahawk is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Tomahawk is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AlbumView.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <qmath.h>

#include "audio/AudioEngine.h"
#include "context/ContextWidget.h"
#include "TomahawkSettings.h"
#include "Artist.h"
#include "PlayableItem.h"
#include "AlbumItemDelegate.h"
#include "AlbumModel.h"
#include "ContextMenu.h"
#include "ViewManager.h"
#include "utils/Logger.h"
#include "utils/AnimatedSpinner.h"

#define SCROLL_TIMEOUT 280

using namespace Tomahawk;


AlbumView::AlbumView( QWidget* parent )
    : QListView( parent )
    , m_model( 0 )
    , m_proxyModel( 0 )
    , m_delegate( 0 )
    , m_loadingSpinner( new AnimatedSpinner( this ) )
    , m_overlay( new OverlayWidget( this ) )
    , m_contextMenu( new ContextMenu( this ) )
    , m_inited( false )
{
    setFrameShape( QFrame::NoFrame );
    setAttribute( Qt::WA_MacShowFocusRect, 0 );

    setDragEnabled( true );
    setDropIndicatorShown( false );
    setDragDropOverwriteMode( false );
    setUniformItemSizes( true );
    setSpacing( 0 );
    setContentsMargins( 0, 0, 0, 0 );
    setMouseTracking( true );
    setContextMenuPolicy( Qt::CustomContextMenu );
    setResizeMode( Adjust );
    setViewMode( IconMode );
    setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
    setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );

    setStyleSheet( "QListView { background-color: #323435; }" );

    setAutoFitItems( true );
    setProxyModel( new AlbumProxyModel( this ) );

    connect( this, SIGNAL( doubleClicked( QModelIndex ) ), SLOT( onItemActivated( QModelIndex ) ) );
    connect( this, SIGNAL( customContextMenuRequested( QPoint ) ), SLOT( onCustomContextMenu( QPoint ) ) );
    connect( this, SIGNAL( customContextMenuRequested( QPoint ) ), SLOT( onCustomContextMenu( QPoint ) ) );
    connect( proxyModel(), SIGNAL( modelReset() ), SLOT( layoutItems() ) );
//    connect( m_contextMenu, SIGNAL( triggered( int ) ), SLOT( onMenuTriggered( int ) ) );
}


AlbumView::~AlbumView()
{
    qDebug() << Q_FUNC_INFO;
}


void
AlbumView::setProxyModel( AlbumProxyModel* model )
{
    m_proxyModel = model;
    m_delegate = new AlbumItemDelegate( this, m_proxyModel );
    connect( m_delegate, SIGNAL( updateIndex( QModelIndex ) ), this, SLOT( update( QModelIndex ) ) );
    setItemDelegate( m_delegate );

    QListView::setModel( m_proxyModel );
}


void
AlbumView::setModel( QAbstractItemModel* model )
{
    Q_UNUSED( model );
    qDebug() << "Explicitly use setAlbumModel instead";
    Q_ASSERT( false );
}


void
AlbumView::setAlbumModel( AlbumModel* model )
{
    m_inited = false;
    m_model = model;

    if ( m_proxyModel )
    {
        m_proxyModel->setSourceAlbumModel( m_model );
        m_proxyModel->sort( 0 );
    }

    connect( m_proxyModel, SIGNAL( filterChanged( QString ) ), SLOT( onFilterChanged( QString ) ) );

    connect( m_model, SIGNAL( itemCountChanged( unsigned int ) ), SLOT( onItemCountChanged( unsigned int ) ) );
    connect( m_model, SIGNAL( loadingStarted() ), m_loadingSpinner, SLOT( fadeIn() ) );
    connect( m_model, SIGNAL( loadingFinished() ), m_loadingSpinner, SLOT( fadeOut() ) );

    emit modelChanged();
}


void
AlbumView::currentChanged( const QModelIndex& current, const QModelIndex& previous )
{
    QListView::currentChanged( current, previous );

    PlayableItem* item = m_model->itemFromIndex( m_proxyModel->mapToSource( current ) );
    if ( item )
    {
        if ( !item->album().isNull() )
            ViewManager::instance()->context()->setAlbum( item->album() );
    }
}


void
AlbumView::onItemActivated( const QModelIndex& index )
{
    PlayableItem* item = m_model->itemFromIndex( m_proxyModel->mapToSource( index ) );
    if ( item )
    {
//        qDebug() << "Result activated:" << item->album()->tracks().first()->toString() << item->album()->tracks().first()->results().first()->url();
//        APP->audioEngine()->playItem( item->album().data(), item->album()->tracks().first()->results().first() );

        if ( !item->album().isNull() )
            ViewManager::instance()->show( item->album() );
        else if ( !item->artist().isNull() )
            ViewManager::instance()->show( item->artist() );
        else if ( !item->query().isNull() )
            AudioEngine::instance()->playItem( playlistinterface_ptr(), item->query() );
    }
}


void
AlbumView::onItemCountChanged( unsigned int items )
{
    if ( items == 0 )
    {
        if ( m_model->collection().isNull() || ( !m_model->collection().isNull() && m_model->collection()->source()->isLocal() ) )
            m_overlay->setText( tr( "After you have scanned your music collection you will find your latest album additions right here." ) );
        else
            m_overlay->setText( tr( "This collection doesn't have any recent albums." ) );

        m_overlay->show();
    }
    else
        m_overlay->hide();
}


void
AlbumView::scrollContentsBy( int dx, int dy )
{
    QListView::scrollContentsBy( dx, dy );
    emit scrolledContents( dx, dy );
}


void
AlbumView::paintEvent( QPaintEvent* event )
{
    if ( !autoFitItems() || m_inited || !m_proxyModel->rowCount() )
        QListView::paintEvent( event );
}


void
AlbumView::resizeEvent( QResizeEvent* event )
{
    QListView::resizeEvent( event );
    layoutItems();
}


void
AlbumView::layoutItems()
{
    if ( autoFitItems() && m_model )
    {
#ifdef Q_WS_X11
//        int scrollbar = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() + 16 : 0;
        int scrollbar = 0; verticalScrollBar()->rect().width();
#else
        int scrollbar = verticalScrollBar()->rect().width();
#endif
        int rectWidth = contentsRect().width() - scrollbar - 3;
        int itemWidth = 160;
        QSize itemSize = m_proxyModel->data( QModelIndex(), Qt::SizeHintRole ).toSize();
        Q_UNUSED( itemSize ); // looks obsolete

        int itemsPerRow = qMax( 1, qFloor( rectWidth / itemWidth ) );
//        int rightSpacing = rectWidth - ( itemsPerRow * ( itemSize.width() + 16 ) );
//        int newSpacing = 16 + floor( rightSpacing / ( itemsPerRow + 1 ) );

        int remSpace = rectWidth - ( itemsPerRow * itemWidth );
        int extraSpace = remSpace / itemsPerRow;
        int newItemWidth = itemWidth + extraSpace;
        
        m_model->setItemSize( QSize( newItemWidth, newItemWidth ) );

        if ( !m_inited )
        {
            m_inited = true;
            repaint();
        }
    }
}


void
AlbumView::onFilterChanged( const QString& )
{
    if ( selectedIndexes().count() )
        scrollTo( selectedIndexes().at( 0 ), QAbstractItemView::PositionAtCenter );
}


void
AlbumView::startDrag( Qt::DropActions supportedActions )
{
    QList<QPersistentModelIndex> pindexes;
    QModelIndexList indexes;
    foreach( const QModelIndex& idx, selectedIndexes() )
    {
        if ( ( m_proxyModel->flags( idx ) & Qt::ItemIsDragEnabled ) )
        {
            indexes << idx;
            pindexes << idx;
        }
    }

    if ( indexes.count() == 0 )
        return;

    qDebug() << "Dragging" << indexes.count() << "indexes";
    QMimeData* data = m_proxyModel->mimeData( indexes );
    if ( !data )
        return;

    QDrag* drag = new QDrag( this );
    drag->setMimeData( data );
    const QPixmap p = TomahawkUtils::createDragPixmap( TomahawkUtils::MediaTypeAlbum, indexes.count() );
    drag->setPixmap( p );
    drag->setHotSpot( QPoint( -20, -20 ) );

    /* Qt::DropAction action = */ drag->exec( supportedActions, Qt::CopyAction );
}


void
AlbumView::onCustomContextMenu( const QPoint& pos )
{
    m_contextMenu->clear();

    QModelIndex idx = indexAt( pos );
    idx = idx.sibling( idx.row(), 0 );
    m_contextMenuIndex = idx;

    if ( !idx.isValid() )
        return;

    QList<query_ptr> queries;
    QList<artist_ptr> artists;
    QList<album_ptr> albums;

    foreach ( const QModelIndex& index, selectedIndexes() )
    {
        if ( index.column() || selectedIndexes().contains( index.parent() ) )
            continue;

        PlayableItem* item = m_model->itemFromIndex( m_proxyModel->mapToSource( index ) );

        if ( item && !item->query().isNull() )
            queries << item->query();
        else if ( item && !item->artist().isNull() )
            artists << item->artist();
        else if ( item && !item->album().isNull() )
            albums << item->album();
    }

    m_contextMenu->setQueries( queries );
    m_contextMenu->setArtists( artists );
    m_contextMenu->setAlbums( albums );

    m_contextMenu->exec( viewport()->mapToGlobal( pos ) );
}
