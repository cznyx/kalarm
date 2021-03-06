/*
 *  sounddlg.cpp  -  sound file selection and configuration dialog and widget
 *  Program:  kalarm
 *  Copyright © 2005-2012 by David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kalarm.h"
#include "sounddlg.h"

#include "checkbox.h"
#include "functions.h"
#include "groupbox.h"
#include "lineedit.h"
#include "pushbutton.h"
#include "slider.h"
#include "soundpicker.h"
#include "spinbox.h"

#include <QIcon>
#include <KGlobal>
#include <KLocalizedString>
#include <kstandarddirs.h>
#include <phonon/mediaobject.h>
#include <phonon/audiooutput.h>

#include <QLabel>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QShowEvent>
#include <QResizeEvent>
#include <QDialogButtonBox>
#include <QStyle>
#include "kalarm_debug.h"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString SoundWidget::i18n_chk_Repeat()      { return i18nc("@option:check", "Repeat"); }

static const char SOUND_DIALOG_NAME[] = "SoundDialog";


SoundDlg::SoundDlg(const QString& file, float volume, float fadeVolume, int fadeSeconds, int repeatPause,
                   const QString& caption, QWidget* parent)
    : QDialog(parent),
      mReadOnly(false)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    mSoundWidget = new SoundWidget(true, true, this);
    layout->addWidget(mSoundWidget);

    mButtonBox = new QDialogButtonBox;
    mButtonBox->addButton(QDialogButtonBox::Ok);
    mButtonBox->addButton(QDialogButtonBox::Cancel);
    connect(mButtonBox, &QDialogButtonBox::clicked,
            this, &SoundDlg::slotButtonClicked);
    layout->addWidget(mButtonBox);

    setWindowTitle(caption);

    // Restore the dialog size from last time
    QSize s;
    if (KAlarm::readConfigWindowSize(SOUND_DIALOG_NAME, s))
        resize(s);

    // Initialise the control values
    mSoundWidget->set(file, volume, fadeVolume, fadeSeconds, repeatPause);
}

/******************************************************************************
* Set the read-only status of the dialog.
*/
void SoundDlg::setReadOnly(bool readOnly)
{
    if (readOnly != mReadOnly)
    {
        mSoundWidget->setReadOnly(readOnly);
        mReadOnly = readOnly;
        if (readOnly)
        {
            mButtonBox->clear();
            mButtonBox->addButton(QDialogButtonBox::Cancel);
        }
        else
        {
            mButtonBox->clear();
            mButtonBox->addButton(QDialogButtonBox::Ok);
            mButtonBox->addButton(QDialogButtonBox::Cancel);
        }
    }
}

QUrl SoundDlg::getFile() const
{
    QUrl url;
    mSoundWidget->file(url);
    return url;
}

/******************************************************************************
* Called when the dialog's size has changed.
* Records the new size in the config file.
*/
void SoundDlg::resizeEvent(QResizeEvent* re)
{
    if (isVisible())
        KAlarm::writeConfigWindowSize(SOUND_DIALOG_NAME, re->size());
    QDialog::resizeEvent(re);
}

/******************************************************************************
* Called when the OK or Cancel button is clicked.
*/
void SoundDlg::slotButtonClicked(QAbstractButton *button)
{
    if (button == mButtonBox->button(QDialogButtonBox::Ok))
    {
        if (mReadOnly)
            reject();
        else if (mSoundWidget->validate(true))
            accept();
    }
    else
        reject();

}


/*=============================================================================
= Class SoundWidget
= Select a sound file and configure how to play it.
=============================================================================*/
QString SoundWidget::mDefaultDir;

