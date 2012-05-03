/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// ModuleTemplate includes
#include "vtkSlicerDoseVolumeHistogramLogic.h"

#include "vtkPolyDataToLabelmapFilter.h"

// MRML includes
#include <vtkMRMLScalarVolumeNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLChartNode.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLChartViewNode.h>
#include <vtkMRMLDoubleArrayNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLModelDisplayNode.h>
#include <vtkMRMLModelHierarchyNode.h>

// VTK includes
#include <vtkNew.h>
#include <vtkGeneralTransform.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkImageAccumulate.h>
#include <vtkImageThreshold.h>
#include <vtkImageToImageStencil.h>
#include <vtkDoubleArray.h>
#include <vtkStringArray.h>

// VTKSYS includes
#include <vtksys/SystemTools.hxx>

// STD includes
#include <cassert>
#include <set>

//----------------------------------------------------------------------------
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_TYPE_ATTRIBUTE_NAME = "Type";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_TYPE_ATTRIBUTE_VALUE = "DVH";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_DOSE_VOLUME_NODE_ID_ATTRIBUTE_NAME = "DoseVolumeNodeId";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_STRUCTURE_NAME_ATTRIBUTE_NAME = "StructureName";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_STRUCTURE_MODEL_NODE_ID_ATTRIBUTE_NAME = "StructureModelNodeId";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_STRUCTURE_COLOR_ATTRIBUTE_NAME = "StructureColor";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_STRUCTURE_PLOT_NAME_ATTRIBUTE_NAME = "DVH_StructurePlotName";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_METRIC_ATTRIBUTE_NAME_PREFIX = "DVH_Metric_";
const char vtkSlicerDoseVolumeHistogramLogic::DVH_METRIC_LIST_SEPARATOR_CHARACTER = '|';
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_METRIC_LIST_ATTRIBUTE_NAME = "List";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_METRIC_TOTAL_VOLUME_CC_ATTRIBUTE_NAME = "Total volume (cc)";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_METRIC_MEAN_DOSE_ATTRIBUTE_NAME_PREFIX = "Mean dose";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_METRIC_MAX_DOSE_ATTRIBUTE_NAME_PREFIX = "Max dose";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_METRIC_MIN_DOSE_ATTRIBUTE_NAME_PREFIX = "Min dose";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_METRIC_VOXEL_COUNT_ATTRIBUTE_NAME = "Voxel count";
const std::string vtkSlicerDoseVolumeHistogramLogic::DVH_METRIC_V_DOSE_ATTRIBUTE_NAME_PREFIX = "V";

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerDoseVolumeHistogramLogic);

vtkCxxSetObjectMacro(vtkSlicerDoseVolumeHistogramLogic, DoseVolumeNode, vtkMRMLVolumeNode);
vtkCxxSetObjectMacro(vtkSlicerDoseVolumeHistogramLogic, StructureSetModelNode, vtkMRMLNode);
vtkCxxSetObjectMacro(vtkSlicerDoseVolumeHistogramLogic, ChartNode, vtkMRMLChartNode);

//----------------------------------------------------------------------------
vtkSlicerDoseVolumeHistogramLogic::vtkSlicerDoseVolumeHistogramLogic()
{
  this->DoseVolumeNode = NULL;
  this->StructureSetModelNode = NULL;
  this->ChartNode = NULL;

  this->DvhDoubleArrayNodes = NULL;
  vtkSmartPointer<vtkCollection> dvhDoubleArrayNodes = vtkSmartPointer<vtkCollection>::New();
  this->SetDvhDoubleArrayNodes(dvhDoubleArrayNodes);

  this->SceneChangedOff();
}

//----------------------------------------------------------------------------
vtkSlicerDoseVolumeHistogramLogic::~vtkSlicerDoseVolumeHistogramLogic()
{
  this->SetDoseVolumeNode(NULL);
  this->SetStructureSetModelNode(NULL);
  this->SetChartNode(NULL);

  for (int i=0; i<DvhDoubleArrayNodes->GetNumberOfItems(); ++i)
  {
    vtkMRMLDoubleArrayNode* dvhNode = vtkMRMLDoubleArrayNode::SafeDownCast( DvhDoubleArrayNodes->GetItemAsObject(i) );
    dvhNode->Delete();
  }
  this->SetDvhDoubleArrayNodes(NULL);
}

