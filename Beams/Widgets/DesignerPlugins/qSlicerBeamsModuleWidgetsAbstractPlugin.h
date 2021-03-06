/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Csaba Pinter, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/\

#ifndef __qSlicerBeamsModuleWidgetsAbstractPlugin_h
#define __qSlicerBeamsModuleWidgetsAbstractPlugin_h

#include <QtGlobal>
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QDesignerCustomWidgetInterface>
#else
#include <QtUiPlugin/QDesignerCustomWidgetInterface>
#endif

#include "qSlicerBeamsModuleWidgetsPluginsExport.h"

class Q_SLICER_MODULE_BEAMS_WIDGETS_PLUGINS_EXPORT qSlicerBeamsModuleWidgetsAbstractPlugin
    : public QDesignerCustomWidgetInterface
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
  Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetInterface")
#endif
  Q_INTERFACES(QDesignerCustomWidgetInterface);

public:

  qSlicerBeamsModuleWidgetsAbstractPlugin();
  // Don't reimplement this method.
  QString group() const;
  // You can reimplement these methods
  QIcon icon() const override;
  QString toolTip() const override;
  QString whatsThis() const override;
};

#endif