SoundWidget::SoundWidget(bool showPlay, bool showRepeat, QWidget* parent)
    : QWidget(parent),
      mFilePlay(nullptr),
      mRepeatGroupBox(nullptr),
      mRepeatPause(nullptr),
      mPlayer(nullptr),
      mReadOnly(false),
      mEmptyFileAllowed(false)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* label = nullptr;
    if (!showPlay)
    {
        label = new QLabel(i18nc("@label", "Sound file:"), this);
        layout->addWidget(label);
    }

    QWidget* box = new QWidget(this);
    layout->addWidget(box);
    QHBoxLayout* boxHLayout = new QHBoxLayout(box);
    boxHLayout->setMargin(0);
    boxHLayout->setSpacing(0);

    if (showPlay)
    {
        // File play button
        mFilePlay = new QPushButton(box);
        boxHLayout->addWidget(mFilePlay);
        mFilePlay->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        connect(mFilePlay, &QPushButton::clicked, this, &SoundWidget::playSound);
        mFilePlay->setToolTip(i18nc("@info:tooltip", "Test the sound"));
        mFilePlay->setWhatsThis(i18nc("@info:whatsthis", "Play the selected sound file."));
    }

    // File name edit box
    mFileEdit = new LineEdit(LineEdit::Url, box);
    boxHLayout->addWidget(mFileEdit);
    mFileEdit->setAcceptDrops(true);
    mFileEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the name or URL of a sound file to play."));
    if (label)
        label->setBuddy(mFileEdit);
    connect(mFileEdit, &LineEdit::textChanged, this, &SoundWidget::changed);

    // File browse button
    mFileBrowseButton = new PushButton(box);
    boxHLayout->addWidget(mFileBrowseButton);
    mFileBrowseButton->setIcon(QIcon(QIcon::fromTheme(QStringLiteral("document-open"))));
    int size = mFileBrowseButton->sizeHint().height();
    mFileBrowseButton->setFixedSize(size, size);
    connect(mFileBrowseButton, &PushButton::clicked, this, &SoundWidget::slotPickFile);
    mFileBrowseButton->setToolTip(i18nc("@info:tooltip", "Choose a file"));
    mFileBrowseButton->setWhatsThis(i18nc("@info:whatsthis", "Select a sound file to play."));

    if (mFilePlay)
    {
        int size = qMax(mFilePlay->sizeHint().height(), mFileBrowseButton->sizeHint().height());
        mFilePlay->setFixedSize(size, size);
        mFileBrowseButton->setFixedSize(size, size);
    }

    if (showRepeat)
    {
        // Sound repetition checkbox
        mRepeatGroupBox = new GroupBox(i18n_chk_Repeat(), this);
        mRepeatGroupBox->setCheckable(true);
        mRepeatGroupBox->setWhatsThis(i18nc("@info:whatsthis", "If checked, the sound file will be played repeatedly for as long as the message is displayed."));
        connect(mRepeatGroupBox, &GroupBox::toggled, this, &SoundWidget::changed);
        layout->addWidget(mRepeatGroupBox);
        QVBoxLayout* glayout = new QVBoxLayout(mRepeatGroupBox);

        // Pause between repetitions
        QWidget* box = new QWidget(mRepeatGroupBox);
        glayout->addWidget(box);
        boxHLayout = new QHBoxLayout(box);
        boxHLayout->setMargin(0);
        label = new QLabel(i18nc("@label:spinbox Length of time to pause between repetitions", "Pause between repetitions:"), box);
        boxHLayout->addWidget(label);
        label->setFixedSize(label->sizeHint());
        mRepeatPause = new SpinBox(0, 999, box);
        boxHLayout->addWidget(mRepeatPause);
        mRepeatPause->setSingleShiftStep(10);
        mRepeatPause->setFixedSize(mRepeatPause->sizeHint());
        label->setBuddy(mRepeatPause);
        connect(mRepeatPause, static_cast<void (SpinBox::*)(int)>(&SpinBox::valueChanged), this, &SoundWidget::changed);
        label = new QLabel(i18nc("@label", "seconds"), box);
        boxHLayout->addWidget(label);
        label->setFixedSize(label->sizeHint());
        box->setWhatsThis(i18nc("@info:whatsthis", "Enter how many seconds to pause between repetitions."));
    }

    // Volume
    QGroupBox* group = new QGroupBox(i18nc("@title:group Sound volume", "Volume"), this);
    layout->addWidget(group);
    QGridLayout* grid = new QGridLayout(group);
    grid->setMargin(style()->pixelMetric(QStyle::PM_DefaultChildMargin));
    grid->setSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    grid->setColumnStretch(2, 1);
    int indentWidth = 3 * style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing);
    grid->setColumnMinimumWidth(0, indentWidth);
    grid->setColumnMinimumWidth(1, indentWidth);

    // 'Set volume' checkbox
    box = new QWidget(group);
    grid->addWidget(box, 1, 0, 1, 3);
    boxHLayout = new QHBoxLayout(box);
    boxHLayout->setMargin(0);
    mVolumeCheckbox = new CheckBox(i18nc("@option:check", "Set volume"), box);
    boxHLayout->addWidget(mVolumeCheckbox);
    mVolumeCheckbox->setFixedSize(mVolumeCheckbox->sizeHint());
    connect(mVolumeCheckbox, &CheckBox::toggled, this, &SoundWidget::slotVolumeToggled);
    mVolumeCheckbox->setWhatsThis(i18nc("@info:whatsthis", "Select to choose the volume for playing the sound file."));

    // Volume slider
    mVolumeSlider = new Slider(0, 100, 10, Qt::Horizontal, box);
    boxHLayout->addWidget(mVolumeSlider);
    mVolumeSlider->setTickPosition(QSlider::TicksBelow);
    mVolumeSlider->setTickInterval(10);
    mVolumeSlider->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    mVolumeSlider->setWhatsThis(i18nc("@info:whatsthis", "Choose the volume for playing the sound file."));
    mVolumeCheckbox->setFocusWidget(mVolumeSlider);
    connect(mVolumeSlider, &Slider::valueChanged, this, &SoundWidget::changed);

    // Fade checkbox
    mFadeCheckbox = new CheckBox(i18nc("@option:check", "Fade"), group);
    mFadeCheckbox->setFixedSize(mFadeCheckbox->sizeHint());
    connect(mFadeCheckbox, &CheckBox::toggled, this, &SoundWidget::slotFadeToggled);
    mFadeCheckbox->setWhatsThis(i18nc("@info:whatsthis", "Select to fade the volume when the sound file first starts to play."));
    grid->addWidget(mFadeCheckbox, 2, 1, 1, 2, Qt::AlignLeft);

    // Fade time
    mFadeBox = new QWidget(group);
    grid->addWidget(mFadeBox, 3, 2, Qt::AlignLeft);
    boxHLayout = new QHBoxLayout(mFadeBox);
    boxHLayout->setMargin(0);
    label = new QLabel(i18nc("@label:spinbox Time period over which to fade the sound", "Fade time:"), mFadeBox);
    boxHLayout->addWidget(label);
    label->setFixedSize(label->sizeHint());
    mFadeTime = new SpinBox(1, 999, mFadeBox);
    boxHLayout->addWidget(mFadeTime);
    mFadeTime->setSingleShiftStep(10);
    mFadeTime->setFixedSize(mFadeTime->sizeHint());
    label->setBuddy(mFadeTime);
    connect(mFadeTime, static_cast<void (SpinBox::*)(int)>(&SpinBox::valueChanged), this, &SoundWidget::changed);
    label = new QLabel(i18nc("@label", "seconds"), mFadeBox);
    boxHLayout->addWidget(label);
    label->setFixedSize(label->sizeHint());
    mFadeBox->setWhatsThis(i18nc("@info:whatsthis", "Enter how many seconds to fade the sound before reaching the set volume."));

    // Fade slider
    mFadeVolumeBox = new QWidget(group);
    grid->addWidget(mFadeVolumeBox, 4, 2);
    boxHLayout = new QHBoxLayout(mFadeVolumeBox);
    boxHLayout->setMargin(0);
    label = new QLabel(i18nc("@label:slider", "Initial volume:"), mFadeVolumeBox);
    boxHLayout->addWidget(label);
    label->setFixedSize(label->sizeHint());
    mFadeSlider = new Slider(0, 100, 10, Qt::Horizontal, mFadeVolumeBox);
    boxHLayout->addWidget(mFadeSlider);
    mFadeSlider->setTickPosition(QSlider::TicksBelow);
    mFadeSlider->setTickInterval(10);
    mFadeSlider->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    label->setBuddy(mFadeSlider);
    connect(mFadeSlider, &Slider::valueChanged, this, &SoundWidget::changed);
    mFadeVolumeBox->setWhatsThis(i18nc("@info:whatsthis", "Choose the initial volume for playing the sound file."));

    slotVolumeToggled(false);
}

