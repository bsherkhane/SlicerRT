import os
import vtk, qt, ctk, slicer
import logging
from SegmentEditorEffects import *

class SegmentEditorDilateEffect(AbstractScriptedSegmentEditorMorphologyEffect):
  """ DilateEffect is an MorphologyEffect to
      dilate a layer of pixels from a segment
  """
  
  # Necessary static member to be able to set python source to scripted subject hierarchy plugin
  filePath = __file__

  def __init__(self, scriptedEffect):
    scriptedEffect.name = 'Dilate'
    AbstractScriptedSegmentEditorMorphologyEffect.__init__(self, scriptedEffect)

    # Effect-specific members
    self.drawPipelines = {}

  def clone(self):
    import qSlicerSegmentationsEditorEffectsPythonQt as effects
    clonedEffect = effects.qSlicerSegmentEditorScriptedMorphologyEffect(None)
    clonedEffect.setPythonSource(SegmentEditorDilateEffect.filePath)
    return clonedEffect

  def icon(self):
    iconPath = os.path.join(os.path.dirname(__file__), 'Resources/Icons/Dilate.png')
    if os.path.exists(iconPath):
      return qt.QIcon(iconPath)
    return qt.QIcon()
    
  def helpText(self):
    return "Use this tool to remove pixels from the boundary of the current label."
    
  def setupOptionsFrame(self):
    self.applyButton = qt.QPushButton("Apply")
    self.applyButton.objectName = self.__class__.__name__ + 'Apply'
    self.applyButton.setToolTip("Dilate selected segment")
    self.scriptedEffect.addOptionsWidget(self.applyButton)

    self.applyButton.connect('clicked()', self.onApply)

  def onApply(self):
    # Get parameters
    neighborMode = self.scriptedEffect.integerParameter("NeighborMode")
    iterations = self.scriptedEffect.integerParameter("Iterations")

    # Get edited labelmap
    editedLabelmap = self.scriptedEffect.parameterSetNode().GetEditedLabelmap()
    
    # Pad edited labelmap by as many voxels as iterations
    extent = editedLabelmap.GetExtent()
    paddedExtent = [extent[0]-iterations, extent[1]+iterations, extent[2]-iterations, extent[3]+iterations, extent[4]-iterations, extent[5]+iterations]
    padder = vtk.vtkImageConstantPad()
    padder.SetInputData(editedLabelmap)
    padder.SetOutputWholeExtent(paddedExtent)

    # Perform dilation (use erode filter and dilate background)
    eroder = slicer.vtkImageErode()
    eroder.SetInputConnection(padder.GetOutputPort())
    eroder.SetOutput(editedLabelmap)
    eroder.SetForeground(0) # Erode becomes dilate by switching the labels
    eroder.SetBackground(1)

    if neighborMode == 8:
      eroder.SetNeighborTo8()
    elif neighborMode == 4:
      eroder.SetNeighborTo4()
    else:
      logging.error("Invalid neighbor mode!")

    for i in xrange(iterations):
      eroder.Update()

    eroder.SetOutput(None)
    print('ZZZ ' + repr(editedLabelmap.GetExtent())) #TODO
    # Notify editor about changes.
    # This needs to be called so that the changes are written back to the edited segment
    self.scriptedEffect.apply()