//----------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  events->InsertNextValue(vtkMRMLScene::EndCloseEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic::RegisterNodes()
{
  assert(this->GetMRMLScene() != 0);
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic::RefreshDvhDoubleArrayNodesFromScene()
{
  this->DvhDoubleArrayNodes->RemoveAllItems();

  if (this->GetMRMLScene() == NULL || this->GetMRMLScene()->GetNumberOfNodesByClass("vtkMRMLDoubleArrayNode") < 1)
  {
    return;
  }

  this->GetMRMLScene()->InitTraversal();
  vtkMRMLNode *node = this->GetMRMLScene()->GetNextNodeByClass("vtkMRMLDoubleArrayNode");
  while (node != NULL)
  {
    vtkMRMLDoubleArrayNode* doubleArrayNode = vtkMRMLDoubleArrayNode::SafeDownCast(node);
    if (doubleArrayNode)
    {
      const char* type = doubleArrayNode->GetAttribute(DVH_TYPE_ATTRIBUTE_NAME.c_str());
      if (type != NULL && stricmp(type, DVH_TYPE_ATTRIBUTE_VALUE.c_str()) == 0)
      {
        this->DvhDoubleArrayNodes->AddItem(doubleArrayNode);
      }
    }

    node = this->GetMRMLScene()->GetNextNodeByClass("vtkMRMLDoubleArrayNode");
  }
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic::UpdateFromMRMLScene()
{
  this->SceneChangedOn();

  RefreshDvhDoubleArrayNodesFromScene();
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic
::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  if (!node || !this->GetMRMLScene())
  {
    return;
  }

  // if the scene is still updating, jump out
  if (this->GetMRMLScene()->IsBatchProcessing())
  {
    return;
  }

  if (node->IsA("vtkMRMLDoubleArrayNode"))
  {
    vtkMRMLDoubleArrayNode* doubleArrayNode = vtkMRMLDoubleArrayNode::SafeDownCast(node);
    if (doubleArrayNode)
    {
      const char* type = doubleArrayNode->GetAttribute(DVH_TYPE_ATTRIBUTE_NAME.c_str());
      if (type != NULL && stricmp(type, DVH_TYPE_ATTRIBUTE_VALUE.c_str()) == 0)
      {
        this->DvhDoubleArrayNodes->AddItem(doubleArrayNode);
      }
    }
  }

  this->SceneChangedOn();
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic
::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  if (!node || !this->GetMRMLScene())
  {
    return;
  }

  // if the scene is still updating, jump out
  if (this->GetMRMLScene()->IsBatchProcessing())
  {
    return;
  }

  if (node->IsA("vtkMRMLDoubleArrayNode"))
  {
    this->DvhDoubleArrayNodes->RemoveItem( vtkMRMLDoubleArrayNode::SafeDownCast(node) );

    // Remove the structure corresponding the deleted node from all charts
    const char* structurePlotName = node->GetAttribute(DVH_STRUCTURE_PLOT_NAME_ATTRIBUTE_NAME.c_str());

    this->GetMRMLScene()->InitTraversal();
    vtkMRMLNode *node = this->GetMRMLScene()->GetNextNodeByClass("vtkMRMLChartNode");
    while (node != NULL)
    {
      vtkMRMLChartNode* chartNode = vtkMRMLChartNode::SafeDownCast(node);
      if (chartNode)
      {
        chartNode->RemoveArray(structurePlotName);
      }

      node = this->GetMRMLScene()->GetNextNodeByClass("vtkMRMLDoubleArrayNode");
    }
  }

  this->SceneChangedOn();
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic
::GetStenciledDoseVolumeForStructure(vtkMRMLScalarVolumeNode* structureStenciledDoseVolumeNode, vtkMRMLModelNode* structureModelNode)
{
  // Create model to doseRas transform
  vtkSmartPointer<vtkGeneralTransform> modelToDoseRasTransform=vtkSmartPointer<vtkGeneralTransform>::New();
  vtkSmartPointer<vtkGeneralTransform> doseRasToWorldTransform=vtkSmartPointer<vtkGeneralTransform>::New();
  vtkMRMLTransformNode* modelTransformNode=structureModelNode->GetParentTransformNode();
  vtkMRMLTransformNode* doseTransformNode=this->DoseVolumeNode->GetParentTransformNode();

  if (doseTransformNode!=NULL)
  {
    // the dosemap is transformed
    doseTransformNode->GetTransformToWorld(doseRasToWorldTransform);    
    if (modelTransformNode!=NULL)
    {      
      modelToDoseRasTransform->PostMultiply(); // GetTransformToNode assumes PostMultiply
      modelTransformNode->GetTransformToNode(doseTransformNode,modelToDoseRasTransform);
    }
    else
    {
      // modelTransformNode is NULL => the transform will be computed for the world coordinate frame
      doseTransformNode->GetTransformToWorld(modelToDoseRasTransform);
      modelToDoseRasTransform->Inverse();
    }
  }
  else
  {
    // the dosemap is not transformed => modelToDoseRasTransformMatrix = modelToWorldTransformMatrix
    if (modelTransformNode!=NULL)
    {
      // the model is transformed
      modelTransformNode->GetTransformToWorld(modelToDoseRasTransform);
    }
    else
    {
      // neither the model nor the dosemap is transformed
      modelToDoseRasTransform->Identity();
    }
  }  

  // Create doseRas to doseIjk transform
  vtkSmartPointer<vtkMatrix4x4> doseRasToDoseIjkTransformMatrix=vtkSmartPointer<vtkMatrix4x4>::New();
  this->DoseVolumeNode->GetRASToIJKMatrix( doseRasToDoseIjkTransformMatrix );  
  
  // Create model to doseIjk transform
  vtkNew<vtkGeneralTransform> modelToDoseIjkTransform;
  modelToDoseIjkTransform->Concatenate(doseRasToDoseIjkTransformMatrix);
  modelToDoseIjkTransform->Concatenate(modelToDoseRasTransform);


  // Transform the model polydata to doseIjk coordinate frame (the labelmap image coordinate frame is doseIjk)
  vtkNew<vtkTransformPolyDataFilter> transformPolyDataModelToDoseIjkFilter;
  transformPolyDataModelToDoseIjkFilter->SetInput( structureModelNode->GetPolyData() );
  transformPolyDataModelToDoseIjkFilter->SetTransform(modelToDoseIjkTransform.GetPointer());

  // Convert model to labelmap
  vtkNew<vtkPolyDataToLabelmapFilter> polyDataToLabelmapFilter;
  transformPolyDataModelToDoseIjkFilter->Update();
  polyDataToLabelmapFilter->SetInputPolyData( transformPolyDataModelToDoseIjkFilter->GetOutput() );
  polyDataToLabelmapFilter->SetReferenceImage( this->DoseVolumeNode->GetImageData() );
  polyDataToLabelmapFilter->Update();

  // Create labelmap node
  structureStenciledDoseVolumeNode->CopyOrientation( this->DoseVolumeNode );
  structureStenciledDoseVolumeNode->SetAndObserveTransformNodeID( this->DoseVolumeNode->GetTransformNodeID() );
  std::string labelmapNodeName( this->StructureSetModelNode->GetName() );
  labelmapNodeName.append( "_Labelmap" );
  structureStenciledDoseVolumeNode->SetName( labelmapNodeName.c_str() );
  structureStenciledDoseVolumeNode->SetAndObserveImageData( polyDataToLabelmapFilter->GetOutput() );
  structureStenciledDoseVolumeNode->LabelMapOn();
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic::GetSelectedStructureModelNodes(std::vector<vtkMRMLModelNode*> &structureModelNodes)
{
  structureModelNodes.clear();

  if (this->StructureSetModelNode->IsA("vtkMRMLModelNode"))
  {
    vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(this->StructureSetModelNode);
    if (modelNode)
    {
      structureModelNodes.push_back(modelNode);
    }
  }
  else if (this->StructureSetModelNode->IsA("vtkMRMLModelHierarchyNode"))
  {
    vtkSmartPointer<vtkCollection> childModelNodes = vtkSmartPointer<vtkCollection>::New();
    vtkMRMLModelHierarchyNode::SafeDownCast(this->StructureSetModelNode)->GetChildrenModelNodes(childModelNodes);
    childModelNodes->InitTraversal();
    if (childModelNodes->GetNumberOfItems() < 1)
    {
      vtkErrorMacro("Error: Selected Structure Set hierarchy node has no children model nodes!");
      return;
    }
    
    for (int i=0; i<childModelNodes->GetNumberOfItems(); ++i)
    {
      vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(childModelNodes->GetItemAsObject(i));
      if (modelNode)
      {
        structureModelNodes.push_back(modelNode);
      }
    }
  }
  else
  {
    vtkErrorMacro("Error: Invalid node type for StructureSetModelNode!");
    return;
  }
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic::ComputeDvh()
{
  std::vector<vtkMRMLModelNode*> structureModelNodes;
  GetSelectedStructureModelNodes(structureModelNodes);

  for (std::vector<vtkMRMLModelNode*>::iterator it = structureModelNodes.begin(); it != structureModelNodes.end(); ++it)
  {
    vtkSmartPointer<vtkMRMLScalarVolumeNode> structureStenciledDoseVolumeNode = vtkSmartPointer<vtkMRMLScalarVolumeNode>::New();
    GetStenciledDoseVolumeForStructure(structureStenciledDoseVolumeNode, (*it));
    // this->GetMRMLScene()->AddNode( structureStenciledDoseVolumeNode ); // add the labelmap to the scene for testing/debugging

    ComputeDvh(structureStenciledDoseVolumeNode.GetPointer(), (*it));
  }
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic
::ComputeDvh(vtkMRMLScalarVolumeNode* structureStenciledDoseVolumeNode, vtkMRMLModelNode* structureModelNode)
{
  if (this->GetMRMLScene() == NULL)
  {
    vtkErrorMacro("No MRML scene found!");
    return;
  }

  double* structureStenciledDoseVolumeSpacing = structureStenciledDoseVolumeNode->GetSpacing();

  double cubicMMPerVoxel = structureStenciledDoseVolumeSpacing[0] * structureStenciledDoseVolumeSpacing[1] * structureStenciledDoseVolumeSpacing[2];
  double ccPerCubicMM = 0.001;

  // Get dose grid scaling and dose units
  const char* doseGridScalingString = this->DoseVolumeNode->GetAttribute("DoseUnitValue");
  double doseGridScaling = 1.0;
  if (doseGridScalingString!=NULL)
  {
    doseGridScaling = atof(doseGridScalingString);
  }
  else
  {
    vtkWarningMacro("Dose grid scaling attribute is not set for the selected dose volume. Assuming scaling = 1.");
  }

  const char* doseUnitName = this->DoseVolumeNode->GetAttribute("DoseUnitName");

  // Compute statistics
  vtkNew<vtkImageToImageStencil> stencil;
  stencil->SetInput(structureStenciledDoseVolumeNode->GetImageData());
  stencil->ThresholdByUpper(0.5 * doseGridScaling); // 0.5 to make sure that 1*doseGridScaling is larger or equal than the threshold

  vtkNew<vtkImageAccumulate> stat;
  stat->SetInput(structureStenciledDoseVolumeNode->GetImageData());
  stat->SetStencil(stencil->GetOutput());
  stat->Update();

  if (stat->GetVoxelCount() < 1)
  {
    vtkWarningMacro("No voxels in the structure. DVH computation aborted.");
    return;
  }

  // Create node and fill statistics
  vtkMRMLDoubleArrayNode* arrayNode = (vtkMRMLDoubleArrayNode*)( this->GetMRMLScene()->CreateNodeByClass("vtkMRMLDoubleArrayNode") );
  std::string dvhArrayNodeName = std::string(structureModelNode->GetName()) + "_DVH";
  arrayNode->SetName(dvhArrayNodeName.c_str());
  //arrayNode->HideFromEditorsOff();

  arrayNode->SetAttribute(DVH_TYPE_ATTRIBUTE_NAME.c_str(), DVH_TYPE_ATTRIBUTE_VALUE.c_str());
  arrayNode->SetAttribute(DVH_DOSE_VOLUME_NODE_ID_ATTRIBUTE_NAME.c_str(), this->DoseVolumeNode->GetID());
  arrayNode->SetAttribute(DVH_STRUCTURE_NAME_ATTRIBUTE_NAME.c_str(), structureModelNode->GetName());
  arrayNode->SetAttribute(DVH_STRUCTURE_MODEL_NODE_ID_ATTRIBUTE_NAME.c_str(), structureModelNode->GetID());

  char attributeValue[64];
  double* color = structureModelNode->GetDisplayNode()->GetColor();
  sprintf(attributeValue, "#%02X%02X%02X", (int)(color[0]*255.0+0.5), (int)(color[1]*255.0+0.5), (int)(color[2]*255.0+0.5));
  arrayNode->SetAttribute(DVH_STRUCTURE_COLOR_ATTRIBUTE_NAME.c_str(), attributeValue);

  char attributeName[64];
  std::ostringstream metricList;

  sprintf(attributeName, "%s%s", DVH_METRIC_ATTRIBUTE_NAME_PREFIX.c_str(), DVH_METRIC_TOTAL_VOLUME_CC_ATTRIBUTE_NAME.c_str());
  sprintf(attributeValue, "%g", stat->GetVoxelCount() * cubicMMPerVoxel * ccPerCubicMM);
  metricList << attributeName << DVH_METRIC_LIST_SEPARATOR_CHARACTER;
  arrayNode->SetAttribute(attributeName, attributeValue);

  sprintf(attributeName, "%s%s (%s)", DVH_METRIC_ATTRIBUTE_NAME_PREFIX.c_str(), DVH_METRIC_MEAN_DOSE_ATTRIBUTE_NAME_PREFIX.c_str(), doseUnitName);
  sprintf(attributeValue, "%g", stat->GetMean()[0]);
  metricList << attributeName << DVH_METRIC_LIST_SEPARATOR_CHARACTER;
  arrayNode->SetAttribute(attributeName, attributeValue);

  sprintf(attributeName, "%s%s (%s)", DVH_METRIC_ATTRIBUTE_NAME_PREFIX.c_str(), DVH_METRIC_MAX_DOSE_ATTRIBUTE_NAME_PREFIX.c_str(), doseUnitName);
  sprintf(attributeValue, "%g", stat->GetMax()[0]);
  metricList << attributeName << DVH_METRIC_LIST_SEPARATOR_CHARACTER;
  arrayNode->SetAttribute(attributeName, attributeValue);

  sprintf(attributeName, "%s%s (%s)", DVH_METRIC_ATTRIBUTE_NAME_PREFIX.c_str(), DVH_METRIC_MIN_DOSE_ATTRIBUTE_NAME_PREFIX.c_str(), doseUnitName);
  sprintf(attributeValue, "%g", stat->GetMin()[0]);
  metricList << attributeName << DVH_METRIC_LIST_SEPARATOR_CHARACTER;
  arrayNode->SetAttribute(attributeName, attributeValue);

  sprintf(attributeName, "%s%s", DVH_METRIC_ATTRIBUTE_NAME_PREFIX.c_str(), DVH_METRIC_LIST_ATTRIBUTE_NAME.c_str());
  arrayNode->SetAttribute(attributeName, metricList.str().c_str());

  arrayNode = vtkMRMLDoubleArrayNode::SafeDownCast( this->GetMRMLScene()->AddNode( arrayNode ) );

  // Create DVH plot values
  int numBins = 100;
  double rangeMin = stat->GetMin()[0];
  double rangeMax = stat->GetMax()[0];
  double spacing = (rangeMax - rangeMin) / (double)numBins;

  stat->SetComponentExtent(0,numBins-1,0,0,0,0);
  stat->SetComponentOrigin(rangeMin,0,0);
  stat->SetComponentSpacing(spacing,1,1);
  stat->Update();

  // We put a fixed point at (0.0, 100%), but only if there are only positive values in the histogram
  // Negative values can occur when the user requests histogram for an image, such as s CT volume.
  // In this case Intensity Volume Histogram is computed.
  bool insertPointAtOrigin=true;
  if (rangeMin<0)
  {
    vtkWarningMacro("There are negative values in the histogram. Probably the input is not a dose volume.");
    insertPointAtOrigin=false;
  }

  vtkDoubleArray* doubleArray = arrayNode->GetArray();
  doubleArray->SetNumberOfTuples( numBins + (insertPointAtOrigin?1:0) ); 

  int outputArrayIndex=0;

  if (insertPointAtOrigin)
  {
    // Add first fixed point at (0.0, 100%)
    doubleArray->SetComponent(outputArrayIndex, 0, 0.0);
    doubleArray->SetComponent(outputArrayIndex, 1, 100.0);
    doubleArray->SetComponent(outputArrayIndex, 2, 0);
    ++outputArrayIndex;
  }

  unsigned long totalVoxels = stat->GetVoxelCount();
  unsigned long voxelBelowDose = 0;
  for (int sampleIndex=0; sampleIndex<numBins; ++sampleIndex)
  {
    unsigned long voxelsInBin = stat->GetOutput()->GetScalarComponentAsDouble(sampleIndex,0,0,0);
    doubleArray->SetComponent( outputArrayIndex, 0, rangeMin+sampleIndex*spacing );
    doubleArray->SetComponent( outputArrayIndex, 1, (1.0-(double)voxelBelowDose/(double)totalVoxels)*100.0 );
    doubleArray->SetComponent( outputArrayIndex, 2, 0 );
    ++outputArrayIndex;
    voxelBelowDose += voxelsInBin;
  }
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic
::AddDvhToSelectedChart(const char* structurePlotName, const char* dvhArrayNodeId)
{
  if (this->ChartNode == NULL)
  {
    vtkErrorMacro("Error: no chart node is selected!");
    return;
  }

  vtkMRMLChartViewNode* chartViewNode = GetChartViewNode();
  if (chartViewNode == NULL)
  {
    vtkErrorMacro("Error: unable to get chart view node!");
    return;
  }

  // Set chart properties
  vtkMRMLChartNode* chartNode = this->ChartNode;
  chartViewNode->SetChartNodeID( chartNode->GetID() );

  std::string doseAxisName;
  std::string chartTitle;
  const char* doseUnitName=this->DoseVolumeNode->GetAttribute("DoseUnitName");
  if (doseUnitName!=NULL)
  {
    doseAxisName=std::string("Dose [")+doseUnitName+"]";
    chartTitle="Dose Volume Histogram";
  }
  else
  {
    doseAxisName="Intensity";
    chartTitle="Intensity Volume Histogram";
  }

  chartNode->SetProperty("default", "title", chartTitle.c_str());
  chartNode->SetProperty("default", "xAxisLabel", doseAxisName.c_str());
  chartNode->SetProperty("default", "yAxisLabel", "Fractional volume [%]");
  chartNode->SetProperty("default", "type", "Line");

  // Change plot line style if there is already a structure with the same name in the chart
  vtkMRMLDoubleArrayNode* dvhArrayNode = vtkMRMLDoubleArrayNode::SafeDownCast( this->GetMRMLScene()->GetNodeByID(dvhArrayNodeId) );
  if (dvhArrayNode == NULL)
  {
    vtkErrorMacro("Error: unable to get double array node!");
    return;
  }

  const char* structureNameToAdd = dvhArrayNode->GetAttribute(DVH_STRUCTURE_NAME_ATTRIBUTE_NAME.c_str());

  int numberOfStructuresWithSameName = 0;
  vtkStringArray* plotNames = chartNode->GetArrayNames();
  const char* color = NULL;
  for (int i=0; i<plotNames->GetNumberOfValues(); ++i)
  {
    vtkStdString plotName = plotNames->GetValue(i);
    vtkMRMLDoubleArrayNode* arrayNode = 
      vtkMRMLDoubleArrayNode::SafeDownCast( this->GetMRMLScene()->GetNodeByID( chartNode->GetArray(plotName) ) );
    if (arrayNode == NULL)
    {
      continue;
    }

    const char* structureNameInChart = arrayNode->GetAttribute(DVH_STRUCTURE_NAME_ATTRIBUTE_NAME.c_str());
    if (stricmp(structureNameInChart, structureNameToAdd) == 0)
    {
      numberOfStructuresWithSameName++;
      if (numberOfStructuresWithSameName == 1)
      {
        color = chartNode->GetProperty(plotName, "color");
      }
    }
  }

  // Add array to chart
  chartNode->AddArray( structurePlotName, dvhArrayNodeId );

  // Set plot properties according to the number of structures with the same name
  if (numberOfStructuresWithSameName % 3 == 1 && color)
  {
    chartNode->SetProperty(structurePlotName, "color", color);
    chartNode->SetProperty(structurePlotName, "showLines", "off");
    chartNode->SetProperty(structurePlotName, "showMarkers", "on");
  }
  else if (numberOfStructuresWithSameName % 3 == 2 && color)
  {
    chartNode->SetProperty(structurePlotName, "color", color);
    chartNode->SetProperty(structurePlotName, "showLines", "on");
    chartNode->SetProperty(structurePlotName, "showMarkers", "on");
  }
  else
  {
    chartNode->SetProperty(structurePlotName, "showLines", "on");
    chartNode->SetProperty(structurePlotName, "showMarkers", "off");
    color = dvhArrayNode->GetAttribute(DVH_STRUCTURE_COLOR_ATTRIBUTE_NAME.c_str());
    chartNode->SetProperty(structurePlotName, "color", color);
  }
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic
::RemoveDvhFromSelectedChart(const char* structureName)
{
  if (this->ChartNode == NULL)
  {
    vtkErrorMacro("Error: no chart node is selected!");
    return;
  }

  vtkMRMLChartViewNode* chartViewNode = GetChartViewNode();
  if (chartViewNode == NULL)
  {
    vtkErrorMacro("Error: unable to get chart view node!");
    return;
  }

  vtkMRMLChartNode* chartNode = this->ChartNode;
  chartViewNode->SetChartNodeID( chartNode->GetID() );
  chartNode->RemoveArray(structureName);
}

//---------------------------------------------------------------------------
vtkMRMLChartViewNode* vtkSlicerDoseVolumeHistogramLogic
::GetChartViewNode()
{
  vtkCollection* layoutNodes = this->GetMRMLScene()->GetNodesByClass("vtkMRMLLayoutNode");
  layoutNodes->InitTraversal();
  vtkObject* layoutNodeVtkObject = layoutNodes->GetNextItemAsObject();
  vtkMRMLLayoutNode* layoutNode = vtkMRMLLayoutNode::SafeDownCast(layoutNodeVtkObject);
  if (layoutNode == NULL)
  {
    vtkErrorMacro("Error: unable to get layout node!");
    return NULL;
  }
  layoutNode->SetViewArrangement( vtkMRMLLayoutNode::SlicerLayoutConventionalQuantitativeView );
  layoutNodes->Delete();
  
  vtkCollection* chartViewNodes = this->GetMRMLScene()->GetNodesByClass("vtkMRMLChartViewNode");
  chartViewNodes->InitTraversal();
  vtkMRMLChartViewNode* chartViewNode = vtkMRMLChartViewNode::SafeDownCast( chartViewNodes->GetNextItemAsObject() );
  if (chartViewNode == NULL)
  {
    vtkErrorMacro("Error: unable to get chart view node!");
    return NULL;
  }
  chartViewNodes->Delete();

  return chartViewNode;
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic
::ComputeVMetrics(vtkMRMLDoubleArrayNode* dvhArrayNode, std::vector<double> doseValues, std::vector<double> &vMetricsCc, std::vector<double> &vMetricsPercent)
{
  vMetricsCc.clear();
  vMetricsPercent.clear();

  // Get structure volume
  char attributeName[64];
  sprintf(attributeName, "%s%s", DVH_METRIC_ATTRIBUTE_NAME_PREFIX.c_str(), DVH_METRIC_TOTAL_VOLUME_CC_ATTRIBUTE_NAME.c_str());
  const char* structureVolumeStr = dvhArrayNode->GetAttribute(attributeName);
  if (!structureVolumeStr)
  {
    vtkErrorMacro("Error: Failed to get total volume attribute from DVH double array MRML node!");
    return;
  }

  double structureVolume = atof(structureVolumeStr);
  if (structureVolume == 0.0)
  {
    vtkErrorMacro("Error: Failed to parse structure total volume attribute value!");
    return;
  }

  // Compute volume for all V's
  vtkDoubleArray* doubleArray = dvhArrayNode->GetArray();
  for (std::vector<double>::iterator it = doseValues.begin(); it != doseValues.end(); ++it)
  {
    double doseValue = (*it);

    // Check if the given dose is below the lowest value in the array then assign the whole volume of the structure
    if (doseValue < doubleArray->GetComponent(0, 0))
    {
      vMetricsCc.push_back(structureVolume);
      vMetricsPercent.push_back(100.0);
      continue;
    }

    // If dose is above the highest value in the array then assign 0 Cc volume
    if (doseValue >= doubleArray->GetComponent(doubleArray->GetNumberOfTuples()-1, 0))
    {
      vMetricsCc.push_back(0.0);
      vMetricsPercent.push_back(0.0);
      continue;
    }

    // Look for the dose in the array
    for (int i=0; i<doubleArray->GetNumberOfTuples()-1; ++i)
    {
      double doseBelow = doubleArray->GetComponent(i, 0);
      double doseAbove = doubleArray->GetComponent(i+1, 0);
      if (doseBelow <= doseValue && doseValue < doseAbove)
      {
        // Compute the volume percent using linear interpolation
        double volumePercentBelow = doubleArray->GetComponent(i, 1);
        double volumePercentAbove = doubleArray->GetComponent(i+1, 1);
        double volumePercentEstimated = volumePercentBelow + (volumePercentAbove-volumePercentBelow)*(doseValue-doseBelow)/(doseAbove-doseBelow);
        vMetricsCc.push_back( volumePercentEstimated*structureVolume/100.0 );
        vMetricsPercent.push_back( volumePercentEstimated );

        break;
      }
    }
  }
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic
::ComputeDMetrics(vtkMRMLDoubleArrayNode* dvhArrayNode, std::vector<double> volumeSizes, std::vector<double> &dMetrics)
{
  dMetrics.clear();

  // Get structure volume
  char attributeName[64];
  sprintf(attributeName, "%s%s", DVH_METRIC_ATTRIBUTE_NAME_PREFIX.c_str(), DVH_METRIC_TOTAL_VOLUME_CC_ATTRIBUTE_NAME.c_str());
  const char* structureVolumeStr = dvhArrayNode->GetAttribute(attributeName);
  if (!structureVolumeStr)
  {
    vtkErrorMacro("Error: Failed to get total volume attribute from DVH double array MRML node!");
    return;
  }

  double structureVolume = atof(structureVolumeStr);
  if (structureVolume == 0.0)
  {
    vtkErrorMacro("Error: Failed to parse structure total volume attribute value!");
    return;
  }

  // Compute volume for all D's
  vtkDoubleArray* doubleArray = dvhArrayNode->GetArray();
  double maximumDose = 0.0;
  for (int d=-1; d<(int)volumeSizes.size(); ++d)
  {
    double volumeSize = 0.0;
    double doseForVolume = 0.0;

    // First we get the maximum dose (D0.1cc)
    if (d == -1)
    {
      volumeSize = 0.1;
    }
    else
    {
      volumeSize = volumeSizes[d];
    }

    // Check if the given volume is above the highest (first) in the array then assign no dose
    if (volumeSize >= doubleArray->GetComponent(0, 1) / 100.0 * structureVolume)
    {
      doseForVolume = 0.0;
    }
    // If volume is below the lowest (last) in the array then assign maximum dose
    else if (volumeSize < doubleArray->GetComponent(doubleArray->GetNumberOfTuples()-1, 1) / 100.0 * structureVolume)
    {
      doseForVolume = doubleArray->GetComponent(doubleArray->GetNumberOfTuples()-1, 0);
    }
    else
    {
      for (int i=0; i<doubleArray->GetNumberOfTuples()-1; ++i)
      {
        double volumePrevious = doubleArray->GetComponent(i, 1) / 100.0 * structureVolume;
        double volumeNext = doubleArray->GetComponent(i+1, 1) / 100.0 * structureVolume;
        if (volumePrevious > volumeSize && volumeSize >= volumeNext)
        {
          // Compute the dose using linear interpolation
          double dosePrevious = doubleArray->GetComponent(i, 0);
          double doseNext = doubleArray->GetComponent(i+1, 0);
          double doseEstimated = dosePrevious + (doseNext-dosePrevious)*(volumeSize-volumePrevious)/(volumeNext-volumePrevious);
          doseForVolume = doseEstimated;

          break;
        }
      }
    }

    // Set found dose
    if (d == -1)
    {
      maximumDose = doseForVolume;
    }
    else
    {
      dMetrics.push_back( maximumDose - doseForVolume );
    }
  }
}

//---------------------------------------------------------------------------
bool vtkSlicerDoseVolumeHistogramLogic
::DoseVolumeContainsDose()
{
  const char* doseUnitName = this->DoseVolumeNode->GetAttribute("DoseUnitName");

  if (doseUnitName != NULL)
  {
    return true;
  }

  return false;
}

//---------------------------------------------------------------------------
void vtkSlicerDoseVolumeHistogramLogic
::CollectMetricsForDvhNodes(vtkCollection* dvhNodeCollection, std::vector<std::string> &metricList)
{
  metricList.clear();

  dvhNodeCollection->InitTraversal();
  if (dvhNodeCollection->GetNumberOfItems() < 1)
  {
    return;
  }

  // Convert separator character to string
  std::ostringstream separatorCharStream;
  separatorCharStream << DVH_METRIC_LIST_SEPARATOR_CHARACTER;
  std::string separatorCharacter = separatorCharStream.str();

  // Collect metrics
  char metricListAttributeName[64];
  sprintf(metricListAttributeName, "%s%s", DVH_METRIC_ATTRIBUTE_NAME_PREFIX.c_str(), DVH_METRIC_LIST_ATTRIBUTE_NAME.c_str());
  std::set<std::string> metricSet;
  for (int i=0; i<dvhNodeCollection->GetNumberOfItems(); ++i)
  {
    vtkMRMLDoubleArrayNode* dvhNode = vtkMRMLDoubleArrayNode::SafeDownCast( dvhNodeCollection->GetItemAsObject(i) );
    if (!dvhNode)
    {
      continue;
    }

    std::string metricListString = dvhNode->GetAttribute(metricListAttributeName);
    if (metricListString.empty())
    {
      continue;
    }

    // Split metric list string into set of metric strings
    size_t separatorPosition = metricListString.find( separatorCharacter );
    while (separatorPosition != std::string::npos)
    {
      metricSet.insert( metricListString.substr(0, separatorPosition) );
      metricListString = metricListString.substr(separatorPosition+1);
      separatorPosition = metricListString.find( separatorCharacter );
    }
    if (! metricListString.empty() )
    {
      metricSet.insert( metricListString );
    }
  }

  // Create an ordered list from the set
  const char* metricSearchList[4] = {"volume", "mean", "min", "max"};
  for (int i=0; i<4; ++i)
  {
    for (std::set<std::string>::iterator it = metricSet.begin(); it != metricSet.end(); ++it)
    {
      if (vtksys::SystemTools::LowerCase(*it).find(metricSearchList[i]) != std::string::npos)
      {
        metricList.push_back(*it);
        metricSet.erase(it);
        break;
      }
    }
  }

  // Append all other metrics in undefined order
  for (std::set<std::string>::iterator it = metricSet.begin(); it != metricSet.end(); ++it)
  {
    metricList.push_back(*it);
  }
}

//---------------------------------------------------------------------------
bool vtkSlicerDoseVolumeHistogramLogic
::ExportDvhToCsv(const char* fileName, bool comma/*=true*/)
{
  if (this->GetMRMLScene() == NULL)
  {
    vtkErrorMacro("Error: No MRML scene is present!");
		return false;
  }

  if (this->ChartNode == NULL)
  {
    vtkErrorMacro("Error: Chart node is not set!");
		return false;
  }

  // Open output file
  std::ofstream outfile;
  outfile.open(fileName);

	if ( !outfile )
	{
    vtkErrorMacro("Error: Output file '" << fileName << "' cannot be opened!");
		return false;
	}

  vtkStringArray* structureNames = this->ChartNode->GetArrayNames();
  vtkStringArray* arrayIDs = this->ChartNode->GetArrays();

  // Check if the number of values is the same in each structure
  int numberOfValues = -1;
	for (int i=0; i<arrayIDs->GetNumberOfValues(); ++i)
	{
    vtkMRMLNode *node = this->GetMRMLScene()->GetNodeByID( arrayIDs->GetValue(i) );
    vtkMRMLDoubleArrayNode* doubleArrayNode = vtkMRMLDoubleArrayNode::SafeDownCast(node);
    if (doubleArrayNode)
    {
      if (numberOfValues == -1)
      {
        numberOfValues = doubleArrayNode->GetArray()->GetNumberOfTuples();
        int numberOfComponents = doubleArrayNode->GetArray()->GetNumberOfComponents();
      }
      else if (numberOfValues != doubleArrayNode->GetArray()->GetNumberOfTuples())
      {
        vtkErrorMacro("Inconsistent number of values in the DVH arrays!");
        return false;
      }
    }
    else
    {
      vtkErrorMacro("Invalid double array node in selected chart!");
      return false;
    }
  }

  // Write header
  for (int i=0; i<structureNames->GetNumberOfValues(); ++i)
  {
  	outfile << structureNames->GetValue(i).c_str() << " Dose (Gy)" << (comma ? "," : "\t");
    outfile << structureNames->GetValue(i).c_str() << " Value (%)" << (comma ? "," : "\t");
  }
	outfile << std::endl;

  // Write values
	for (int row=0; row<numberOfValues; ++row)
  {
	  for (int column=0; column<arrayIDs->GetNumberOfValues(); ++column)
	  {
      vtkMRMLNode *node = this->GetMRMLScene()->GetNodeByID( arrayIDs->GetValue(column) );
      vtkMRMLDoubleArrayNode* doubleArrayNode = vtkMRMLDoubleArrayNode::SafeDownCast(node);

    	std::ostringstream doseStringStream;
			doseStringStream << std::fixed << std::setprecision(6) << doubleArrayNode->GetArray()->GetComponent(row, 0);
      std::string dose = doseStringStream.str();
      if (!comma)
      {
        size_t periodPosition = dose.find(".");
        if (periodPosition != std::string::npos)
        {
          dose.replace(periodPosition, 1, ",");
        }
      }
      outfile << dose << (comma ? "," : "\t");

    	std::ostringstream valueStringStream;
			valueStringStream << std::fixed << std::setprecision(6) << doubleArrayNode->GetArray()->GetComponent(row, 1);
      std::string value = valueStringStream.str();
      if (!comma)
      {
        size_t periodPosition = value.find(".");
        if (periodPosition != std::string::npos)
        {
          value.replace(periodPosition, 1, ",");
        }
      }
      outfile << value << (comma ? "," : "\t");
    }
		outfile << std::endl;
  }

	outfile.close();

  return true;
}

//---------------------------------------------------------------------------
bool vtkSlicerDoseVolumeHistogramLogic
::ExportDvhMetricsToCsv(const char* fileName,
                        std::vector<double> vDoseValuesCc,
                        std::vector<double> vDoseValuesPercent,
                        std::vector<double> dVolumeValues,
                        bool comma/*=true*/)
{
  if (this->GetMRMLScene() == NULL)
  {
    vtkErrorMacro("Error: No MRML scene is present!");
		return false;
  }

  // Open output file
  std::ofstream outfile;
  outfile.open(fileName);

	if ( !outfile )
	{
    vtkErrorMacro("Error: Output file '" << fileName << "' cannot be opened!");
		return false;
	}

  // Collect metrics for all included nodes
  std::vector<std::string> metricList;
  CollectMetricsForDvhNodes(this->DvhDoubleArrayNodes, metricList);

  // Write header
  outfile << "Structure" << (comma ? "," : "\t");
  for (std::vector<std::string>::iterator it = metricList.begin(); it != metricList.end(); ++it)
  {
    outfile << it->substr(DVH_METRIC_ATTRIBUTE_NAME_PREFIX.size()) << (comma ? "," : "\t");
  }
  for (std::vector<double>::iterator it = vDoseValuesCc.begin(); it != vDoseValuesCc.end(); ++it)
  {
    outfile << "V" << (*it) << " (cc)" << (comma ? "," : "\t");
  }
  for (std::vector<double>::iterator it = vDoseValuesPercent.begin(); it != vDoseValuesPercent.end(); ++it)
  {
    outfile << "V" << (*it) << " (%)" << (comma ? "," : "\t");
  }
  for (std::vector<double>::iterator it = dVolumeValues.begin(); it != dVolumeValues.end(); ++it)
  {
    outfile << "D" << (*it) << "cc (Gy)" << (comma ? "," : "\t");
  }
  outfile << std::endl;

  outfile.setf(std::ostream::fixed);
  outfile.precision(6);

  // Fill the table
  for (int i=0; i<this->DvhDoubleArrayNodes->GetNumberOfItems(); ++i)
  {
    vtkMRMLDoubleArrayNode* dvhNode = vtkMRMLDoubleArrayNode::SafeDownCast( this->DvhDoubleArrayNodes->GetItemAsObject(i) );
    if (!dvhNode)
    {
      continue;
    }

    outfile << dvhNode->GetAttribute(DVH_STRUCTURE_NAME_ATTRIBUTE_NAME.c_str()) << (comma ? "," : "\t");

    // Add default metric values
    for (std::vector<std::string>::iterator it = metricList.begin(); it != metricList.end(); ++it)
    {
      std::string metricValue( dvhNode->GetAttribute( it->c_str() ) );
      if (metricValue.empty())
      {
        outfile << (comma ? "," : "\t");
        continue;
      }

      outfile << metricValue << (comma ? "," : "\t");
    }

    // Add V metric values
    std::vector<double> dummy;
    if (vDoseValuesCc.size() > 0)
    {
      std::vector<double> volumes;
      ComputeVMetrics(dvhNode, vDoseValuesCc, volumes, dummy);
      for (std::vector<double>::iterator it = volumes.begin(); it != volumes.end(); ++it)
      {
        outfile << (*it) << (comma ? "," : "\t");
      }
    }
    if (vDoseValuesPercent.size() > 0)
    {
      std::vector<double> percents;
      ComputeVMetrics(dvhNode, vDoseValuesPercent, dummy, percents);
      for (std::vector<double>::iterator it = percents.begin(); it != percents.end(); ++it)
      {
        outfile << (*it) << (comma ? "," : "\t");
      }
    }

    // Add D metric values
    if (dVolumeValues.size() > 0)
    {
      std::vector<double> doses;
      ComputeDMetrics(dvhNode, dVolumeValues, doses);
      for (std::vector<double>::iterator it = doses.begin(); it != doses.end(); ++it)
      {
        outfile << (*it) << (comma ? "," : "\t");
      }
    }

    outfile << std::endl;
  }

	outfile.close();

  return true;
}