SoundWidget::~SoundWidget()
{
    delete mPlayer;   // this stops playing if not already stopped
    mPlayer = nullptr;
}

/******************************************************************************
* Set the controls' values.
*/
void SoundWidget::set(const QString& file, float volume, float fadeVolume, int fadeSeconds, int repeatPause)
{
    // Initialise the control values
    mFileEdit->setText(KAlarm::pathOrUrl(file));
    if (mRepeatGroupBox)
    {
        mRepeatGroupBox->setChecked(repeatPause >= 0);
        mRepeatPause->setValue(repeatPause >= 0 ? repeatPause : 0);
    }
    mVolumeCheckbox->setChecked(volume >= 0);
    mVolumeSlider->setValue(volume >= 0 ? static_cast<int>(volume*100) : 100);
    mFadeCheckbox->setChecked(fadeVolume >= 0);
    mFadeSlider->setValue(fadeVolume >= 0 ? static_cast<int>(fadeVolume*100) : 100);
    mFadeTime->setValue(fadeSeconds);
    slotVolumeToggled(volume >= 0);
}

/******************************************************************************
* Set the read-only status of the widget.
*/
void SoundWidget::setReadOnly(bool readOnly)
{
    if (readOnly != mReadOnly)
    {
        mFileEdit->setReadOnly(readOnly);
        mFileBrowseButton->setReadOnly(readOnly);
        if (mRepeatGroupBox)
            mRepeatGroupBox->setReadOnly(readOnly);
        mVolumeCheckbox->setReadOnly(readOnly);
        mVolumeSlider->setReadOnly(readOnly);
        mFadeCheckbox->setReadOnly(readOnly);
        mFadeTime->setReadOnly(readOnly);
        mFadeSlider->setReadOnly(readOnly);
        mReadOnly = readOnly;
    }
}

