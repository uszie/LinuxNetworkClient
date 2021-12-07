/***************************************************************************
                         knetworkclientconfig.cpp  -  description
                            -------------------
   begin                : ma aug 19 12:29:58 EDT 2002
   copyright            : (C) 2002 by Usarin Heininga
   email                : usarin@heininga.nl
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qlayout.h>
#include <qcheckbox.h>
#include <qtabwidget.h>
#include <qlistbox.h>

#include <klocale.h>
#include <kconfig.h>
#include <kcombobox.h>
#include <klistview.h>
#include <kpushbutton.h>
#include <knuminput.h>
#include <klineedit.h>
#include <ksqueezedtextlabel.h>
#include <knuminput.h>
#include <kactionselector.h>

#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>


#include "knetworkclientconfig.h"
#include "../../config.h"
#include "networkclient.h"
#include "nconfig.h"

#include <kdb.h>

#define CHECKBOX 1
#define LINEEDIT 2
#define SPINBOX  3

typedef struct pluginWidget {
	struct list list;
	QWidget *widget;
	QButtonGroup *group;
	unsigned int type;
	const char name[ NAME_MAX + 1 ];
}
pluginWidget;

defineList( pluginWidgetList, struct pluginWidget );
static struct pluginWidgetList pluginWidgetList;

KNetworkClientConfig::KNetworkClientConfig( QWidget *parent, const char *name ) : KCModule( parent, name )
{
	ui = new KNetworkClientConfigUI( this );

	QVBoxLayout *vbox = new QVBoxLayout( this, KDialog::marginHint(), KDialog::spacingHint() );
	vbox->addWidget( ui );

	connect( ui->BUISpinBox, SIGNAL( valueChanged( int ) ), SLOT( configChanged() ) );
	connect( ui->UISpinBox, SIGNAL( valueChanged( int ) ), SLOT( configChanged() ) );
	connect( ui->LLkComboBox, SIGNAL( activated( int ) ), SLOT( configChanged() ) );
	connect( ui->EDMComboBox, SIGNAL( activated( int ) ), SLOT( configChanged() ) );
	connect( ui->AddPushButton, SIGNAL( clicked() ), SLOT( configChanged() ) );
	connect( ui->OptionsPushButton, SIGNAL( clicked() ), SLOT( configChanged() ) );
	connect( ui->AppListView, SIGNAL( executed( QListViewItem * ) ), SLOT( configChanged() ) );
	connect( ui->DULineEdit, SIGNAL( textChanged( const QString& ) ), SLOT( configChanged() ) );
	connect( ui->DPLineEdit, SIGNAL( textChanged( const QString& ) ), SLOT( configChanged() ) );
	connect( ui->PNLineEdit, SIGNAL( textChanged( const QString& ) ), SLOT( configChanged() ) );
	connect( ui->IALineEdit, SIGNAL( textChanged( const QString& ) ), SLOT( configChanged() ) );
	connect( ui->DPWCheckBox, SIGNAL( stateChanged( int ) ), SLOT( configChanged() ) );
	connect( ui->UBCheckBox, SIGNAL( stateChanged( int ) ), SLOT( configChanged() ) );
	connect( ui->PActionSelector, SIGNAL( added( QListBoxItem * ) ), SLOT( configChanged() ) );
	connect( ui->PActionSelector, SIGNAL( removed( QListBoxItem * ) ), SLOT( configChanged() ) );
	load();
};

KNetworkClientConfig::~KNetworkClientConfig() {
}

void KNetworkClientConfig::createPluginLineEdit( QButtonGroup *group, QGridLayout *layout, const char *name, const char *value )
{
	KLineEdit *lineEdit;
	KSqueezedTextLabel *textLabel;
	pluginWidget *pluginWidget;

	textLabel = new KSqueezedTextLabel( group, name );
	textLabel->setText( tr2i18n( name ) );
	textLabel->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)3, (QSizePolicy::SizeType)0, 0, 0, textLabel->sizePolicy().hasHeightForWidth() ) );
	layout->addWidget( textLabel, layout->numRows(), 0 );

	lineEdit = new KLineEdit( value, group, name );
	lineEdit->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)3, (QSizePolicy::SizeType)0, 0, 0, lineEdit->sizePolicy().hasHeightForWidth() ) );
	layout->addWidget( lineEdit, layout->numRows() -1, 1 );
	connect( lineEdit, SIGNAL( textChanged( const QString & ) ), SLOT( configChanged() ) );

	pluginWidget = new struct pluginWidget;
	pluginWidget->widget = lineEdit;
	pluginWidget->group = group;
	pluginWidget->type = LINEEDIT;
	addListEntry( &pluginWidgetList, pluginWidget );
}

void KNetworkClientConfig::createPluginCheckBox( QButtonGroup *group, QGridLayout *layout, const char *name, bool value )
{
	QCheckBox *checkBox;
	pluginWidget *pluginWidget;

	checkBox = new QCheckBox ( name, group, name );
	checkBox->setChecked( value );
	checkBox->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)3, (QSizePolicy::SizeType)0, 0, 0, checkBox->sizePolicy().hasHeightForWidth() ) );
	layout->addWidget( checkBox, layout->numRows(), 0 );
	connect( checkBox, SIGNAL( stateChanged( int ) ), SLOT( configChanged() ) );

	pluginWidget = new struct pluginWidget;
	pluginWidget->widget = checkBox;
	pluginWidget->group = group;
	pluginWidget->type = CHECKBOX;
	addListEntry( &pluginWidgetList, pluginWidget );
}

void KNetworkClientConfig::createPluginSpinBox( QButtonGroup *group, QGridLayout *layout, const char *name, int value )
{
	KIntSpinBox *spinBox;
	KSqueezedTextLabel *textLabel;
	pluginWidget *pluginWidget;

	textLabel = new KSqueezedTextLabel( group, name );
	textLabel->setText( tr2i18n( name ) );
	textLabel->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)3, (QSizePolicy::SizeType)0, 0, 0, textLabel->sizePolicy().hasHeightForWidth() ) );
	layout->addWidget( textLabel, layout->numRows(), 0 );

	spinBox = new KIntSpinBox( INT_MIN, INT_MAX, 1, value, 10, group, name );
	spinBox->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)3, (QSizePolicy::SizeType)0, 0, 0, spinBox->sizePolicy().hasHeightForWidth() ) );
	layout->addWidget( spinBox, layout->numRows() - 1, 1 );
	connect( spinBox, SIGNAL( valueChanged( int ) ), SLOT( configChanged() ) );

	pluginWidget = new struct pluginWidget;
	pluginWidget->widget = spinBox;
	pluginWidget->group = group;
	pluginWidget->type = SPINBOX;
	addListEntry( &pluginWidgetList, pluginWidget );
}
void KNetworkClientConfig::load()
{
	char *ptr;
	char *disabledPlugins;
	char *remain;
	int mode = USER_ROOT;
	long int value;
	int i, cnt;
	size_t size;
	DIR *dir;
	struct key *key;
	struct dirent *dirent;

	initList( &pluginWidgetList, NULL, NULL, NULL, NULL, NULL, NULL, NULL );

	initKeys( "LNC" );
	initKeys( "networkclient" );

	ui->BUISpinBox->setValue( getNumKey( "LNC", "BrowselistUpdateInterval", 600 )/60 );
	ui->UISpinBox->setValue( getNumKey( "LNC", "UmountInterval", 60 ) );
	ui->LLkComboBox->setCurrentItem( getNumKey( "LNC", "LogLevel", LNC_ERROR ) - 1 );
	ui->EDMComboBox->setCurrentItem( getNumKey( "LNC", "EnableDebugMode", 0 ) );
	ui->DPWCheckBox->setChecked( getNumKey( "LNC", "DisablePasswordWriting", 0 ) );
	ui->UBCheckBox->setChecked( getNumKey( "LNC", "UseBroadcast", 0 ) );
	ptr = getCharKey( "LNC", "PingNetworks", "ALL" );
	ui->PNLineEdit->setText( ptr );
	free( ptr );

	ptr = getCharKey( "LNC", "IgnoreAddresses", "" );
	ui->IALineEdit->setText( ptr );
	free( ptr );

	ptr = getCharKey( "LNC", "DefaultUsername", "" );
	ui->DULineEdit->setText( ptr );
	free( ptr );

	ptr = getCharKey( "LNC", "DefaultPassword", "" );
	ui->DPLineEdit->setText( ptr );
	free( ptr );

	disabledPlugins = getCharKey( "LNC", "DisabledPlugins", "" );
	QListBox *enabledList = ui->PActionSelector->availableListBox();
	QListBox *disabledList = ui->PActionSelector->selectedListBox();
	dir = opendir( LNC_PLUGIN_DIR );
	if ( dir ) {
		while ( ( dirent = readdir( dir ) ) ) {
			if ( dirent->d_type == DT_REG ) {
				ptr = strstr( dirent->d_name, ".so" );
				if ( !ptr )
					continue;
				*(ptr+3) = '\0';
				if ( strstr( disabledPlugins, dirent->d_name ) )
					disabledList->insertItem( dirent->d_name );
				else
					enabledList->insertItem( dirent->d_name );
			}
		}

		closedir( dir );
	}

	enabledList->sort( TRUE );
	disabledList->sort( TRUE );
	free( disabledPlugins );

	size = getKeyCount( "networkclient/Applications", USER_ROOT );
	if ( size <= 0 )
		mode = SYSTEM_ROOT;

	rewindKeys( "networkclient", mode );
	while ( ( key = forEveryKey2( "networkclient", mode ) ) ) {
		if ( key->type == LNC_CONFIG_KEY && strcmp( key->parentName, "Applications" ) == 0 )
			addListViewItem( key->name, key->value );
		free( key );
	}

	pluginConfigTab = new QWidget( ui->tabWidget, "pluginConfigTab" );
	ui->tabWidget->insertTab( pluginConfigTab, QString::fromLatin1("") );
	ui->tabWidget->changeTab( pluginConfigTab, tr2i18n( "Plugin Settings" ) );
	pluginConfigLayout = new QGridLayout( pluginConfigTab, 1, 1, 11, 6, "pluginLayout" );
	pluginConfigSpacer = new QSpacerItem( 61, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
	pluginConfigLayout->addItem( pluginConfigSpacer, 10, 0 );

	cnt= 0;
	rewindKeys( "LNC", USER_ROOT );
	while ( ( key = forEveryKey2( "LNC", USER_ROOT ) ) && cnt < 10 ) {
		if ( key->type == LNC_CONFIG_DIR && strcmp( key->name, "plugins" ) != 0 && strcmp( key->parentName, "plugins" ) == 0 ) {
			buttonGroup[ cnt ] = new QButtonGroup( pluginConfigTab, key->name);
			buttonGroup[ cnt ]->setTitle( key->name );
			buttonGroup[ cnt ]->setEnabled( TRUE );
			buttonGroup[ cnt ]->setColumnLayout(0, Qt::Vertical );
			buttonGroup[ cnt ]->layout()->setSpacing( 6 );
			buttonGroup[ cnt ]->layout()->setMargin( 11 );
			buttonLayout[ cnt ] = new QGridLayout( buttonGroup[ cnt ]->layout(), 0, 0 );
			pluginConfigLayout->addWidget( ( QWidget *) buttonGroup[ cnt ], cnt, 0 );
			cnt += 1;
		}

		free( key );
	}

	if ( cnt == 0 ) {
		ui->tabWidget->removePage( pluginConfigTab );
		goto out;
	}

	rewindKeys( "LNC", USER_ROOT );
	while ( ( key = forEveryKey2( "LNC", USER_ROOT ) ) ) {
		for ( i = 0; i < cnt; ++i ) {
			if ( buttonGroup[ i]->title().contains( key->parentName ) ) {
				tmpGroup = buttonGroup[ i ];
				tmpLayout = buttonLayout[ i ];
				if ( key->type == LNC_CONFIG_DIR  )
					continue;
				else if ( strcasecmp( key->value, "True" ) == 0 )
					createPluginCheckBox( tmpGroup,  tmpLayout, key->name, TRUE );
				else if ( strcasecmp( key->value, "False" ) == 0 ) {
					createPluginCheckBox( tmpGroup,  tmpLayout, key->name, FALSE );
				} else {
					value = strtol( key->value, &remain, 10 );
					if ( remain && remain[ 0 ] == '\0' )
						createPluginSpinBox( tmpGroup, tmpLayout, key->name, value );
					else
						createPluginLineEdit( tmpGroup, tmpLayout, key->name, key->value );
				}
			}
		}

		free( key );
	}

out:
	cleanupKeys( "LNC" );
	cleanupKeys( "networkclient" );

	emit changed( false );
}

void KNetworkClientConfig::addListViewItem( const char *application, const char *string )
{
	char *token;
	char *cpy;
	char *ptr;
	char *begin;
	int i;
	QListViewItem *item;

	if ( !application || !string )
		return;

	item = new QListViewItem( ui->AppListView, 0 );
	item->setText( 0, application);

	cpy = strdup( string );
	begin = cpy;
	for ( i = 1; ( token = strtok_r( cpy, ",", &ptr ) ); ++i ) {
		cpy = NULL;
		if ( strcasecmp( token, "Null") == 0 )
			item->setText( i, "" );
		else
			item->setText( i, i18n( token ) );
	}

	free( begin );
}

void KNetworkClientConfig::addListViewItem( QStringList *list )
{
	for ( QStringList::Iterator it = list->begin(); it != list->end(); it++ )
	{
		if ( *it == "END" )
			break;
		QListViewItem *item = new QListViewItem( ui->AppListView, 0 );
		item->setText( 0, *it++ );
		item->setText( 1, *it++ );
		item->setText( 2, i18n( *it++ ) );
		item->setText( 3, i18n( *it++ ) );
		item->setText( 4, i18n( *it++ ) );
		item->setText( 5, i18n( *it ) );
	}
}

void KNetworkClientConfig::addListViewItem( const QString app, const QString args, const QString disabled, const QString fakeSyscall, const QString opendir, const QString stat )
{
	QListViewItem * item = new QListViewItem( ui->AppListView, 0 );
	item->setText( 0, app );
	item->setText( 1, args );
	item->setText( 2, i18n( disabled ) );
	item->setText( 3, i18n( fakeSyscall ) );
	item->setText( 4, i18n( opendir ) );
	item->setText( 5, i18n( stat ) );
}

void KNetworkClientConfig::defaultApps()
{
	addListViewItem( "konqueror", "", "Yes", "Yes", "Yes", "Yes" );
/*	addListViewItem( "kdeinit", "kio_file file", "Yes" );
	addListViewItem( "mc" );
	addListViewItem( "dir" );
	addListViewItem( "bash" );
	addListViewItem( "csh" );
	addListViewItem( "nautilus" );
	addListViewItem( "xmms" );
	addListViewItem( "gmplayer" );
	addListViewItem( "mplayer" );
	addListViewItem( "xine" );*/
}

