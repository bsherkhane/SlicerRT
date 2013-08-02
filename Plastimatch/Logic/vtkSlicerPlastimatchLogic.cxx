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

#include "SlicerRtCommon.h"

// Plastimatch Logic includes
#include "vtkSlicerPlastimatchLogic.h"

// MRML includes
#include <vtkMRMLAnnotationFiducialNode.h>
#include <vtkMRMLScalarVolumeNode.h>
#include <vtkMRMLLinearTransformNode.h>

// STD includes
#include <string.h>

// ITK includes
#include <itkAffineTransform.h>
#include <itkArray.h>
#include <itkImageRegionIteratorWithIndex.h>

// VTK includes
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkMatrix4x4.h>

// Plastimatch includes
#include "bspline_interpolate.h"
#include "plm_config.h"
#include "plm_image_header.h"
#include "plm_warp.h"
#include "plmregister.h"
#include "pointset.h"
#include "pointset_warp.h"
#include "xform.h"
#include "raw_pointset.h"
#include "volume.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerPlastimatchLogic);

//----------------------------------------------------------------------------
vtkSlicerPlastimatchLogic::vtkSlicerPlastimatchLogic()
{
  this->FixedID=NULL;
  this->MovingID=NULL;
  this->FixedLandmarks=NULL;
  this->MovingLandmarks=NULL;
  this->FixedLandmarksFileName=NULL;
  this->MovingLandmarksFileName=NULL;
  this->WarpedLandmarks=NULL;
  this->RegistrationParameters=new Registration_parms();
  this->RegistrationData=new Registration_data();
  this->InputTransformationID=NULL;
  this->InputTransformation=NULL;
  this->OutputTransformation=NULL;
  this->OutputVectorField=NULL;
  this->WarpedImage=NULL;
  this->OutputVolumeID=NULL;
}

//----------------------------------------------------------------------------
vtkSlicerPlastimatchLogic::~vtkSlicerPlastimatchLogic()
{
  this->SetFixedID(NULL);
  this->SetMovingID(NULL);
  this->SetFixedLandmarks(NULL);
  this->SetMovingLandmarks(NULL);
  this->SetFixedLandmarksFileName(NULL);
  this->SetMovingLandmarksFileName(NULL);
  this->SetWarpedLandmarks(NULL);
  this->RegistrationParameters=NULL;
  this->RegistrationData=NULL;
  this->SetInputTransformationID(NULL);
  this->InputTransformation=NULL;
  this->OutputTransformation=NULL;
  this->OutputVectorField=NULL;
  this->WarpedImage=NULL;
  this->SetOutputVolumeID(NULL);
}