/******************************************************************************
* Return the file name typed in the edit field.
*/
QString SoundWidget::fileName() const
{
    return mFileEdit->text();
}

/******************************************************************************
* Validate the entered file and return it.
*/
bool SoundWidget::file(QUrl& url, bool showErrorMessage) const
{
    bool result = validate(showErrorMessage);
    url = mUrl;
    return result;
}

/******************************************************************************
* Return the entered repetition and volume settings:
* 'volume' is in range 0 - 1, or < 0 if volume is not to be set.
* 'fadeVolume is similar, with 'fadeTime' set to the fade interval in seconds.
*/
void SoundWidget::getVolume(float& volume, float& fadeVolume, int& fadeSeconds) const
{
    volume = mVolumeCheckbox->isChecked() ? (float)mVolumeSlider->value() / 100 : -1;
    if (mFadeCheckbox->isChecked())
    {
        fadeVolume  = (float)mFadeSlider->value() / 100;
        fadeSeconds = mFadeTime->value();
    }
    else
    {
        fadeVolume  = -1;
        fadeSeconds = 0;
    }
}

/******************************************************************************
* Return the entered repetition setting.
* Reply = seconds to pause between repetitions, or -1 if no repeat.
*/
int SoundWidget::repeatPause() const
{
    return mRepeatGroupBox && mRepeatGroupBox->isChecked() ? mRepeatPause->value() : -1;
}

/******************************************************************************
* Called when the dialog's size has changed.
* Records the new size in the config file.
*/
void SoundWidget::resizeEvent(QResizeEvent* re)
{
    mVolumeSlider->resize(mFadeSlider->size());
    QWidget::resizeEvent(re);
}

void SoundWidget::showEvent(QShowEvent* se)
{
    mVolumeSlider->resize(mFadeSlider->size());
    QWidget::showEvent(se);
}

/******************************************************************************
* Called when the file browser button is clicked.
*/
void SoundWidget::slotPickFile()
{
    QString url = SoundPicker::browseFile(mDefaultDir, mFileEdit->text());
    if (!url.isEmpty())
        mFileEdit->setText(KAlarm::pathOrUrl(url));
}

