/***************************************************************************
                         knetworkclientconfig.h  -  description
                            -------------------
   copyright            : (C) 2003 by Usarin Heininga
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
#ifndef KNETWORKCLIENTCONFIG_H_
#define KNETWORKCLIENTCONFIG_H_

#include <kcmodule.h>
#include <kaboutdata.h>
#include <qbuttongroup.h>
#include "knetworkclientconfigui.h"

class KNetworkClientConfig: public KCModule
{
		Q_OBJECT
	public:
		KNetworkClientConfig( QWidget *parent = 0L, const char *name = 0L );
		virtual ~KNetworkClientConfig();

/*		QString getUserConfigurationFile();
		QString getUserPasswordFile();
		QString getUserLogFile();
		QString getUserNetworkDirectory();*/
		void createPluginLineEdit( QButtonGroup *group, QGridLayout *layout, const char *name, const char *value );
		void createPluginCheckBox( QButtonGroup *group, QGridLayout *layout, const char *name, bool value );
		void createPluginSpinBox( QButtonGroup *group, QGridLayout *layout, const char *name, int value );
		void load();
		void save();
		void defaults();
		int buttons();
		void addListViewItem( const char *application, const char *string );
		void addListViewItem( const QString app, const QString args = "", const QString disabled = "No", const QString fakeSyscall = "No", const QString opendir = "Yes", const QString stat = "Yes" );
		void addListViewItem( QStringList *list );
		void defaultApps();
		QString quickHelp() const;
		const KAboutData* aboutData() { return myAboutData; };

	public slots:
		void configChanged();

	private:
		KAboutData *myAboutData;
		KNetworkClientConfigUI *ui;
		QWidget *pluginConfigTab;
		QSpacerItem *pluginConfigSpacer;
		QGridLayout *pluginConfigLayout;
		QGridLayout *buttonLayout[ 10 ];
		QGridLayout *tmpLayout;
		QButtonGroup *buttonGroup [ 10 ];
		QButtonGroup *tmpGroup;
};

#endif
