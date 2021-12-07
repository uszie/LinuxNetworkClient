#include <qlineedit.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qaccel.h>
#include <qhbox.h>
#include <qdialog.h>

class QGridLayout;
class QVBoxLayout;
class QHBoxLayout;
class QPushButton;

class QTPasswordDialog :public QDialog
{
//	Q_OBJECT

public:
	QTPasswordDialog(QWidget* parent = 0, const char *name = 0, bool modal = false);
	~QTPasswordDialog();

	const char *getUsername();
	const char *getPassword();

	QLabel *promptLabel;
	QLineEdit *userEdit;
	QLineEdit *passEdit;
};