//----------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic::RegisterNodes()
{
  assert(this->GetMRMLScene() != 0);
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic::UpdateFromMRMLScene()
{
  assert(this->GetMRMLScene() != 0);
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
::OnMRMLSceneNodeAdded(vtkMRMLNode* vtkNotUsed(node))
{
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
::OnMRMLSceneNodeRemoved(vtkMRMLNode* vtkNotUsed(node))
{
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
:: AddStage()
{
  this->RegistrationParameters->append_stage();
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
::SetPar(char* key, char* value)
{        
  this->RegistrationParameters->set_key_val(key, value, 1);
}

//---------------------------------------------------------------------------
template<class T> 
static void 
itk_rectify_volume_hack (T image)
{
  typename T::ObjectType::RegionType rg = image->GetLargestPossibleRegion ();
  typename T::ObjectType::PointType og = image->GetOrigin();
  typename T::ObjectType::SpacingType sp = image->GetSpacing();
  typename T::ObjectType::SizeType sz = rg.GetSize();
  typename T::ObjectType::DirectionType dc = image->GetDirection();

  og[0] = og[0] - (sz[0] - 1) * sp[0];
  og[1] = og[1] - (sz[1] - 1) * sp[1];
  dc[0][0] = 1.;
  dc[1][1] = 1.;

  image->SetOrigin(og);
  image->SetDirection(dc);
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
::RunRegistration()
{
  // Set input images
  vtkMRMLVolumeNode* FixedVtkImage =
    vtkMRMLVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(GetFixedID()));
  itk::Image<float, 3>::Pointer FixedItkImage = itk::Image<float, 3>::New();
  SlicerRtCommon::ConvertVolumeNodeToItkImage<float>(FixedVtkImage, FixedItkImage);

  vtkMRMLVolumeNode* MovingVtkImage =
    vtkMRMLVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(GetMovingID()));
  itk::Image<float, 3>::Pointer MovingItkImage = itk::Image<float, 3>::New();
  SlicerRtCommon::ConvertVolumeNodeToItkImage<float>(MovingVtkImage, MovingItkImage);

  itk_rectify_volume_hack (FixedItkImage);
  itk_rectify_volume_hack (MovingItkImage);

  this->RegistrationData->fixed_image = new Plm_image (FixedItkImage);
  this->RegistrationData->moving_image = new Plm_image (MovingItkImage);
  
  // Set landmarks 
  bool landmarksSetted = false;
  if (GetFixedLandmarks() != NULL && GetMovingLandmarks() != NULL)
    {
    // From Slicer
    SetLandmarksFromSlicer();
    landmarksSetted = true;
    }
  else if (this->GetFixedLandmarksFileName() != NULL && this->GetFixedLandmarksFileName() != NULL)
    {
    // From Files
    SetLandmarksFromFiles();
    landmarksSetted = true;
    }
  
  // Set initial affine transformation
  if (GetInputTransformationID() != NULL)
    {
    ApplyInitialLinearTransformation(); 
    } 
  
  // Run registration and warp image
  do_registration_pure (&this->OutputTransformation, this->RegistrationData ,this->RegistrationParameters);
  this->WarpedImage=new Plm_image();
  ApplyWarp(this->WarpedImage, this->OutputVectorField, this->OutputTransformation,
    this->RegistrationData->fixed_image, this->RegistrationData->moving_image, -1200, 0, 1);
  GetOutputImage();

  // Warp landmarks
  if (landmarksSetted)
    {
    WarpLandmarks();
    }
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
::SetLandmarksFromSlicer()
{
  Labeled_pointset* fixedLandmarksSet = new Labeled_pointset();
  Labeled_pointset* movingLandmarksSet = new Labeled_pointset();
  
  for (int i = 0; i < this->FixedLandmarks->GetNumberOfPoints(); i++)
    {
    Labeled_point* fixedLandmark = new Labeled_point("point",
      - this->FixedLandmarks->GetPoint(i)[0],
      - this->FixedLandmarks->GetPoint(i)[1],
      this->FixedLandmarks->GetPoint(i)[2]);

    Labeled_point* movingLandmark = new Labeled_point("point",
      - this->MovingLandmarks->GetPoint(i)[0],
      - this->MovingLandmarks->GetPoint(i)[1],
      this->MovingLandmarks->GetPoint(i)[2]);
   
    fixedLandmarksSet->point_list.push_back(*fixedLandmark);
    movingLandmarksSet->point_list.push_back(*movingLandmark);
    }

  this->RegistrationData->fixed_landmarks = fixedLandmarksSet;
  this->RegistrationData->moving_landmarks = movingLandmarksSet;
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
::SetLandmarksFromFiles()
{
  Labeled_pointset* FixedLandmarksFromFile = new Labeled_pointset();
  FixedLandmarksFromFile->load(GetFixedLandmarksFileName());
  this->RegistrationData->fixed_landmarks = FixedLandmarksFromFile;

  Labeled_pointset* MovingLandmarksFromFile = new Labeled_pointset();
  MovingLandmarksFromFile->load(GetMovingLandmarksFileName());
  this->RegistrationData->moving_landmarks = MovingLandmarksFromFile;
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
::ApplyInitialLinearTransformation()
{
  // Get transformation as 4x4 matrix
  vtkMRMLLinearTransformNode* inputTransformation =
    vtkMRMLLinearTransformNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(GetInputTransformationID()));
  vtkMatrix4x4* inputVtkTransformation = inputTransformation->GetMatrixTransformToParent();

  // Create ITK array to store the parameters
  itk::Array<double> affineParameters;
  affineParameters.SetSize(12);

  // Set rotations
  int index=0;
  for (int i=0; i < 3; i++)
    {
    for (int j=0; j < 3; j++)
      {
      affineParameters.SetElement(index, inputVtkTransformation->GetElement(j,i));
      index++;
      }
    }

  // Set translations
  affineParameters.SetElement(9, inputVtkTransformation->GetElement(0,3));
  affineParameters.SetElement(10, inputVtkTransformation->GetElement(1,3));
  affineParameters.SetElement(11, inputVtkTransformation->GetElement(2,3));

  // Create ITK affine transformation
  itk::AffineTransform<double, 3>::Pointer inputItkTransformation =
    itk::AffineTransform<double, 3>::New();
  inputItkTransformation->SetParameters(affineParameters);

  // Set transformation
  this->InputTransformation = new Xform;
  this->InputTransformation->set_aff(inputItkTransformation);

  // Warp image using the input transformation
  Plm_image* outputImageFromInputTransformation = new Plm_image;
  ApplyWarp(outputImageFromInputTransformation, NULL, this->InputTransformation,
    this->RegistrationData->fixed_image, this->RegistrationData->moving_image, -1200, 0, 1);

  // Update moving image
  this->RegistrationData->moving_image=outputImageFromInputTransformation;
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
::ApplyWarp(Plm_image* warpedImage, DeformationFieldType::Pointer vectorFieldOut,
  Xform* inputTransformation, Plm_image* fixedImage, Plm_image* inputImage,
  float defaultValue, int useItk, int interpolationLinear)
{
  Plm_image_header* pih = new Plm_image_header(fixedImage);
  plm_warp(warpedImage, &vectorFieldOut, inputTransformation, pih, inputImage, defaultValue,
    useItk, interpolationLinear);
  this->OutputVectorField = vectorFieldOut;
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
::GetOutputImage ()
{
  itk::Image<float, 3>::Pointer OutputImageItk = this->WarpedImage->itk_float();    
  
  vtkSmartPointer<vtkImageData> OutputImageVtk = vtkSmartPointer<vtkImageData>::New();
  itk::Image<float, 3>::RegionType region = OutputImageItk->GetBufferedRegion();
  itk::Image<float, 3>::SizeType imageSize = region.GetSize();
  int extent[6]={0, (int) imageSize[0]-1, 0, (int) imageSize[1]-1, 0, (int) imageSize[2]-1};
  OutputImageVtk->SetExtent(extent);
  OutputImageVtk->SetScalarType(VTK_FLOAT);
  OutputImageVtk->SetNumberOfScalarComponents(1);
  OutputImageVtk->AllocateScalars();
  
  float* OutputImagePtr = (float*)OutputImageVtk->GetScalarPointer();
  itk::ImageRegionIteratorWithIndex< itk::Image<float, 3> > ItOutputImageItk(
  OutputImageItk, OutputImageItk->GetLargestPossibleRegion() );
  
  for ( ItOutputImageItk.GoToBegin(); !ItOutputImageItk.IsAtEnd(); ++ItOutputImageItk)
    {
    itk::Image<float, 3>::IndexType i = ItOutputImageItk.GetIndex();
    (*OutputImagePtr) = OutputImageItk->GetPixel(i);
    OutputImagePtr++;
    }
  
  // Read fixed image to get the geometrical information
  vtkMRMLVolumeNode* FixedVtkImage =
    vtkMRMLVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(GetFixedID()));
  
  // Create new image node
  vtkMRMLVolumeNode* WarpedImageNode =
    vtkMRMLVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(GetOutputVolumeID()));
  
  // Set warped image to a Slicer node
  WarpedImageNode->CopyOrientation(FixedVtkImage);
  WarpedImageNode->SetAndObserveImageData(OutputImageVtk);
}

//---------------------------------------------------------------------------
void vtkSlicerPlastimatchLogic
::WarpLandmarks()
{
  Labeled_pointset warpedPointset;
  pointset_warp (&warpedPointset, this->RegistrationData->moving_landmarks, this->OutputVectorField);
  
  this->WarpedLandmarks = vtkPoints::New();


  for (int i=0; i < (int) warpedPointset.count(); i++)
  {
    printf ("[RTN] %g %g %g -> %g %g %g\n",
            warpedPointset.point_list[i].p[0],
            warpedPointset.point_list[i].p[1],
            warpedPointset.point_list[i].p[2],
            - warpedPointset.point_list[i].p[0],
            - warpedPointset.point_list[i].p[1],
            warpedPointset.point_list[i].p[2]);
    this->WarpedLandmarks->InsertPoint(i,
      - warpedPointset.point_list[i].p[0],
      - warpedPointset.point_list[i].p[1],
      warpedPointset.point_list[i].p[2]);
    }
}

