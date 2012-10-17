/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

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

==============================================================================*/

#include "vtkLabelmapToModelFilter.h"

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkNew.h>
#include <vtkMarchingCubes.h>
#include <vtkDecimatePro.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkLabelmapToModelFilter);

//----------------------------------------------------------------------------
vtkLabelmapToModelFilter::vtkLabelmapToModelFilter()
{
  this->InputLabelmap = NULL;
  vtkSmartPointer<vtkImageData> inputLabelmap = vtkSmartPointer<vtkImageData>::New();
  this->SetInputLabelmap(inputLabelmap);

  this->OutputModel = NULL;
  vtkSmartPointer<vtkPolyData> outputModel = vtkSmartPointer<vtkPolyData>::New();
  this->SetOutputModel(outputModel);

  this->SetDecimateTargetReduction(0.0);
}

//----------------------------------------------------------------------------
vtkLabelmapToModelFilter::~vtkLabelmapToModelFilter()
{
  this->SetInputLabelmap(NULL);
  this->SetOutputModel(NULL);
}

//----------------------------------------------------------------------------
void vtkLabelmapToModelFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
vtkPolyData* vtkLabelmapToModelFilter::GetOutput()
{
  return this->OutputModel;
}

//----------------------------------------------------------------------------
void vtkLabelmapToModelFilter::Update()
{
  //this->OutputModel->ShallowCopy(imageCast->GetOutput());
} 