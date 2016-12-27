/***************************************************************************
 *   Copyright (C) 2011 by David Pinelo   *
 *   alepherp@alephsistemas.es   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include <QtPlugin>
#include <alepherpdaobusiness.h>
#include <alepherpdbcommons.h>

AERPMainWindowPlugin::AERPMainWindowPlugin(QObject *parent) : QObject (parent)
{
    m_initialized = false;
    m_mainWindow = NULL;
}

void AERPMainWindowPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if ( m_initialized )
    {
        return;
    }

    m_initialized = true;
}

bool AERPMainWindowPlugin::isInitialized() const
{
    return m_initialized;
}

QWidget *AERPMainWindowPlugin::createWidget(QWidget *parent)
{
    m_mainWindow = new AERPMainWindow(parent);
    return m_mainWindow;
}

QString AERPMainWindowPlugin::name() const
{
    return "AERPMainWindow";
}

QString AERPMainWindowPlugin::group() const
{
    return "AlephERP";
}

QIcon AERPMainWindowPlugin::icon() const
{
    return QIcon(":/images/PERPMainWindow.png");
}

QString AERPMainWindowPlugin::toolTip() const
{
    return tr("QMainWindow personalizable");
}

QString AERPMainWindowPlugin::whatsThis() const
{
    return tr("QMainWindow personalizable");
}

bool AERPMainWindowPlugin::isContainer() const
{
    return false;
}

QString AERPMainWindowPlugin::domXml() const
{
    return "<ui language=\"c++\">\n"
           " <widget class=\"AERPMainWindow\" name=\"AERPMainWindow\">\n"
           "  <property name=\"geometry\">\n"
           "   <rect>\n"
           "    <x>0</x>\n"
           "    <y>0</y>\n"
           "    <width>100</width>\n"
           "    <height>25</height>\n"
           "   </rect>\n"
           "  </property>\n"
           " </widget>\n"
           "</ui>";
}

QString AERPMainWindowPlugin::includeFile() const
{
    return "AERPMainWindow.h";
}