/******************************************************************************
* Called when the file play or stop button is clicked.
*/
void SoundWidget::playSound()
{
    if (mPlayer)
    {
        // The file is currently playing. Stop it.
        playFinished();
        return;
    }
    if (!validate(true))
        return;
#if 0
#warning Phonon::createPlayer() does not work
    mPlayer = Phonon::createPlayer(Phonon::MusicCategory, mUrl);
    mPlayer->setParent(this);
#else
    mPlayer = new Phonon::MediaObject(this);
    Phonon::AudioOutput* output = new Phonon::AudioOutput(Phonon::MusicCategory, mPlayer);
    mPlayer->setCurrentSource(mUrl);
    Phonon::createPath(mPlayer, output);
#endif
    connect(mPlayer, &Phonon::MediaObject::finished, this, &SoundWidget::playFinished);
    mFilePlay->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-stop")));   // change the play button to a stop button
    mFilePlay->setToolTip(i18nc("@info:tooltip", "Stop sound"));
    mFilePlay->setWhatsThis(i18nc("@info:whatsthis", "Stop playing the sound"));
    mPlayer->play();
}

/******************************************************************************
* Called when playing the file has completed, or to stop playing.
*/
void SoundWidget::playFinished()
{
    delete mPlayer;   // this stops playing if not already stopped
    mPlayer = nullptr;
    mFilePlay->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    mFilePlay->setToolTip(i18nc("@info:tooltip", "Test the sound"));
    mFilePlay->setWhatsThis(i18nc("@info:whatsthis", "Play the selected sound file."));
}

/******************************************************************************
* Check whether the specified sound file exists.
*/
bool SoundWidget::validate(bool showErrorMessage) const
{
    QString file = mFileEdit->text();
    if (file == mValidatedFile  &&  !file.isEmpty())
        return true;
    mValidatedFile = file;
    if (file.isEmpty()  &&  mEmptyFileAllowed)
    {
        mUrl.clear();
        return true;
    }
    KAlarm::FileErr err = KAlarm::checkFileExists(file, mUrl);
    if (err == KAlarm::FileErr_None)
        return true;
    if (err == KAlarm::FileErr_Nonexistent)
    {
        if (mUrl.isLocalFile()  &&  !file.startsWith(QStringLiteral("/")))
        {
            // It's a relative path.
            // Find the first sound resource that contains files.
            QStringList soundDirs = KGlobal::dirs()->resourceDirs("sound");
            if (!soundDirs.isEmpty())
            {
                QDir dir;
                dir.setFilter(QDir::Files | QDir::Readable);
                for (int i = 0, end = soundDirs.count();  i < end;  ++i)
                {
                    dir = soundDirs[i];
                    if (dir.isReadable() && dir.count() > 2)
                    {
                        QString f = soundDirs[i] + QDir::separator() + file;
                        err = KAlarm::checkFileExists(f, mUrl);
                        if (err == KAlarm::FileErr_None)
                            return true;
                        if (err != KAlarm::FileErr_Nonexistent)
                        {
                            file = f;   // for inclusion in error message
                            break;
                        }
                    }
                }
            }
            if (err == KAlarm::FileErr_Nonexistent)
            {
                QString f = QDir::homePath() + QDir::separator() + file;
                err = KAlarm::checkFileExists(f, mUrl);
                if (err == KAlarm::FileErr_None)
                    return true;
                if (err != KAlarm::FileErr_Nonexistent)
                    file = f;   // for inclusion in error message
            }
        }
    }
    mFileEdit->setFocus();
    if (showErrorMessage
    &&  KAlarm::showFileErrMessage(file, err, KAlarm::FileErr_BlankPlay, const_cast<SoundWidget*>(this)))
        return true;
    mValidatedFile.clear();
    mUrl.clear();
    return false;
}

/******************************************************************************
* Called when the Set Volume checkbox is toggled.
*/
void SoundWidget::slotVolumeToggled(bool on)
{
    mVolumeSlider->setEnabled(on);
    mFadeCheckbox->setEnabled(on);
    slotFadeToggled(on  &&  mFadeCheckbox->isChecked());
}

/******************************************************************************
* Called when the Fade checkbox is toggled.
*/
void SoundWidget::slotFadeToggled(bool on)
{
    mFadeBox->setEnabled(on);
    mFadeVolumeBox->setEnabled(on);
    Q_EMIT changed();
}

// vim: et sw=4:
