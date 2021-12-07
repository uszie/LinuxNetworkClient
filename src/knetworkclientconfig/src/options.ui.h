/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/


void Options::setDisabled()
{
	if ( DisabledCheckBox->isChecked() )
		disabled = 1;
	else
		disabled = 0;
}

void Options::setFakeSyscall()
{
	if ( FakeCheckBox->isChecked() )
		fakeSyscall = 1;
	else
		fakeSyscall = 0;
}

void Options::setOpendir()
{
	if ( OpendirCheckBox->isChecked() )
		opendir = 1;
	else
		opendir = 0;
}

/*void Options::setChdir()
{
	if ( ChdirCheckBox->isChecked() )
		chdir = 1;
	else
		chdir = 0;
}

void Options::setAccess()
{
	if ( AccessCheckBox->isChecked() )
		access = 1;
	else
		access = 0;
}*/

void Options::setStat()
{
	if ( StatCheckBox->isChecked() )
		stat = 1;
	else
		stat = 0;
}

/*void Options::setLstat()
{
	if ( LstatCheckBox->isChecked() )
		lstat = 1;
	else
		lstat = 0;
}*/
