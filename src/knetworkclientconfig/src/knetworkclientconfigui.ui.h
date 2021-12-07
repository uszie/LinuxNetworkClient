/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename slots use Qt Designer which will
** update this file, preserving your code. Create an init() slot in place of
** a constructor, and a destroy() slot in place of a destructor.
*****************************************************************************/
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <string.h>
#include "knetworkclientconfig.h"
#include "options.h"
#include <qcheckbox.h>

void KNetworkClientConfigUI::appBrowse()
{
	QStringList files = KFileDialog::getOpenFileNames( "/usr/bin", "application/x-executable application/x-sharedlib", this, i18n( "Select Application" ) );

	for ( QStringList::Iterator it = files.begin(); it != files.end(); it++ ) {
		const char *ptr = strrchr( *it, '/' );
		if ( !ptr )
			ptr = *it;
		else
			++ptr;
		QListViewItem *item = AppListView->findItem( ptr, 0);
		if ( !item )
			addListViewEntry( ptr );
	}
}

void KNetworkClientConfigUI::addListViewEntry( const char * file)
{
//	QString entry = file;//AppLineEdit->text();
	if ( file )
	{
		QListViewItem * item = new QListViewItem( AppListView, 0 );
//		char *ptr = strrchr( entry, '/' );
//		if ( !ptr )
//			item->setText( 0, entry );
//		else
			item->setText( 0, file );

		item->setText( 1, "" );
		item->setText( 2, tr2i18n( "No" ) );
		item->setText( 3, tr2i18n( "No" ) );
		item->setText( 4, tr2i18n( "Yes" ) );
		item->setText( 5, tr2i18n( "Yes" ) );
//		AppLineEdit->clear();
	}
}

void KNetworkClientConfigUI::delListViewEntry()
{
	QListViewItem * item;

	item = AppListView->currentItem();
	if ( item )
		AppListView->removeItem( item );
}

void KNetworkClientConfigUI::optionsDialog()
{
	Options dlg( this );
	QListViewItem *item = AppListView->selectedItem();

	dlg.disabled = dlg.fakeSyscall = dlg.opendir = dlg.stat = 0;

	dlg.ArgLineEdit->setText( item->text( 1 ) );

	if ( strcmp( item->text( 2 ), tr2i18n( "Yes" ) ) == 0 )
	{
		dlg.DisabledCheckBox->setChecked( true );
		dlg.disabled = 1;
	}

	if ( strcmp( item->text( 3 ), tr2i18n( "Yes" ) ) == 0 )
	{
		dlg.FakeCheckBox->setChecked( true );
		dlg.fakeSyscall = 1;
	}

	if ( strcmp( item->text( 4 ), tr2i18n( "Yes" ) ) == 0 )
	{
		dlg.OpendirCheckBox->setChecked( true );
		dlg.opendir = 1;
	}

	if ( strcmp( item->text( 5 ), tr2i18n( "Yes" ) ) == 0 )
	{
		dlg.StatCheckBox->setChecked( true );
		dlg.stat = 1;
	}

	int retval = dlg.exec();
	if ( retval == 1 )
	{
		item->setText( 1, dlg.ArgLineEdit->text() );
		if ( dlg.disabled )
			item->setText( 2, tr2i18n( "Yes" ) );
		else
			item->setText( 2, tr2i18n( "No" ) );

		if ( dlg.fakeSyscall )
			item->setText( 3, tr2i18n( "Yes" ) );
		else
			item->setText( 3, tr2i18n( "No" ) );

		if ( dlg.opendir )
			item->setText( 4, tr2i18n( "Yes" ) );
		else
			item->setText( 4, tr2i18n( "No" ) );

		if ( dlg.stat )
			item->setText( 5, tr2i18n( "Yes" ) );
		else
			item->setText( 5, tr2i18n( "No" ) );
	}
}

void KNetworkClientConfigUI::toggleOptionsButton()
{
	QListViewItem * item = AppListView->selectedItem();
	if ( item )
		OptionsPushButton->setEnabled( true );
	else
		OptionsPushButton->setDisabled( true );
}

void KNetworkClientConfigUI::toggleRemoveButton()
{
	QListViewItem * item = AppListView->selectedItem();
	if ( item )
		RemovePushButton->setEnabled( true );
	else
		RemovePushButton->setDisabled( true );
}
