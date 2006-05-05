/*
 *  resourcelocalwidget.cpp  -  configuration widget for a local file calendar resource
 *  Program:  kalarm
 *  Copyright (c) 2006 by David Jarvie <software@astrojar.org.uk>
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
#include <typeinfo>

#include <QLabel>
#include <QGridLayout>

#include <kurlrequester.h>
#include <klocale.h>
#include <kdebug.h>

#include "resourcelocal.h"
#include "resourcelocalwidget.moc"


ResourceLocalConfigWidget::ResourceLocalConfigWidget(QWidget* parent)
	: ResourceConfigWidget(parent)
{
	QGridLayout* layout = new QGridLayout(this);

	QLabel* label = new QLabel(i18n("Location:"), this);
	layout->addWidget(label, 1, 0);
	mURL = new KUrlRequester(this);
	layout->addWidget(mURL, 1, 1);
}

void ResourceLocalConfigWidget::loadSettings(KRES::Resource* resource)
{
kDebug(5950)<<"ResourceLocalConfigWidget::loadSettings("<<typeid(resource).name()<<")\n";
//	KAResourceLocal* res = dynamic_cast<KAResourceLocal*>(resource);
	KAResourceLocal* res = static_cast<KAResourceLocal*>(resource);
	if (!res)
		kError(5950) << "ResourceLocalConfigWidget::loadSettings(KAResourceLocal): cast failed" << endl;
	else
	{
		ResourceConfigWidget::loadSettings(resource);
		mURL->setURL(res->fileName());
#ifndef NDEBUG
		kDebug(5950) << "ResourceLocalConfigWidget::loadSettings(): File " << mURL->url() << " type " << res->typeName() << endl;
#endif
	}
}

void ResourceLocalConfigWidget::saveSettings(KRES::Resource* resource)
{
//	KAResourceLocal* res = dynamic_cast<KAResourceLocal*>(resource);
	KAResourceLocal* res = static_cast<KAResourceLocal*>(resource);
	if (!res)
		kDebug(5950) << "ResourceLocalConfigWidget::saveSettings(KAResourceLocal): cast failed" << endl;
	else
		res->setFileName(mURL->url());
}