void KNetworkClientConfig::defaults()
{
	if ( ui->tabWidget->currentPageIndex() == 0 )
	{
		ui->BUISpinBox->setValue( 10 );
		ui->UISpinBox->setValue( 60 );
		ui->LLkComboBox->setCurrentItem( LNC_ERROR - 1 );
		ui->EDMComboBox->setCurrentItem( 0 );
		ui->PNLineEdit->setText( "ALL" );
		ui->IALineEdit->setText( "" );
		ui->DULineEdit->setText( "" );
		ui->DPLineEdit->setText( "" );
		ui->DPWCheckBox->setChecked( false );
		ui->UBCheckBox->setChecked( false );
	}
	else if ( ui->tabWidget->currentPageIndex() == 1 )
	{
		ui->AppListView->clear();
		defaultApps();
	} else if ( ui->tabWidget->currentPageIndex() == 2 ) {
		QListBox *enabledList = ui->PActionSelector->availableListBox();
		QListBox *disabledList = ui->PActionSelector->selectedListBox();
		int i;
		int cnt;

		cnt = disabledList->count();
		for ( i = 0; i < cnt; ++i ) {
			enabledList->insertItem( disabledList->text( 0 ) );
			disabledList->removeItem( 0 );
		}
		enabledList->sort();
	}

	emit changed( true );
}

