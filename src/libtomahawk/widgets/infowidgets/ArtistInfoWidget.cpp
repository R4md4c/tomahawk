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

#include "ArtistInfoWidget.h"
#include "ArtistInfoWidget_p.h"
#include "ui_ArtistInfoWidget.h"

#include "audio/AudioEngine.h"
#include "playlist/TrackHeader.h"
#include "playlist/TreeModel.h"
#include "playlist/PlaylistModel.h"
#include "playlist/TreeProxyModel.h"
#include "Source.h"

#include "database/DatabaseCommand_AllTracks.h"
#include "database/DatabaseCommand_AllAlbums.h"

#include "utils/StyleHelper.h"
#include "utils/TomahawkUtilsGui.h"
#include "utils/Logger.h"

#include "widgets/OverlayButton.h"
#include "widgets/OverlayWidget.h"

#include "Pipeline.h"

using namespace Tomahawk;


ArtistInfoWidget::ArtistInfoWidget( const Tomahawk::artist_ptr& artist, QWidget* parent )
    : QWidget( parent )
    , ui( new Ui::ArtistInfoWidget )
    , m_artist( artist )
    , m_infoId( uuid() )
{
    ui->setupUi( this );

    m_plInterface = Tomahawk::playlistinterface_ptr( new MetaPlaylistInterface( this ) );

    TomahawkUtils::unmarginLayout( layout() );
    TomahawkUtils::unmarginLayout( ui->layoutWidget->layout() );
    TomahawkUtils::unmarginLayout( ui->layoutWidget1->layout() );
    TomahawkUtils::unmarginLayout( ui->layoutWidget2->layout() );
    TomahawkUtils::unmarginLayout( ui->albumHeader->layout() );

    m_albumsModel = new AlbumModel( ui->albums );
    ui->albums->setAlbumModel( m_albumsModel );

    m_relatedModel = new AlbumModel( ui->relatedArtists );
//    m_relatedModel->setColumnStyle( TreeModel::TrackOnly );
    ui->relatedArtists->setAlbumModel( m_relatedModel );
//    ui->relatedArtists->setSortingEnabled( false );
    ui->relatedArtists->proxyModel()->sort( -1 );

    m_topHitsModel = new PlaylistModel( ui->topHits );
    m_topHitsModel->setStyle( PlayableModel::Short );
    ui->topHits->setPlayableModel( m_topHitsModel );
    ui->topHits->setSortingEnabled( false );

    m_pixmap = TomahawkUtils::defaultPixmap( TomahawkUtils::DefaultArtistImage, TomahawkUtils::ScaledCover, QSize( 48, 48 ) );

    connect( m_albumsModel, SIGNAL( loadingStarted() ), SLOT( onLoadingStarted() ) );
    connect( m_albumsModel, SIGNAL( loadingFinished() ), SLOT( onLoadingFinished() ) );

    connect( Tomahawk::InfoSystem::InfoSystem::instance(),
             SIGNAL( info( Tomahawk::InfoSystem::InfoRequestData, QVariant ) ),
             SLOT( infoSystemInfo( Tomahawk::InfoSystem::InfoRequestData, QVariant ) ) );

    load( artist );
}


ArtistInfoWidget::~ArtistInfoWidget()
{
    delete ui;
}


Tomahawk::playlistinterface_ptr
ArtistInfoWidget::playlistInterface() const
{
    return m_plInterface;
}


void
ArtistInfoWidget::onLoadingStarted()
{
}


void
ArtistInfoWidget::onLoadingFinished()
{
}


bool
ArtistInfoWidget::isBeingPlayed() const
{
    if ( ui->albums->playlistInterface() == AudioEngine::instance()->currentTrackPlaylist() )
        return true;

    if ( ui->relatedArtists->playlistInterface() == AudioEngine::instance()->currentTrackPlaylist() )
        return true;

    if ( ui->topHits->playlistInterface() == AudioEngine::instance()->currentTrackPlaylist() )
        return true;

    return false;
}


bool
ArtistInfoWidget::jumpToCurrentTrack()
{
    if ( ui->albums->jumpToCurrentTrack() )
        return true;

    if ( ui->relatedArtists->jumpToCurrentTrack() )
        return true;

    if ( ui->topHits->jumpToCurrentTrack() )
        return true;

    return false;
}


