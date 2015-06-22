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

  This file was originally developed by Andras Lasso, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

// Segmentations includes
#include "vtkMRMLSegmentationDisplayNode.h"
#include "vtkMRMLSegmentationNode.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLColorTableNode.h>

// SegmentationCore includes
#include "vtkSegmentation.h"
#include "vtkOrientedImageData.h"
#include "vtkTopologicalHierarchy.h"

// VTK includes
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkVersion.h>
#include <vtkLookupTable.h>
#include <vtkVector.h>

// STD includes
#include <algorithm>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLSegmentationDisplayNode);

//----------------------------------------------------------------------------
vtkMRMLSegmentationDisplayNode::vtkMRMLSegmentationDisplayNode()
{
  this->PolyDataDisplayRepresentationName = NULL;
  this->EnableTransparencyInColorTableOff();
  this->SliceIntersectionVisibilityOn();

  this->SegmentationDisplayProperties.clear();
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationDisplayNode::~vtkMRMLSegmentationDisplayNode()
{
  this->SetPolyDataDisplayRepresentationName(NULL);
  this->SegmentationDisplayProperties.clear();
}

//----------------------------------------------------------------------------
void vtkMRMLSegmentationDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{

  Superclass::PrintSelf(os,indent);
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationDisplayNode::ProcessMRMLEvents ( vtkObject *caller,
                                           unsigned long event,
                                           void *callData )
{
  Superclass::ProcessMRMLEvents(caller, event, callData);

  //if (event == vtkCommand::ModifiedEvent)
  //  {
  //  this->UpdatePolyDataPipeline();
  //  }
}

//---------------------------------------------------------------------------
vtkMRMLColorTableNode* vtkMRMLSegmentationDisplayNode::CreateColorTableNode(const char* segmentationNodeName)
{
  if (!this->Scene)
  {
    vtkErrorMacro("CreateColorTableNode: Invalid MRML scene!");
    return NULL;
  }

  vtkSmartPointer<vtkMRMLColorTableNode> segmentationColorTableNode = vtkSmartPointer<vtkMRMLColorTableNode>::New();
  std::string segmentationColorTableNodeName = std::string(segmentationNodeName ? segmentationNodeName : "Segmentation") + GetColorTableNodeNamePostfix();
  segmentationColorTableNodeName = this->Scene->GenerateUniqueName(segmentationColorTableNodeName);
  segmentationColorTableNode->SetName(segmentationColorTableNodeName.c_str());
  segmentationColorTableNode->HideFromEditorsOff();
  segmentationColorTableNode->SetTypeToUser();
  segmentationColorTableNode->SetAttribute("Category", this->GetNodeTagName());
  this->Scene->AddNode(segmentationColorTableNode);

  // Initialize color table
  segmentationColorTableNode->SetNumberOfColors(2);
  segmentationColorTableNode->GetLookupTable()->SetTableRange(0,1);
  segmentationColorTableNode->AddColor(GetSegmentationColorNameBackground(), 0.0, 0.0, 0.0, 0.0); // Black background
  segmentationColorTableNode->AddColor(GetSegmentationColorNameInvalid(),
    vtkSegment::SEGMENT_COLOR_VALUE_INVALID[0], vtkSegment::SEGMENT_COLOR_VALUE_INVALID[1],
    vtkSegment::SEGMENT_COLOR_VALUE_INVALID[2], vtkSegment::SEGMENT_COLOR_VALUE_INVALID[3] ); // Color indicating invalid index

  // Set reference to color table node
  this->SetAndObserveColorNodeID(segmentationColorTableNode->GetID());

  return segmentationColorTableNode;
}

//---------------------------------------------------------------------------
char* vtkMRMLSegmentationDisplayNode::GetPolyDataDisplayRepresentationName()
{
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(this->GetDisplayableNode());

  bool polyDataRepresentationNeedsUpdate = (this->PolyDataDisplayRepresentationName == NULL);

  // Check if the currently set poly data representation (if any) exists in the segmentation
  if (!polyDataRepresentationNeedsUpdate)
  {
    if (!segmentationNode)
    {
      return NULL;
    }

    if (!segmentationNode->GetSegmentation()->ContainsRepresentation(this->PolyDataDisplayRepresentationName))
    {
      polyDataRepresentationNeedsUpdate = true;
    }
  }

  // Determine poly data representation if necessary
  if (polyDataRepresentationNeedsUpdate)
  {
    if (segmentationNode->GetSegmentation()->GetNumberOfSegments() == 0)
    {
      return NULL;
    }
    const char* name = this->DeterminePolyDataDisplayRepresentationName();
    if (!name)
    {
      return NULL;
    }
    this->SetPolyDataDisplayRepresentationName(name);
  }

  return this->PolyDataDisplayRepresentationName;
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationDisplayNode::SetSegmentColor(std::string segmentId, double r, double g, double b, double a/* = 1.0*/)
{
  // Set color in display properties
  SegmentDisplayPropertiesMap::iterator propsIt = this->SegmentationDisplayProperties.find(segmentId);
  if (propsIt != this->SegmentationDisplayProperties.end())
  {
    propsIt->second.Color[0] = r;
    propsIt->second.Color[1] = g;
    propsIt->second.Color[2] = b;
    propsIt->second.PolyDataOpacity = a;
  }

  // Set color in color table too
  this->SetSegmentColorTableEntry(segmentId, r, g, b, a);

  return true;
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationDisplayNode::SetSegmentColorTableEntry(std::string segmentId, double r, double g, double b, double a)
{
  // Set color in color table (for merged labelmap)
  vtkMRMLColorTableNode* colorTableNode = vtkMRMLColorTableNode::SafeDownCast(this->GetColorNode());
  if (!colorTableNode)
  {
    vtkErrorMacro("SetSegmentColorTableEntry: No color table node associated with segmentation. Maybe CreateColorTableNode was not called?");
    return false;
  }
  // Look up segment color in color table node (-1 if not found)
  int colorIndex = colorTableNode->GetColorIndexByName(segmentId.c_str());
  if (colorIndex < 0)
  {
    vtkWarningMacro("SetSegmentColorTableEntry: No color table entry found for segment " << segmentId);
    return false;
  }
  colorTableNode->SetColor(colorIndex, r, g, b, (this->EnableTransparencyInColorTable) ? a : (a>0.0 ? 1.0 : 0.0));
  return true;
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationDisplayNode::GetSegmentDisplayProperties(std::string segmentId, SegmentDisplayProperties &properties)
{
  SegmentDisplayPropertiesMap::iterator propsIt = this->SegmentationDisplayProperties.find(segmentId);
  if (propsIt == this->SegmentationDisplayProperties.end())
  {
    vtkErrorMacro("GetSegmentDisplayProperties: No display properties found for segment with ID " << segmentId);
    return false;
  }

  properties.Visible = propsIt->second.Visible;
  properties.Color[0] = propsIt->second.Color[0];
  properties.Color[1] = propsIt->second.Color[1];
  properties.Color[2] = propsIt->second.Color[2];
  properties.PolyDataOpacity = propsIt->second.PolyDataOpacity;

  return true;
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationDisplayNode::SetSegmentDisplayProperties(std::string segmentId, SegmentDisplayProperties &properties)
{
  SegmentDisplayPropertiesMap::iterator propsIt = this->SegmentationDisplayProperties.find(segmentId);
  if (propsIt == this->SegmentationDisplayProperties.end())
  {
    // If not found then add
    SegmentDisplayProperties newPropertiesEntry;
    newPropertiesEntry.Visible = properties.Visible;
    newPropertiesEntry.Color[0] = properties.Color[0];
    newPropertiesEntry.Color[1] = properties.Color[1];
    newPropertiesEntry.Color[2] = properties.Color[2];
    newPropertiesEntry.PolyDataOpacity = properties.PolyDataOpacity;
    this->SegmentationDisplayProperties[segmentId] = newPropertiesEntry;
  }
  else
  {
    // If found then replace values
    propsIt->second.Visible = properties.Visible;
    propsIt->second.Color[0] = properties.Color[0];
    propsIt->second.Color[1] = properties.Color[1];
    propsIt->second.Color[2] = properties.Color[2];
    propsIt->second.PolyDataOpacity = properties.PolyDataOpacity;
  }

  // Set color in color table too
  this->SetSegmentColorTableEntry(segmentId, properties.Color[0], properties.Color[1], properties.Color[2],
    (properties.Visible ? properties.PolyDataOpacity : 0.0) );

  this->Modified();
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationDisplayNode::GetSegmentVisibility(std::string segmentID)
{
  SegmentDisplayPropertiesMap::iterator propsIt = this->SegmentationDisplayProperties.find(segmentID);
  if (propsIt == this->SegmentationDisplayProperties.end())
  {
    vtkErrorMacro("GetSegmentVisibility: No display properties found for segment with ID " << segmentID);
    return false;
  }
  return propsIt->second.Visible;
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationDisplayNode::SetSegmentVisibility(std::string segmentID, bool visible)
{
  SegmentDisplayProperties properties;
  if (!this->GetSegmentDisplayProperties(segmentID, properties))
    {
    return;
    }
  properties.Visible = visible;
  this->SetSegmentDisplayProperties(segmentID, properties);
}

//---------------------------------------------------------------------------
vtkVector3d vtkMRMLSegmentationDisplayNode::GetSegmentColor(std::string segmentID)
{
  SegmentDisplayPropertiesMap::iterator propsIt = this->SegmentationDisplayProperties.find(segmentID);
  if (propsIt == this->SegmentationDisplayProperties.end())
  {
    vtkErrorMacro("GetSegmentColor: No display properties found for segment with ID " << segmentID);
    vtkVector3d color(vtkSegment::SEGMENT_COLOR_VALUE_INVALID[0], vtkSegment::SEGMENT_COLOR_VALUE_INVALID[1], vtkSegment::SEGMENT_COLOR_VALUE_INVALID[2]);
    return color;
  }

  vtkVector3d color(propsIt->second.Color[0], propsIt->second.Color[1], propsIt->second.Color[2]);
  return color;
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationDisplayNode::SetSegmentColor(std::string segmentID, vtkVector3d color)
{
  SegmentDisplayProperties properties;
  if (!this->GetSegmentDisplayProperties(segmentID, properties))
    {
    return;
    }
  properties.Color[0] = color.GetX();
  properties.Color[1] = color.GetY();
  properties.Color[2] = color.GetZ();
  this->SetSegmentDisplayProperties(segmentID, properties);
}

//---------------------------------------------------------------------------
double vtkMRMLSegmentationDisplayNode::GetSegmentPolyDataOpacity(std::string segmentID)
{
  SegmentDisplayPropertiesMap::iterator propsIt = this->SegmentationDisplayProperties.find(segmentID);
  if (propsIt == this->SegmentationDisplayProperties.end())
  {
    vtkErrorMacro("GetSegmentPolyDataOpacity: No display properties found for segment with ID " << segmentID);
    return 0.0;
  }
  return propsIt->second.PolyDataOpacity;
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationDisplayNode::SetSegmentPolyDataOpacity(std::string segmentID, double opacity)
{
  SegmentDisplayProperties properties;
  if (!this->GetSegmentDisplayProperties(segmentID, properties))
    {
    return;
    }
  properties.PolyDataOpacity = opacity;
  this->SetSegmentDisplayProperties(segmentID, properties);
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationDisplayNode::RemoveSegmentDisplayProperties(std::string segmentId)
{
  SegmentDisplayPropertiesMap::iterator propsIt = this->SegmentationDisplayProperties.find(segmentId);
  if (propsIt != this->SegmentationDisplayProperties.end())
  {
    this->SegmentationDisplayProperties.erase(propsIt);
  }

  this->Modified();
}


//---------------------------------------------------------------------------
void vtkMRMLSegmentationDisplayNode::ClearSegmentDisplayProperties()
{
  this->SegmentationDisplayProperties.clear();
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationDisplayNode::CalculateAutoOpacitiesForSegments()
{
  // Get segmentation node
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(this->GetDisplayableNode());
  if (!segmentationNode)
  {
    vtkErrorMacro("CalculateAutoOpacitiesForSegments: No segmentation node associated to this display node!");
    return false;
  }

  // Make sure the requested representation exists
  vtkSegmentation* segmentation = segmentationNode->GetSegmentation();
  if (!segmentation->CreateRepresentation(this->PolyDataDisplayRepresentationName))
  {
    return false;
  }

  // Determine poly data representation name if empty
  if (this->PolyDataDisplayRepresentationName == NULL)
  {
    this->DeterminePolyDataDisplayRepresentationName();
  }

  // Assemble segment poly datas into a collection that can be fed to topological hierarchy algorithm
  vtkSmartPointer<vtkPolyDataCollection> segmentPolyDataCollection = vtkSmartPointer<vtkPolyDataCollection>::New();
  for (SegmentDisplayPropertiesMap::iterator propIt = this->SegmentationDisplayProperties.begin();
    propIt != this->SegmentationDisplayProperties.end(); ++propIt)
  {
    // Get segment
    vtkSegment* currentSegment = segmentation->GetSegment(propIt->first);
    if (!currentSegment)
    {
      vtkErrorMacro("CalculateAutoOpacitiesForSegments: Mismatch in display properties and segments!");
      continue;
    }

    // Get poly data from segment
    vtkPolyData* currentPolyData = vtkPolyData::SafeDownCast(
      currentSegment->GetRepresentation(this->PolyDataDisplayRepresentationName) );
    if (!currentPolyData)
    {
      continue;
    }

    segmentPolyDataCollection->AddItem(currentPolyData);
  }
    

  // Set opacities according to topological hierarchy levels
  vtkSmartPointer<vtkTopologicalHierarchy> topologicalHierarchy = vtkSmartPointer<vtkTopologicalHierarchy>::New();
  topologicalHierarchy->SetInputPolyDataCollection(segmentPolyDataCollection);
  topologicalHierarchy->Update();
  vtkIntArray* levels = topologicalHierarchy->GetOutputLevels();

  // Determine number of levels
  int numberOfLevels = 0;
  for (int i=0; i<levels->GetNumberOfTuples(); ++i)
  {
    if (levels->GetValue(i) > numberOfLevels)
    {
      numberOfLevels = levels->GetValue(i);
    }
  }
  // Sanity check
  if (this->SegmentationDisplayProperties.size() != levels->GetNumberOfTuples())
  {
    vtkErrorMacro("CalculateAutoOpacitiesForSegments: Number of poly data colors does not match number of segment display properties!");
    return false;
  }

  // Set opacities into lookup table
  int idx = 0;
  SegmentDisplayPropertiesMap::iterator propIt;
  for (idx=0, propIt=this->SegmentationDisplayProperties.begin(); idx < levels->GetNumberOfTuples(); ++idx, ++propIt)
  {
    int level = levels->GetValue(idx);

    // The opacity level is set evenly distributed between 0 and 1 (excluding 0)
    // according to the topological hierarchy level of the segment
    double opacity = 1.0 - ((double)level) / (numberOfLevels+1);
    propIt->second.PolyDataOpacity = opacity;
  }

  return true;
}

//---------------------------------------------------------------------------
const char* vtkMRMLSegmentationDisplayNode::DeterminePolyDataDisplayRepresentationName()
{
  // Get segmentation node
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(this->GetDisplayableNode());
  if (!segmentationNode)
  {
    vtkErrorMacro("DeterminePolyDataDisplayRepresentationName: No segmentation node associated to this display node!");
    return vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName();
  }
  // If segmentation is empty then we cannot show poly data
  vtkSegmentation* segmentation = segmentationNode->GetSegmentation();
  if (!segmentation || segmentation->GetNumberOfSegments() == 0)
  {
    vtkWarningMacro("DeterminePolyDataDisplayRepresentationName: Empty segmentation!");
    return vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName();
  }

  // If master representation is poly data then use that
  // Assume the first segment contains the same name of representations as all segments (this should be the case by design)
  vtkSegment* firstSegment = segmentation->GetSegments().begin()->second;
  char* masterRepresentationName = segmentation->GetMasterRepresentationName();
  vtkDataObject* masterRepresentation = firstSegment->GetRepresentation(masterRepresentationName);
  if (vtkPolyData::SafeDownCast(masterRepresentation))
  {
    return masterRepresentationName;
  }

  // Otherwise return first poly data representation if any
  std::vector<std::string> containedRepresentationNames;
  segmentation->GetContainedRepresentationNames(containedRepresentationNames);
  for (std::vector<std::string>::iterator reprIt = containedRepresentationNames.begin();
    reprIt != containedRepresentationNames.end(); ++reprIt)
  {
    vtkDataObject* currentRepresentation = firstSegment->GetRepresentation(*reprIt);
    if (vtkPolyData::SafeDownCast(currentRepresentation))
    {
      return reprIt->c_str();
    }
  }
  
  // If no poly data representations are available, then return closed surface representation name,
  // it might be able to be created from other representations
  return vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName();
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationDisplayNode::GetPolyDataRepresentationNames(std::vector<std::string> &representationNames)
{
  representationNames.clear();

  // Get segmentation node
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(this->GetDisplayableNode());
  if (!segmentationNode)
  {
    vtkErrorMacro("GetPolyDataRepresentationNames: No segmentation node associated to this display node!");
    return;
  }
  // If segmentation is empty then we cannot show poly data
  vtkSegmentation* segmentation = segmentationNode->GetSegmentation();
  if (!segmentation || segmentation->GetNumberOfSegments() == 0)
  {
    vtkWarningMacro("GetPolyDataRepresentationNames: Empty segmentation!");
    return;
  }

  // Collect poly data representation names
  // Assume the first segment contains the same name of representations as all segments (this should be the case by design)
  vtkSegment* firstSegment = segmentation->GetSegments().begin()->second;
  std::vector<std::string> containedRepresentationNames;
  segmentation->GetContainedRepresentationNames(containedRepresentationNames);
  for (std::vector<std::string>::iterator reprIt = containedRepresentationNames.begin();
    reprIt != containedRepresentationNames.end(); ++reprIt)
  {
    vtkDataObject* currentRepresentation = firstSegment->GetRepresentation(*reprIt);
    if (vtkPolyData::SafeDownCast(currentRepresentation))
    {
      representationNames.push_back(*reprIt);
    }
  }
}