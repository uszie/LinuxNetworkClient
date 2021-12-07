#include"config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "gtkdialog.h"


int PasswordDialogCallBack( void );
GtkWidget *create_pixmap( GtkWidget *widget, const gchar *filename );

GtkWidget *usernameEntry;
GtkWidget *passwordEntry;
gint userCancel;
static GList *pixmaps_directories = NULL;

int PasswordDialogCallBack( void ) {
	gchar * tmp;

	userCancel = 0;

	tmp = ( gchar * ) gtk_entry_get_text( GTK_ENTRY( usernameEntry ) );
	if ( tmp && tmp[ 0 ] != '\0' )
		username = strdup( tmp );

	tmp = ( gchar * ) gtk_entry_get_text( GTK_ENTRY( passwordEntry ) );
	if ( tmp && tmp[ 0 ] != '\0' )
		password = strdup( tmp );

	gtk_main_quit();
	return 0;
}

void setUsername( char *user ) {
	if ( user )
		gtk_entry_set_text( GTK_ENTRY( usernameEntry ), user );
}

void setPassword( char *pass ) {
	if ( pass )
		gtk_entry_set_text( GTK_ENTRY( passwordEntry ), pass );
}

gchar *getUsername( void ) {
	return username;
}

gchar *getPassword( void ) {
	return password;
}

gint isCanceled( void ) {
	return userCancel;
}

void add_pixmap_directory( const gchar *directory ) {
	pixmaps_directories = g_list_prepend ( pixmaps_directories, g_strdup ( directory ) );
}

/*static gchar *find_pixmap_file(const gchar *filename)
{
	GList *elem;

	elem = pixmaps_directories;
	while (elem)
	{
		gchar *pathname = g_strdup_printf ("%s%s%s", (gchar*)elem->data, G_DIR_SEPARATOR_S, filename);
		if (g_file_test (pathname, G_FILE_TEST_EXISTS))
			return pathname;
		g_free (pathname);
		elem = elem->next;
	}
	return NULL;
}*/

GtkWidget *create_pixmap( GtkWidget *widget, const gchar *filename ) {
	/*	gchar *pathname = NULL;*/
	GtkWidget * pixmap;

	( void ) widget;
	/*	if (!filename || !filename[0])
			return gtk_image_new();*/

	/*	pathname = find_pixmap_file(filename);

		if (!pathname)
	    {
			g_warning("Couldn't find pixmap file: %s", filename);
			return gtk_image_new();
		}*/

	pixmap = gtk_image_new_from_file(  /*path*/filename );
	/*	g_free(pathname);*/
	return pixmap;
}

