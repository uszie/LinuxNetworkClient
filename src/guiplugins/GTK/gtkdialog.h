/*#include <gdk/gdkkeysyms.h>*/
#include <gtk/gtk.h>

gchar *username;
gchar *password;

void setUsername( char *user );
void setPassword( char *pass );
gchar *getUsername( void );
gchar *getPassword( void );
gint isCanceled( void );

void add_pixmap_directory( const gchar *directory );
GtkWidget *GTKPasswordDialog ( const gchar *title, const char *URL );