void KNetworkClientConfig::save()
{
	unsigned int i;
	int len;
	char path[ PATH_MAX ];
	char disabledPlugins[ PATH_MAX ];
	struct pluginWidget *entry;

	disabledPlugins[ 0 ] = '\0';
	QListBox *disabledList = ui->PActionSelector->selectedListBox();
	for ( i = 0; i < disabledList->count(); ++i ) {
		len = strlen( disabledPlugins );
		snprintf( disabledPlugins+len, PATH_MAX - len, "%s,", disabledList->text( i ).ascii() );
	}

	clearKeys( "networkclient/Applications", USER_ROOT );
	initKeys( "LNC" );
//	initKeys( "networkclient/Applications" );
	initKeys( "networkclient" );

	setNumKey( "LNC", "BrowselistUpdateInterval", ui->BUISpinBox->value() * 60, USER_ROOT );
	setNumKey( "LNC", "UmountInterval", ui->UISpinBox->value(), USER_ROOT );
	setNumKey( "LNC", "LogLevel", ui->LLkComboBox->currentItem() + 1, USER_ROOT );
	setNumKey( "LNC", "EnableDebugMode", ui->EDMComboBox->currentItem(), USER_ROOT );
	setCharKey( "LNC", "DisabledPlugins", disabledPlugins, USER_ROOT );
	setCharKey( "LNC", "DefaultUsername", ui->DULineEdit->text(), USER_ROOT );
	setCharKey( "LNC", "DefaultPassword", ui->DPLineEdit->text(), USER_ROOT );
	setNumKey( "LNC", "DisablePasswordWriting", ui->DPWCheckBox->isChecked(), USER_ROOT );
	setNumKey( "LNC", "UseBroadcast", ui->UBCheckBox->isChecked(), USER_ROOT );
	setCharKey( "LNC", "PingNetworks", ui->PNLineEdit->text(), USER_ROOT );
	setCharKey( "LNC", "IgnoreAddresses", ui->IALineEdit->text(), USER_ROOT );

	i = 0;
	for ( QListViewItem * item = ui->AppListView->itemAtIndex( i ); item; item = ui->AppListView->itemAtIndex( ++i ) )
	{
		QStringList list;
		QString string;

		if ( item->text( 1 ) == "" )
			list.append( "Null" );
		else
			list.append( item->text( 1 ) );

		if ( item->text( 2 ) == i18n( "Yes" ) )
			list.append( "Yes" );
		else
			list.append( "No" );

		if ( item->text( 3 ) == i18n( "Yes" ) )
			list.append( "Yes" );
		else
			list.append( "No" );

		if ( item->text( 4 ) == i18n( "Yes" ) )
			list.append( "Yes" );
		else
			list.append( "No" );

		if ( item->text( 5 ) == i18n( "Yes" ) )
			list.append( "Yes" );
		else
			list.append( "No" );
		string = list.join( "," );
		setCharKey( "networkclient/Applications", item->text( 0 ), string, USER_ROOT );
	}

	forEachListEntry( &pluginWidgetList, entry ) {
		if ( entry->type == LINEEDIT ) {
			KLineEdit *lineEdit = ( KLineEdit *) entry->widget;
			snprintf( path, PATH_MAX, "LNC/plugins/%s", entry->group->name() );
			setCharKey( path, lineEdit->name(), lineEdit->text().ascii(), USER_ROOT );
		} else if ( entry->type == CHECKBOX ) {
			QCheckBox *checkBox = ( QCheckBox *) entry->widget;
			snprintf( path, PATH_MAX, "LNC/plugins/%s", entry->group->name() );
			setCharKey( path, checkBox->name(), checkBox->isChecked() ? "True" : "False", USER_ROOT );
		} else if ( entry->type == SPINBOX ) {
			KIntSpinBox *spinbox = ( KIntSpinBox *) entry->widget;
			snprintf( path, PATH_MAX, "LNC/plugins/%s", entry->group->name() );
			setNumKey( path, spinbox->name(), spinbox->value(), USER_ROOT );
		}
	}

	cleanupKeys( "LNC" );
//	cleanupKeys( "networkclient/Applications" );
	cleanupKeys( "networkclient" );

	emit changed( true );
}

int KNetworkClientConfig::buttons ()
{
	return KCModule::Default | KCModule::Apply | KCModule::Help;
}

void KNetworkClientConfig::configChanged()
{

	emit changed( true );
}

QString KNetworkClientConfig::quickHelp() const
{
	return i18n( "Helpful information about the knetworkclientconfig module." );
}

// ------------------------------------------------------------------------

extern "C"
{

	KCModule *create_KNetworkClientConfig( QWidget * parent, const char * name )
	{
		KGlobal::locale() ->insertCatalogue( "KNetworkClientConfig" );
		return new KNetworkClientConfig( parent, name );
	}
}

#include "knetworkclientconfig.moc"