void
ArtistInfoWidget::load( const artist_ptr& artist )
{
    if ( !m_artist.isNull() )
        disconnect( m_artist.data(), SIGNAL( updated() ), this, SLOT( onArtistImageUpdated() ) );

    m_artist = artist;
    m_title = artist->name();

    connect( artist.data(), SIGNAL( albumsAdded( QList<Tomahawk::album_ptr>, Tomahawk::ModelMode ) ),
                              SLOT( onAlbumsFound( QList<Tomahawk::album_ptr>, Tomahawk::ModelMode ) ) );

    onAlbumsFound( artist->albums( Mixed ), Mixed );

    Tomahawk::InfoSystem::InfoStringHash artistInfo;
    artistInfo["artist"] = artist->name();

    Tomahawk::InfoSystem::InfoRequestData requestData;
    requestData.caller = m_infoId;
    requestData.customData = QVariantMap();

    requestData.input = artist->name();
    requestData.type = Tomahawk::InfoSystem::InfoArtistBiography;
    Tomahawk::InfoSystem::InfoSystem::instance()->getInfo( requestData );

    requestData.input = QVariant::fromValue< Tomahawk::InfoSystem::InfoStringHash >( artistInfo );

    requestData.type = Tomahawk::InfoSystem::InfoArtistSimilars;
    requestData.requestId = TomahawkUtils::infosystemRequestId();
    Tomahawk::InfoSystem::InfoSystem::instance()->getInfo( requestData );

    requestData.type = Tomahawk::InfoSystem::InfoArtistSongs;
    requestData.requestId = TomahawkUtils::infosystemRequestId();
    Tomahawk::InfoSystem::InfoSystem::instance()->getInfo( requestData );

    connect( m_artist.data(), SIGNAL( updated() ), SLOT( onArtistImageUpdated() ) );
    onArtistImageUpdated();
}


void
ArtistInfoWidget::onAlbumsFound( const QList<Tomahawk::album_ptr>& albums, ModelMode mode )
{
    Q_UNUSED( mode );

    m_albumsModel->addAlbums( albums );
}


void
ArtistInfoWidget::infoSystemInfo( Tomahawk::InfoSystem::InfoRequestData requestData, QVariant output )
{
    if ( requestData.caller != m_infoId )
        return;

    InfoSystem::InfoStringHash trackInfo;
    trackInfo = requestData.input.value< InfoSystem::InfoStringHash >();

    if ( output.canConvert< QVariantMap >() )
    {
        const QString artist = requestData.input.toString();
        if ( trackInfo["artist"] != m_artist->name() && artist != m_artist->name() )
        {
            qDebug() << "Returned info was for:" << trackInfo["artist"] << "- was looking for:" << m_artist->name();
            return;
        }
    }

    QVariantMap returnedData = output.value< QVariantMap >();
    switch ( requestData.type )
    {
        case InfoSystem::InfoArtistBiography:
        {
            QVariantMap bmap = output.toMap();

            foreach ( const QString& source, bmap.keys() )
            {
                if ( m_longDescription.isEmpty() || source == "last.fm" )
                    m_longDescription = bmap[ source ].toHash()[ "text" ].toString();
            }
            emit longDescriptionChanged( m_longDescription );
            break;
        }

        case InfoSystem::InfoArtistSongs:
        {
            const QStringList tracks = returnedData["tracks"].toStringList();

            QList< query_ptr > queries;
            int i = 0;
            foreach ( const QString& track, tracks )
            {
                queries << Query::get( m_artist->name(), track, QString(), QString(), false );

                if ( ++i == 15 )
                    break;
            }

            Pipeline::instance()->resolve( queries );
            m_topHitsModel->append( queries );
            break;
        }

        case InfoSystem::InfoArtistSimilars:
        {
            const QStringList artists = returnedData["artists"].toStringList();
            QList<artist_ptr> al;
            foreach ( const QString& artist, artists )
            {
                al << Artist::get( artist );
            }
            m_relatedModel->addArtists( al );
            break;
        }

        default:
            return;
    }
}


void
ArtistInfoWidget::onArtistImageUpdated()
{
    if ( m_artist->cover( QSize( 0, 0 ) ).isNull() )
        return;

    m_pixmap = m_artist->cover( QSize( 0, 0 ) );
    emit pixmapChanged( m_pixmap );
}


void
ArtistInfoWidget::changeEvent( QEvent* e )
{
    QWidget::changeEvent( e );
    switch ( e->type() )
    {
        case QEvent::LanguageChange:
            ui->retranslateUi( this );
            break;

        default:
            break;
    }
}