GtkWidget *GTKPasswordDialog ( const gchar *title, const char *URL ) {
	GtkWidget *GTKDialog;
	GtkWidget *dialogVBox;
	GtkWidget *messageLabel;
	GtkWidget *dialogImage;
	GtkWidget *dialogTable;
	GtkWidget *usernameLabel;
	GtkWidget *passwordLabel;
	GtkWidget *dialog_action_area1;
	GtkWidget *cancelButton;
	GtkWidget *okButton;
	char message[ BUFSIZ ];
	char path[ PATH_MAX ];

	userCancel = 1;
	username = password = NULL;

	GTKDialog = gtk_dialog_new();
	gtk_window_set_title ( GTK_WINDOW ( GTKDialog ), title );
	gtk_window_set_position ( GTK_WINDOW ( GTKDialog ), GTK_WIN_POS_CENTER );
	gtk_window_set_modal ( GTK_WINDOW ( GTKDialog ), TRUE );

	dialogVBox = GTK_DIALOG ( GTKDialog ) ->vbox;
	gtk_box_set_spacing ( GTK_BOX ( dialogVBox ), 10 );
	gtk_widget_show ( dialogVBox );

	dialogTable = gtk_table_new ( 3, 2, FALSE );
	gtk_widget_show ( dialogTable );
	gtk_box_pack_start ( GTK_BOX ( dialogVBox ), dialogTable, TRUE, TRUE, 0 );
	gtk_container_set_border_width ( GTK_CONTAINER ( dialogTable ), 4 );
	gtk_table_set_row_spacings ( GTK_TABLE ( dialogTable ), 10 );
	gtk_table_set_col_spacings ( GTK_TABLE ( dialogTable ), 10 );

	sprintf( message, "You need to supply a username and\na password to access this service\n\n%s", URL );
	messageLabel = gtk_label_new ( message );
	gtk_widget_show ( messageLabel );
	gtk_table_attach ( GTK_TABLE ( dialogTable ), messageLabel, 1, 2, 0, 1,
	                   ( GtkAttachOptions ) ( GTK_FILL ),
	                   ( GtkAttachOptions ) ( 0 ), 0, 0 );
	gtk_label_set_justify ( GTK_LABEL ( messageLabel ), GTK_JUSTIFY_LEFT );
	gtk_label_set_line_wrap ( GTK_LABEL ( messageLabel ), TRUE );
	gtk_misc_set_alignment ( GTK_MISC ( messageLabel ), 0, 0.5 );

	snprintf( path, PATH_MAX, "%s/keys.png", APP_DATA_DIR );
	dialogImage = create_pixmap ( GTKDialog, path );
	gtk_widget_show ( dialogImage );
	gtk_table_attach ( GTK_TABLE ( dialogTable ), dialogImage, 0, 1, 0, 1,
	                   ( GtkAttachOptions ) ( GTK_FILL ),
	                   ( GtkAttachOptions ) ( GTK_FILL ), 0, 0 );

	usernameEntry = gtk_entry_new();
	gtk_widget_show ( usernameEntry );
	gtk_table_attach ( GTK_TABLE ( dialogTable ), usernameEntry, 1, 2, 1, 2,
	                   ( GtkAttachOptions ) ( GTK_EXPAND | GTK_FILL ),
	                   ( GtkAttachOptions ) ( GTK_EXPAND ), 0, 0 );

	passwordEntry = gtk_entry_new ();
	gtk_widget_show ( passwordEntry );
	gtk_table_attach ( GTK_TABLE ( dialogTable ), passwordEntry, 1, 2, 2, 3,
	                   ( GtkAttachOptions ) ( GTK_EXPAND | GTK_FILL ),
	                   ( GtkAttachOptions ) ( GTK_EXPAND ), 0, 0 );

	usernameLabel = gtk_label_new_with_mnemonic ( "_Username:" );
	gtk_label_set_mnemonic_widget ( GTK_LABEL ( usernameLabel ), usernameEntry );

	gtk_widget_show ( usernameLabel );
	gtk_table_attach ( GTK_TABLE ( dialogTable ), usernameLabel, 0, 1, 1, 2,
	                   ( GtkAttachOptions ) ( GTK_FILL ),
	                   ( GtkAttachOptions ) ( GTK_EXPAND ), 0, 0 );
	gtk_label_set_justify ( GTK_LABEL ( usernameLabel ), GTK_JUSTIFY_LEFT );
	gtk_misc_set_alignment ( GTK_MISC ( usernameLabel ), 1, 0.5 );

	passwordLabel = gtk_label_new_with_mnemonic ( "_Password:" );
	gtk_label_set_mnemonic_widget ( GTK_LABEL ( passwordLabel ), passwordEntry );
	gtk_widget_show ( passwordLabel );
	gtk_table_attach ( GTK_TABLE ( dialogTable ), passwordLabel, 0, 1, 2, 3,
	                   ( GtkAttachOptions ) ( GTK_FILL ),
	                   ( GtkAttachOptions ) ( GTK_EXPAND ), 0, 0 );
	gtk_label_set_justify ( GTK_LABEL ( passwordLabel ), GTK_JUSTIFY_LEFT );
	gtk_misc_set_alignment ( GTK_MISC ( passwordLabel ), 1, 0.5 );

	dialog_action_area1 = GTK_DIALOG ( GTKDialog ) ->action_area;
	gtk_widget_show ( dialog_action_area1 );
	gtk_button_box_set_layout ( GTK_BUTTON_BOX ( dialog_action_area1 ), GTK_BUTTONBOX_END );

	cancelButton = gtk_button_new_from_stock ( "gtk-cancel" );
	gtk_widget_show ( cancelButton );
	gtk_dialog_add_action_widget ( GTK_DIALOG ( GTKDialog ), cancelButton, GTK_RESPONSE_CANCEL );
	GTK_WIDGET_SET_FLAGS ( cancelButton, GTK_CAN_DEFAULT );

	okButton = gtk_button_new_from_stock ( "gtk-ok" );
	gtk_widget_show ( okButton );
	gtk_dialog_add_action_widget ( GTK_DIALOG ( GTKDialog ), okButton, GTK_RESPONSE_OK );
	GTK_WIDGET_SET_FLAGS ( okButton, GTK_CAN_DEFAULT );

	g_signal_connect ( ( gpointer ) GTKDialog, "destroy",
	                   G_CALLBACK ( gtk_main_quit ),
	                   NULL );
	g_signal_connect ( ( gpointer ) cancelButton, "clicked",
	                   G_CALLBACK ( gtk_main_quit ),
	                   NULL );
	g_signal_connect ( ( gpointer ) okButton, "clicked",
	                   G_CALLBACK ( PasswordDialogCallBack ),
	                   NULL );

	gtk_dialog_set_default_response ( GTK_DIALOG ( GTKDialog ), GTK_RESPONSE_OK );

	return GTKDialog;
}
