#include"config.h"
#include "qtdialog.h"

QTPasswordDialog::QTPasswordDialog( QWidget* parent, const char *name, bool modal )
		: QDialog( parent, name, modal, WStyle_DialogBorder ) {
	QWidget * main = new QFrame( this );
	QBoxLayout *mainLayout = new QBoxLayout( this, QBoxLayout::TopToBottom, 11, 6 );
	mainLayout->addWidget( main, 10 );

	QFrame *Separator = new QFrame( this );
	Separator->setFrameShape( QFrame::HLine );
	Separator->setFrameShadow( QFrame::Sunken );
	mainLayout->addWidget( Separator );

	QWidget *buttonBox = new QWidget( this );
	QBoxLayout *buttonBoxLayout = new QBoxLayout( buttonBox, QBoxLayout::LeftToRight, 0, 6 );
	mainLayout->addWidget( buttonBox );

	QSpacerItem* Spacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
	buttonBoxLayout->addItem( Spacer );

	QPushButton *buttonOk = new QPushButton( tr( "Ok" ), buttonBox );
	buttonOk->setAutoDefault( TRUE );
	buttonOk->setDefault( TRUE );
	buttonBoxLayout->addWidget( buttonOk );

	QPushButton *buttonCancel = new QPushButton( tr( "Cancel" ), buttonBox );
	buttonCancel->setAutoDefault( TRUE );
	buttonBoxLayout->addWidget( buttonCancel );

	QGridLayout *dialogLayout = new QGridLayout( main, 9, 3, 6, 11 );
	dialogLayout->addColSpacing( 1, 5 );

	QLabel* pixLabel = new QLabel( main );
	;
	QString path( APP_DATA_DIR );
	path += "/keys.png";
	QPixmap promptPix( /*"/usr/share/apps/networkclient/keys.png"*/path );
	pixLabel->setPixmap( promptPix );
	pixLabel->setAlignment( Qt::AlignLeft | Qt::AlignVCenter );
	pixLabel->setFixedSize( pixLabel->sizeHint() );
	dialogLayout->addWidget( pixLabel, 0, 0, Qt::AlignLeft );

	promptLabel = new QLabel( main );
	//	promptLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter|Qt::WordBreak);
	//	promptLabel->setText("You need to supply a username and a password to access this host");
	//	int w = QMIN(promptLabel->sizeHint().width(), 250);
	//	promptLabel->setFixedSize(w, promptLabel->heightForWidth( w ));
	dialogLayout->addWidget( promptLabel, 0, 2, Qt::AlignLeft );
	dialogLayout->addRowSpacing( 1, 7 );

	QLabel *userLabel = new QLabel( tr( "Username:" ), main );
	userLabel->setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
	userLabel->setFixedSize( userLabel->sizeHint() );
	dialogLayout->addWidget( userLabel, 4, 0 );

	QHBox* userBox = new QHBox( main );
	dialogLayout->addWidget( userBox, 4, 2 );

	userEdit = new QLineEdit( userBox );
	QSize s = userEdit->sizeHint();
	userEdit->setFixedHeight( s.height() );
	userEdit->setMinimumWidth( s.width() );
	userEdit->setFocus();
	userLabel->setBuddy( userEdit );
	dialogLayout->addRowSpacing( 5, 4 );


	QLabel *passLabel = new QLabel( tr( "&Password:" ), main );
	passLabel->setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
	passLabel->setFixedSize( passLabel->sizeHint() );
	dialogLayout->addWidget( passLabel, 6, 0 );

	QHBox *passBox = new QHBox( main );
	dialogLayout->addWidget( passBox, 6, 2 );

	passEdit = new QLineEdit( passBox );
	passEdit->setEchoMode( QLineEdit::Password );
	s = passEdit->sizeHint();
	passEdit->setFixedHeight( s.height() );
	passEdit->setMinimumWidth( s.width() );
	passLabel->setBuddy( passEdit );

	connect( userEdit, SIGNAL( returnPressed() ), passEdit, SLOT( setFocus() ) );
	connect( passEdit, SIGNAL( returnPressed() ), this, SLOT( accept() ) );
	connect( buttonOk, SIGNAL( clicked() ), this, SLOT( accept() ) );
	connect( buttonCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );
}

QTPasswordDialog::~QTPasswordDialog() {}

const char *QTPasswordDialog::getUsername() {
		return userEdit->text();
}

const char *QTPasswordDialog::getPassword() {
	return passEdit->text();
}
