/*
 *
 *  Copyright (c) 2021
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utility.h"

#include "settings.h"
#include "context.hpp"
#include "downloadmanager.hpp"
#include "tableWidget.h"
#include "tabmanager.h"
#include "version.h"

#include <QEventLoop>
#include <QDesktopServices>
#include <QClipboard>
#include <QMimeData>
#include <QFileDialog>
#include <QSysInfo>

#include <ctime>
#include <cstring>

const char * utility::selectedAction::CLEAROPTIONS = "Clear Options" ;
const char * utility::selectedAction::CLEARSCREEN  = "Clear Screen" ;
const char * utility::selectedAction::OPENFOLDER   = "Open Download Folder" ;

#if defined(__OS2__) || defined(OS2) || defined(_OS2)

bool utility::platformisOS2()
{
	return true ;
}

bool utility::platformIsLinux()
{
	return false ;
}

bool utility::platformIsOSX()
{
	return false ;
}

bool utility::platformIsWindows()
{
	return false ;
}

#endif

#ifdef Q_OS_LINUX

bool utility::platformisOS2()
{
	return false ;
}

bool utility::platformIsLinux()
{
	return true ;
}

bool utility::platformIsOSX()
{
	return false ;
}

bool utility::platformIsWindows()
{
	return false ;
}

#endif

#ifdef Q_OS_MACOS

bool utility::platformisOS2()
{
	return false ;
}

bool utility::platformIsOSX()
{
	return true ;
}

bool utility::platformIsLinux()
{
	return false ;
}

bool utility::platformIsWindows()
{
	return false ;
}

#endif

#ifdef Q_OS_WIN

bool utility::platformIsWindows()
{
	return true ;
}

bool utility::platformIsLinux()
{
	return false ;
}

bool utility::platformIsOSX()
{
	return false ;
}

bool utility::platformisOS2()
{
	return false ;
}

#include <libloaderapi.h>
#include <array>

QString utility::windowsApplicationDirPath()
{
	std::array< char,4096 > buffer ;

	GetModuleFileNameA( nullptr,buffer.data(),static_cast< DWORD >( buffer.size() ) ) ;

	auto m = QDir::fromNativeSeparators( buffer.data() ) ;
	auto s = m.lastIndexOf( '/' ) ;

	if( s != -1 ){

		m.truncate( s ) ;
	}

	return m ;
}

#else

QString utility::windowsApplicationDirPath()
{
	return {} ;
}

#endif

utility::debug& utility::debug::operator<<( const QString& e )
{
	return _print( e.toStdString().c_str() ) ;
}

utility::debug& utility::debug::operator<<( const QList<QByteArray>& e )
{
	if( e.isEmpty() ){

		return _print( "()" ) ;
	}else{
		QString m = "(\"" + e.at( 0 ) + "\"" ;

		for( int s = 1 ; s < e.size() ; s++ ){

			m += ", \"" + e.at( s ) + "\"" ;
		}

		m += ")";

		return _print( m.toStdString().c_str() ) ;
	}
}

utility::debug& utility::debug::operator<<( const QStringList& e )
{
	if( e.isEmpty() ){

		return _print( "()" ) ;
	}else{
		QString m = "(\"" + e.at( 0 ) + "\"" ;

		for( int s = 1 ; s < e.size() ; s++ ){

			m += ", \"" + e.at( s ) + "\"" ;
		}

		m += ")";

		return _print( m.toStdString().c_str() ) ;
	}
}

utility::debug& utility::debug::operator<<( const QByteArray& e )
{
	return _print( e.data() ) ;
}

static void _kill_children_recursively( const QString& id )
{
	auto path = QString( "/proc/%1/task/" ).arg( id ) ;

	auto filter = QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot ;

	for( const auto& it : QDir( path ).entryList( filter ) ){

		QFile file( path + it + "/children" ) ;

		if( file.open( QIODevice::ReadOnly ) ){

			auto pids = util::split( file.readAll(),' ',true ) ;

			for( const auto& it : pids ){

				_kill_children_recursively( it ) ;

				QProcess exe ;
				exe.start( "kill",{ "-s","SIGTERM",it } ) ;
				exe.waitForFinished( -1 ) ;
			}
		}
	}
}

bool utility::Terminator::terminate( QProcess& exe )
{
	if( utility::platformIsWindows() ){

		if( exe.state() == QProcess::ProcessState::Running ){

			QStringList args{ "/F","/T","/PID",QString::number( exe.processId() ) } ;

			QProcess::startDetached( "taskkill",args ) ;
		}

	}else if( utility::platformIsLinux() ){

		utils::qthread::run( [ &exe ](){

			_kill_children_recursively( QString::number( exe.processId() ) ) ;

			exe.terminate() ;
		} ) ;
	}else{
		exe.terminate() ;
	}

	return true ;
}

bool utility::platformIsNOTWindows()
{
	return !utility::platformIsWindows() ;
}

QMenu * utility::setUpMenu( const Context& ctx,
			    const QStringList&,
			    bool addClear,
			    bool addOpenFolder,
			    bool combineText,
			    QWidget * parent )
{
	auto menu = new QMenu( parent ) ;
	auto& tr = ctx.Translator() ;

	auto& configure = ctx.TabManager().Configure() ;

	struct webEntries
	{
		struct entries
		{
			entries( const QString& u,const QString& b ) : uiName( u ),bkName( b )
			{
			}
			QString uiName ;
			QString bkName ;
		} ;

		webEntries( const QString& w,const QString& uiName,const QString& bkName ) : website( w )
		{
			values.emplace_back( uiName,bkName ) ;
		}

		QString website ;
		std::vector< webEntries::entries > values ;
	} ;

	class entries
	{
	public:
		void add( const QString& website,const QString& uiName,const QString& bkName )
		{
			for( auto& it : m_entries ){

				if( it.website == website ){

					it.values.emplace_back( uiName,bkName ) ;

					return ;
				}
			}

			m_entries.emplace_back( website,uiName,bkName ) ;
		}
		void sort()
		{
		}
		void add( QMenu * menu,translator& tr )
		{
			this->sort() ;

			for( const auto& it : m_entries ){

				if( it.website.isEmpty() ){

					translator::entry ss( QObject::tr( "Preset Options" ),"","" ) ;
					tr.addAction( menu,std::move( ss ) )->setEnabled( false ) ;
				}else{
					auto m = QObject::tr( "%1 Preset Options" ).arg( it.website ) ;

					translator::entry ss( m,"","" ) ;
					tr.addAction( menu,std::move( ss ),false )->setEnabled( false ) ;
				}

				for( const auto& xt : it.values ){

					menu->addAction( xt.uiName )->setObjectName( xt.bkName ) ;
				}

				menu->addSeparator() ;
			}
		}
	private:
		std::vector< webEntries > m_entries ;
	} ;

	entries mm ;

	configure.presetOptionsForEach( [ & ]( const configure::presetEntry& e ){

		if( combineText ){

			mm.add( e.website,e.uiNameTranslated,e.options + "\n" + e.uiNameTranslated ) ;
		}else{
			mm.add( e.website,e.uiNameTranslated,e.options ) ;
		}
	} ) ;

	mm.add( menu,tr ) ;

	if( addClear ){

		menu->addSeparator() ;

		translator::entry sx( QObject::tr( "Clear" ),
						   utility::selectedAction::CLEARSCREEN,
						   utility::selectedAction::CLEARSCREEN ) ;

		tr.addAction( menu,std::move( sx ) ) ;
	}

	if( addOpenFolder ){

		menu->addSeparator() ;

		translator::entry mm( QObject::tr( "Open Download Folder" ),
						   utility::selectedAction::OPENFOLDER,
						   utility::selectedAction::OPENFOLDER ) ;

		tr.addAction( menu,std::move( mm ) ) ;
	}

	return menu ;
}

bool utility::hasDigitsOnly( const QString& e )
{
	for( const auto& it : e ){

		if( !( it >= '0' && it <= '9'  ) ){

			return false ;
		}
	}

	return true ;
}

QString utility::homePath()
{
	if( utility::platformIsWindows() ){

		return QDir::homePath() + "/Desktop" ;
	}else{
		return QDir::homePath() ;
	}
}

void utility::waitForOneSecond()
{
	utility::wait( 1000 ) ;
}

void utility::wait( int time )
{
	QEventLoop e ;

	util::Timer( time,[ & ](){ e.exit() ; } ) ;

	e.exec() ;
}

void utility::openDownloadFolderPath( const QString& url )
{
	if( utility::platformIsWindows() ){

		QProcess::startDetached( "explorer.exe",{ QDir::toNativeSeparators( url ) } ) ;
	}else{
		QDesktopServices::openUrl( url ) ;
	}
}

QStringList utility::updateOptions( const updateOptionsStruct& s )
{
	const tableWidget::entry& ent  = s.tableEntry ;
	const engines::engine& engine  = s.engine ;
	const engines::enginePaths& ep = s.ctx.Engines().engineDirPaths() ;
	settings& settings             = s.stts ;
	const utility::args& args      = s.args ;
	const QStringList& urls        = s.urls ;
	bool forceDownload             = s.forceDownload ;
	const QString& downloadPath    = settings.downloadFolder() ;

	const utility::uiIndex& uiIndex       = s.uiIndex;
	const utility::downLoadOptions& dopts = s.dopts ;

	auto opts = [ & ](){

		if( dopts.hasExtraOptions ){

			return QStringList() ;
		}else{
			auto& t = s.ctx.TabManager().Configure() ;

			auto m = t.engineDefaultDownloadOptions( engine.name() ) ;

			if( m.isEmpty() ){

				return engine.defaultDownLoadCmdOptions() ;
			}else{
				return util::splitPreserveQuotes( m ) ;
			}
		}
	}() ;

	for( const auto& it : args.otherOptions() ){

		opts.append( it ) ;
	}

	auto url = urls ;

	engine.updateDownLoadCmdOptions( { args.uiDownloadOptions(),
					   args.otherOptions(),
					   uiIndex,
					   args.credentials(),
					   ent.playlist,
					   ent.playlist_count,
					   ent.playlist_id,
					   ent.playlist_title,
					   ent.playlist_uploader,
					   ent.playlist_uploader_id,
					   ent.n_entries,
					   url,
					   opts } ) ;

	const auto& ca = engine.cookieArgument() ;
	const auto& cv = settings.cookieFilePath( engine.name() ) ;

	if( !ca.isEmpty() && !cv.isEmpty() ){

		opts.append( ca ) ;
		opts.append( cv ) ;
	}

	for( auto& it : opts ){

		it.replace( utility::stringConstants::mediaDownloaderDataPath(),ep.dataPath() ) ;
		it.replace( utility::stringConstants::mediaDownloaderDefaultDownloadPath(),downloadPath ) ;
		it.replace( utility::stringConstants::mediaDownloaderCWD(),QDir::currentPath() ) ;
	}

	if( forceDownload ){

		utility::arguments( opts ).removeOptionWithArgument( "--download-archive" ) ;
	}

	engine.setTextEncondig( opts ) ;

	const auto& p = s.ctx.Engines().proxyServer() ;

	if( !p.isEmpty() ){

		opts.append( "--proxy" ) ;

		opts.append( p ) ;
	}

	opts.append( url ) ;

	return opts ;
}

void utility::initDone()
{
}

int utility::sequentialID()
{
	static int id = 0 ;

	--id ;

	return id ;
}

int utility::concurrentID()
{
	static int id = -1 ;

	++id ;

	return id ;
}

QString utility::failedToFindExecutableString( const QString& cmd )
{
	return QObject::tr( "Failed to find executable \"%1\"" ).arg( cmd ) ;
}

QString utility::clipboardText()
{
	auto m = QApplication::clipboard() ;

	if( m ){

		auto e = m->mimeData() ;

		if( e->hasText() ){

			return e->text() ;
		}
	}

	return {} ;
}

QString utility::downloadFolder( const Context& ctx )
{
	return ctx.Settings().downloadFolder() ;
}

static QJsonArray _saveDownloadList( tableWidget& tableWidget,bool noFinishedSuccess )
{
	QJsonArray arr ;

	auto _add = [ & ]( const tableWidget::entry& e ){

		if( e.url.isEmpty() ){

			return ;
		}

		auto obj = e.uiJson ;

		auto title = obj.value( "title" ).toString() ;

		if( !title.isEmpty() ){

			auto url = obj.value( "url" ).toString() ;

			if( title == url ){

				obj.insert( "title","" ) ;
			}
		}

		obj.insert( "runningState",e.runningState ) ;

		obj.insert( "downloadOptions",e.downloadingOptions ) ;

		obj.insert( "engineName",e.engineName ) ;

		obj.insert( "downloadExtraOptions",e.extraDownloadingOptions ) ;

		arr.append( obj ) ;
	} ;

	if( noFinishedSuccess ){

		tableWidget.forEach( [ & ]( const tableWidget::entry& e ){

			using gg = downloadManager::finishedStatus ;

			if( !gg::finishedWithSuccess( e.runningState ) ){

				_add( e ) ;
			}
		} ) ;
	}else{
		tableWidget.forEach( [ & ]( const tableWidget::entry& e ){

			_add( e ) ;
		} ) ;
	}

	return arr ;
}

void utility::saveDownloadList( const Context& ctx,tableWidget& tableWidget,bool pld )
{
	if( ctx.Settings().autoSavePlaylistOnExit() ){

		if( pld ){

			if( tableWidget.rowCount() == 1 ){

				return ;
			}
		}else{
			if( tableWidget.rowCount() == 0 ){

				return ;
			}
		}

		auto arr = _saveDownloadList( tableWidget,true ) ;

		auto e = ctx.Engines().engineDirPaths().dataPath( "autoSavedList.json" ) ;

		if( QFile::exists( e ) ){

			auto m = engines::file( e,ctx.logger() ).readAll() ;

			QFile::remove( e ) ;

			const auto rr = QJsonDocument::fromJson( m ).array() ;

			for( const auto& it : rr ){

				arr.append( it ) ;
			}
		}

		auto m = QJsonDocument( arr ).toJson( QJsonDocument::Indented ) ;

		engines::file( e,ctx.logger() ).write( m ) ;
	}
}

void utility::saveDownloadList( const Context& ctx,QMenu& m,tableWidget& tableWidget,bool pld )
{
	m.setToolTipsVisible( true ) ;

	auto dialogTxt = QObject::tr( "Save List To File" ) ;
	auto toolTip = QObject::tr( "Filename with \".txt\" Extension Will Save Urls Only" ) ;

	auto ac = m.addAction( dialogTxt ) ;

	ac->setToolTip( toolTip ) ;

	QObject::connect( ac,&QAction::triggered,[ &ctx,&tableWidget,pld,toolTip ](){

		QString filePath ;

		if( pld && tableWidget.rowCount() > 1 ){

			auto uploader = tableWidget.entryAt( 1 ).uiJson.value( "uploader" ).toString() ;

			if( !uploader.isEmpty() ){

				filePath = utility::homePath() + "/MediaDowloaderList-" + uploader + ".json" ;
			}else{
				filePath = utility::homePath() + "/MediaDowloaderList.json" ;
			}
		}else{
			filePath = utility::homePath() + "/MediaDowloaderList.json" ;
		}

		auto s = QFileDialog::getSaveFileName( &ctx.mainWidget(),
						       toolTip,
						       filePath ) ;
		if( !s.isEmpty() ){

			const auto e = _saveDownloadList( tableWidget,false ) ;

			if( s.endsWith( ".json" ) ){

				auto m = QJsonDocument( e ).toJson( QJsonDocument::Indented ) ;

				engines::file( s,ctx.logger() ).write( m ) ;
			}else{
				QByteArray m ;

				for( const auto& it : e ){

					m.append( it.toObject().value( "url" ).toString().toUtf8() + "\n" ) ;
				}

				engines::file( s,ctx.logger() ).write( m ) ;
			}
		}
	} ) ;
}

bool utility::isRelativePath( const QString& e )
{
	return QDir::isRelativePath( e ) ;
}

static QString _stringValue( QJsonObject& obj,const char * key )
{
	return obj.value( key ).toString().replace( "\"NA\"","NA" ) ;
}

static QString _intValue( QJsonObject& obj,const char * key )
{
	return QString::number( obj.value( key ).toInt() ) ;
}

utility::MediaEntry::MediaEntry( const QByteArray& data ) : m_json( data )
{
	if( m_json ){

		auto object = m_json.doc().object() ;

		m_formats              = object.value( "formats" ).toArray() ;

		m_title                = _stringValue( object,"title" ) ;
		m_url                  = _stringValue( object,"webpage_url" ) ;
		m_uploadDate           = _stringValue( object,"upload_date" ) ;
		m_id                   = _stringValue( object,"id" ) ;
		m_thumbnailUrl         = _stringValue( object,"thumbnail" ) ;
		m_uploader             = _stringValue( object,"uploader" ) ;
		m_playlist             = _stringValue( object,"playlist" ) ;
		m_playlist_id          = _stringValue( object,"playlist_id" ) ;
		m_playlist_title       = _stringValue( object,"playlist_title" ) ;
		m_playlist_uploader    = _stringValue( object,"playlist_uploader" ) ;
		m_playlist_uploader_id = _stringValue( object,"playlist_uploader_id" ) ;

		m_n_entries            = _intValue( object,"n_entries" ) ;
		m_playlist_count       = _intValue( object,"playlist_count" ) ;

		if( m_uploadDate.size() == 8 ){

			auto year  = m_uploadDate.mid( 0,4 ).toInt() ;
			auto month = m_uploadDate.mid( 4,2 ).toInt() ;
			auto day   = m_uploadDate.mid( 6,2 ).toInt() ;

			QDate d ;

			if( d.setDate( year,month,day ) ){

				m_uploadDate = d.toString() ;
			}
		}

		if( !m_uploadDate.isEmpty() ){

			m_uploadDate = utility::stringConstants::uploadDate() + " " + m_uploadDate ;
		}

		auto duration = object.value( "duration" ) ;

		if( duration.isDouble() ){

			m_intDuration = static_cast< int >( duration.toDouble() ) ;
		}else{
			m_intDuration = duration.toInt() ;
		}

		if( m_intDuration != 0 ){

			auto s = engines::engine::functions::timer::duration( m_intDuration * 1000 ) ;
			m_duration = utility::stringConstants::duration() + " " + s ;
		}
	}
}

QString utility::MediaEntry::uiText() const
{
	auto title = [ & ](){

		if( m_title.isEmpty() || m_title == "\n" ){

			return m_url ;
		}else{
			return m_title ;
		}
	}() ;

	if( m_duration.isEmpty() ){

		if( m_uploadDate.isEmpty() ){

			return title ;
		}else{
			return m_uploadDate + "\n" + title ;
		}
	}else{
		if( m_uploadDate.isEmpty() ){

			return m_duration + "\n" + title ;
		}else{
			return m_duration + ", " + m_uploadDate + "\n" + title ;
		}
	}
}

QJsonObject utility::MediaEntry::uiJson() const
{
	QJsonObject obj ;

	auto u = QString( m_uploadDate ).replace( utility::stringConstants::uploadDate() + " ","" ) ;
	auto d = QString( m_duration ).replace( utility::stringConstants::duration() + " ","" ) ;

	obj.insert( "title",m_title ) ;
	obj.insert( "url",m_url ) ;
	obj.insert( "duration",d ) ;
	obj.insert( "upload_date",u ) ;
	obj.insert( "uploader",m_uploader ) ;

	return obj ;
}

const engines::engine& utility::resolveEngine( const tableWidget& table,
					       const engines::engine& engine,
					       const engines& engines,
					       int row )
{
	const auto& engineName = table.engineName( row ) ;

	if( engineName.isEmpty() ){

		return engine ;
	}else{
		const auto& ee = engines.getEngineByName( engineName ) ;

		if( ee.has_value() ){

			return ee.value() ;
		}else{
			return engine ;
		}
	}
}

bool utility::platformIs32Bit()
{
#if QT_VERSION >= QT_VERSION_CHECK( 5,4,0 )
	return QSysInfo::currentCpuArchitecture() != "x86_64" ;
#else
	//?????
	return false ;
#endif
}

void utility::addJsonCmd::add( const utility::addJsonCmd::entry& e )
{
	m_obj.insert( e.platform,[ & ]{

		QJsonObject s ;

		for( const auto& it : e.platformData ){

			s.insert( it.archName,[ & ](){

				QJsonObject a ;

				a.insert( "Name",it.exeName ) ;

				a.insert( "Args",[ & ](){

					QJsonArray arr ;

					for( const auto& xt : it.exeArgs ){

						arr.append( xt ) ;
					}

					return arr ;
				}() ) ;

				return a ;
			}() ) ;
		}

		return s ;

	}() ) ;
}

QString utility::fromSecsSinceEpoch( qint64 s )
{
	std::time_t epoch = static_cast< std::time_t >( s ) ;
	return QString( std::asctime( std::gmtime( &epoch ) ) ).trimmed() ;
}

utility::downLoadOptions utility::setDownloadOptions( const engines::engine& engine,
						      tableWidget& table,
						      int row,
						      const QString& downloadOpts )
{
	utility::downLoadOptions opts ;

	auto m = table.subTitle( row ) ;

	if( !m.isEmpty() ){

		auto s = util::split( m,' ',true ) ;

		if( s.size() > 1 ){

			auto e = engine.defaultSubtitleDownloadOptions().join( " " ) ;

			if( s.at( 0 ) == "ac:" ){

				m = " " + e + " --write-auto-subs --sub-langs " + s.at( 1 ) ;
			}else{
				m = " " + e + " --sub-langs " + s.at( 1 ) ;
			}
		}
	}

	auto z = table.timeInterval( row ) ;

	if( !z.isEmpty() ){

		m += " --download-sections *" + z ;
	}

	z = table.chapters( row ) ;

	if( !z.isEmpty() ){

		for( const auto& it : util::split( z,',',true ) ){

			m += " --download-sections \"" + it + "\"" ;
		}
	}

	if( table.splitByChapters( row ) ){

		m += " --split-chapters" ;
	}

	auto zz = table.extraDownloadOptions( row ) ;

	if( !zz.isEmpty() ){

		opts.hasExtraOptions = true ;

		m += " " + zz + " " ;
	}

	opts.downloadOptions = [ & ](){

		auto u = table.downloadingOptions( row ) ;

		if( u.isEmpty() ){

			if( downloadOpts.isEmpty() ){

				return m ;
			}else{
				return downloadOpts + m ;
			}

		}else if( downloadOpts.isEmpty() ){

			return u + m ;

		}else if( u == downloadOpts ){

			return u + m ;
		}else{
			return u + m + " " + downloadOpts ;
		}
	}() ;

	return opts ;
}

void utility::setDefaultEngine( const Context& ctx,const QString& name )
{
	if( !name.isEmpty() ){

		ctx.Engines().setDefaultEngine( name ) ;

		ctx.TabManager().setDefaultEngines() ;
	}
}

bool utility::onlyWantedVersionInfo( const utility::cliArguments& args )
{
	if( args.contains( "--version" ) ){

		std::cout << util::split( VERSION,'\n' ).at( 0 ).constData() << std::endl ;

		return true ;
	}else{
		return false ;
	}
}

bool utility::startedUpdatedVersion( settings& s,const utility::cliArguments& cargs )
{
	if( utility::platformIsNOTWindows() ){

		return false ;
	}

	auto cpath = s.configPaths() ;

	auto m = cpath + "/update_new" ;
	auto mm = cpath + "/update" ;

	if( QFile::exists( m ) ){

		QDir dir( mm ) ;

		dir.removeRecursively() ;
		dir.rename( m,mm ) ;
	}

	QString exePath = mm + "/media-downloader.exe" ;

	if( QFile::exists( exePath ) && !cargs.runningUpdated() ){

		auto exeDirPath = utility::windowsApplicationDirPath() ;

		auto args = cargs.arguments( cpath,exeDirPath,s.portableVersion() ) ;

		auto env = QProcessEnvironment::systemEnvironment() ;

		env.insert( "PATH",exeDirPath + ";" + env.value( "PATH" ) ) ;

		env.insert( "QT_PLUGIN_PATH",exeDirPath ) ;

		QProcess exe ;

		exe.setProgram( exePath ) ;
		exe.setArguments( args ) ;
		exe.setProcessEnvironment( env ) ;

#if QT_VERSION >= QT_VERSION_CHECK( 5,10,0 )
		return exe.startDetached() ;
#else
		exe.start() ;
		exe.waitForFinished( -1 ) ;
		return true ;
#endif
	}else{		
		return false ;
	}
}

bool utility::platformIsLikeWindows()
{
	return utility::platformIsWindows() || utility::platformisOS2() ;
}

static QString _instanceVersion ;
static QString _aboutInstanceVersion ;

QString utility::aboutVersionInfo()
{
	if( _aboutInstanceVersion.isEmpty() ){

		return utility::runningVersionOfMediaDownloader() ;
	}else{
		return _aboutInstanceVersion ;
	}
}

QString utility::runningVersionOfMediaDownloader()
{
	if( _instanceVersion.isEmpty() ){

		return VERSION ;
	}else{
		return _instanceVersion ;
	}
}

void utility::setRunningVersionOfMediaDownloader( const QString& e )
{
	_instanceVersion = e ;
}

void utility::setHelpVersionOfMediaDownloader( const QString& e )
{
	_aboutInstanceVersion = e ;
}

static QStringList _parseOptions( const QString& e,const engines::engine& engine )
{
	auto m = util::splitPreserveQuotes( e ) ;

	if( m.isEmpty() ){

		return {} ;
	}

	const auto& q = engine.optionsArgument() ;

	if( q.isEmpty() ){

		return m ;
	}

	if( !m[ 0 ].startsWith( '-' ) ){

		if( m[ 0 ].compare( "default",Qt::CaseInsensitive ) ){

			m.insert( 0,q ) ;
		}
	}

	QStringList opts ;

	for( int i = 0 ; i < m.size() ; i++ ){

		const auto& s = m[ i ] ;

		if( s == q && i + 1 < m.size() ){

			const auto& ss = m[ i + 1 ] ;

			if( !ss.startsWith( '-' ) ){

				if( ss.compare( "default",Qt::CaseInsensitive ) ){

					opts.append( q ) ;

					opts.append( ss ) ;
				}
			}

			i++ ;
		}else{
			opts.append( s ) ;
		}
	}

	return opts ;
}

utility::args::args( const QString& uiOptions,const QString& otherOptions,const engines::engine& engine )
{
	m_uiDownloadOptions = _parseOptions( uiOptions,engine ) ;
	m_otherOptions      = _parseOptions( otherOptions,engine ) ;
	m_credentials       = engine.setCredentials( m_uiDownloadOptions,m_otherOptions ) ;
}

QStringList utility::args::options() const
{
	return m_otherOptions + m_uiDownloadOptions ;
}

QString utility::uiIndex::toString( bool pad,const QStringList& e ) const
{
	auto start = [ & ](){

		for( int m = 0 ; m < e.size() ; m++ ){

			if( e[ m ] == "--autonumber-start" ){

				if( m + 1 < e.size() ){

					return e[ m + 1 ].toInt() - 1 ;
				}
			}
		}

		return 0 ;
	}() ;

	if( pad ){

		return this->toString( start + m_index ) ;
	}else{
		return QString::number( start + m_index ) ;
	}
}

QString utility::uiIndex::toString( int index ) const
{
	auto s = QString::number( index ) ;

	auto m = QString::number( m_total ) ;

	while( s.size() < m.size() ){

		s.insert( 0,'0' ) ;
	}

	return s ;
}

void utility::setPermissions( QFile& qfile )
{
	if( !QFileInfo( qfile ).isExecutable() ){

		qfile.setPermissions( qfile.permissions() | QFileDevice::ExeOwner ) ;
	}
}

void utility::setPermissions( const QString& e )
{
	QFile s( e ) ;

	utility::setPermissions( s ) ;
}

void utility::networkReply::invoke( QObject * obj,const char * member )
{
	QMetaObject::invokeMethod( obj,
				   member,
				   Qt::QueuedConnection,
				   Q_ARG( utility::networkReply,*this ) ) ;
}

void utility::networkReply::getData( const Context& ctx,const utils::network::reply& reply )
{
	if( reply.success() ){

		m_data = reply.data() ;

	}else if( reply.timeOut() ){

		QString m = "Network Error: Network Request Timed Out" ;

		ctx.logger().add( m,utility::sequentialID() ) ;
	}else{
		ctx.logger().add( "Network Error: " + reply.errorString(),utility::sequentialID() ) ;
	}
}

utility::cliArguments::cliArguments( int argc,char ** argv )
{
	for( int i = 0 ; i < argc ; i++ ){

		m_args.append( argv[ i ] ) ;
	}

	if( this->runningUpdated() ){

		utility::setRunningVersionOfMediaDownloader( this->value( "--fake-updated-version" ) ) ;
	}else{
		utility::setRunningVersionOfMediaDownloader( this->value( "--fake-version" ) ) ;
	}
}

bool utility::cliArguments::contains( const char * m ) const
{
	return m_args.contains( m ) ;
}

bool utility::cliArguments::runningUpdated() const
{
	return this->contains( "--running-updated" ) ;
}

bool utility::cliArguments::portable() const
{
	return this->contains( "--portable" ) ;
}

QString utility::cliArguments::dataPath() const
{
	return this->value( "--dataPath" ) ;
}

QString utility::cliArguments::originalPath() const
{
	return this->value( "--exe-org-path" ) ;
}

QString utility::cliArguments::originalVersion() const
{
	return this->value( "--running-version" ) ;
}

QString utility::cliArguments::value( const char * m ) const
{
	for( auto it = m_args.begin() ; it != m_args.end() ; it++ ){

		if( *it == m ){

			auto xt = it + 1 ;

			if( xt != m_args.end() ){

				return *xt ;
			}
		}
	}

	return {} ;
}

QStringList utility::cliArguments::arguments( const QString& cpath,
					      const QString& exeDirPath,
					      bool portableVersion ) const
{
	auto args = m_args ;

	args.append( "--running-updated" ) ;

	args.append( "--dataPath" ) ;

	args.append( cpath ) ;

	args.append( "--running-version" ) ;

	args.append( utility::runningVersionOfMediaDownloader() ) ;

	if( portableVersion ){

		args.append( "--portable" ) ;
	}

	args.append( "--exe-org-path" ) ;
	args.append( exeDirPath ) ;

	return args ;
}

const QStringList& utility::cliArguments::arguments() const
{
	return m_args ;
}

#ifdef Q_OS_WIN

extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;

void utility::ntfsEnablePermissionChecking( bool e )
{
	if( e ){

		qt_ntfs_permission_lookup++ ;
	}else{
		qt_ntfs_permission_lookup-- ;
	}
}

#else

void utility::ntfsEnablePermissionChecking( bool )
{
}

#endif

bool utility::pathIsFolderAndExists( const QString& e )
{
	QFileInfo m( e ) ;

	return m.exists() && m.isDir() ;
}

QByteArray utility::barLine()
{
	return "*************************************************************" ;
}

utility::printOutPut::printOutPut( QObject * obj,const utility::cliArguments& args ) :
	m_qObject( obj )
{
	if( args.contains( "--qDebug" ) || args.contains( "--qdebug" ) ){

		m_status = utility::printOutPut::status::qdebug ;

	}else if( args.contains( "--debug" ) ){

		m_status = utility::printOutPut::status::debug ;
	}else{
		auto m = args.value( "--log-to-file" ) ;

		if( !m.isEmpty() ){

			m_outPutFile.setFileName( m ) ;

			m_outPutFile.open( QIODevice::WriteOnly | QIODevice::Append ) ;
		}
	}
}

void utility::printOutPut::operator()( const QByteArray& e )
{
	QMetaObject::invokeMethod( m_qObject,[ this,e ](){

		if( m_outPutFile.isOpen() ){

			m_outPutFile.write( e ) ;
		}

		if( m_status == utility::printOutPut::status::qdebug ){

			qDebug() << e ;
			qDebug() << "--------------------------------" ;

		}else if( m_status == utility::printOutPut::status::debug ){

			std::cout << e.constData() << std::endl ;
			std::cout << "--------------------------------" << std::endl ;
		}

	},Qt::QueuedConnection ) ;
}

bool utility::printOutPut::isEmpty() const
{
	return m_status == utility::printOutPut::status::notSet ;
}

bool utility::printOutPut::debugging() const
{
	return m_status != utility::printOutPut::status::notSet ;
}

void utility::failedToParseJsonData( Logger& logger,const QJsonParseError& error )
{
	auto id = utility::sequentialID() ;

	logger.add( "Failed To Parse Json Data:" + error.errorString(),id ) ;
}

void utility::hideUnhideEntries( QMenu& m,tableWidget& table,int row,bool showHide )
{
	if( showHide ){

		auto ac = m.addAction( QObject::tr( "Hide Row" ) ) ;

		QObject::connect( ac,&QAction::triggered,[ &table,row ](){

			table.hideRow( row ) ;
		} ) ;
	}

	if( table.containsHiddenRows() ){

		auto ac = m.addAction( QObject::tr( "Unhide All Hidden Rows" ) ) ;

		QObject::connect( ac,&QAction::triggered,[ &table ](){

			auto& t = table.get() ;

			for( int row = 0 ; row < table.rowCount() ; row++ ){

				if( t.isRowHidden( row ) ){

					t.showRow( row ) ;
				}
			}
		} ) ;
	}
}

static QStringList _listOptionsFromDownloadOptions( const QString& e )
{
	QStringList m ;

	auto ee = util::splitPreserveQuotes( e ) ;

	for( auto it = ee.begin() ; it != ee.end() ; it++ ){

		const auto& s = *it ;

		if( s == "\"--proxy\"" || s == "--proxy" ){

			auto xt = it + 1 ;

			if( xt != ee.end() ){

				m.append( "--proxy" ) ;
				m.append( *xt ) ;
			}

			break ;
		}
	}

	return m ;
}

static QNetworkProxy& _defaultProxy()
{
	static QNetworkProxy m( QNetworkProxy::applicationProxy() ) ;

	return m ;
}

static void _set_proxy( QString url )
{
	QNetworkProxy proxy ;

	if( url.startsWith( "socks5://" ) ){

		proxy.setType( QNetworkProxy::Socks5Proxy ) ;
	}else{
		proxy.setType( QNetworkProxy::HttpProxy ) ;
	}

	auto e = url.indexOf( "//" ) ;

	if( e != -1 ){

		url = url.mid( e + 2 ) ;
	}

	e = url.indexOf( '@' ) ;

	if( e != -1 ){

		auto credentials = url.mid( 0,e ) ;

		auto ee = credentials.indexOf( ':' ) ;

		if( ee != -1 ){

			proxy.setUser( credentials.mid( 0,ee ) ) ;
			proxy.setPassword( credentials.mid( ee + 1 ) ) ;
		}

		url = url.mid( e + 1 ) ;
	}

	e = url.indexOf( ':' ) ;

	if( e != -1 ){

		proxy.setPort( url.mid( e + 1 ).toInt() ) ;

		url = url.mid( 0,e ) ;
	}

	proxy.setHostName( url ) ;

	QNetworkProxy::setApplicationProxy( proxy ) ;
}

static void _setNetworkProxy( const QStringList& ee )
{
	for( int i = ee.size() - 2 ; i > -1 ; i-- ){

		if( ee[ i ] == "--proxy" ){

			return _set_proxy( ee[ i + 1 ] ) ;
		}
	}

	QNetworkProxy::setApplicationProxy( _defaultProxy() ) ;
}

void utility::addToListOptionsFromsDownload( QStringList& args,
					     const QString& downLoadOptions,
					     const Context& ctx,
					     const QString& engine )
{
	auto m = ctx.TabManager().Configure().engineDefaultDownloadOptions( engine ) ;

	auto ee = _listOptionsFromDownloadOptions( m ) ;

	auto eee = _listOptionsFromDownloadOptions( downLoadOptions ) ;

	const auto& mm = ctx.Engines().proxyServer() ;

	if( !ee.isEmpty() ){

		args = args + ee ;
	}

	if( mm.isEmpty() ){

		_setNetworkProxy( ee + eee ) ;
	}else{
		args.append( "--proxy" ) ;

		args.append( mm ) ;

		_set_proxy( mm ) ;
	}
}
