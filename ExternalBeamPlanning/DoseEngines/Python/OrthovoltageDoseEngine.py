import os
import vtk, qt, ctk, slicer
import logging
from DoseEngines import *

class OrthovoltageDoseEngine(AbstractScriptedDoseEngine):
  """ Orthovoltage python dose engine
  """

  def __init__(self, scriptedEngine):
    scriptedEngine.name = 'Orthovoltage python'
    AbstractScriptedDoseEngine.__init__(self, scriptedEngine)

  def defineBeamParameters(self):
    ##########################################
    # Generate ctcrate phantom parameters tab
    ##########################################

    self.scriptedEngine.addBeamParameterLineEdit(
    "Generate ctcreate phantom", "CtcreateOutputPath", "Ctcreate output filepath:", 
    "Enter filepath to store ctcreate phantom and related files", "C:\\\\d\\\\tmp") 

    self.scriptedEngine.addBeamParameterLineEdit(
    "Generate ctcreate phantom", "SeriesIUD", "Series IUD:", 
    "Enter the series IUD of the CT phantom being used (to get DICOM slicenames for ctcreate)", 
    "1.2.840.113704.1.111.3228.1498828556.10")

    self.scriptedEngine.addBeamParameterLineEdit(
    "Generate ctcreate phantom", "ROIName", "ROI node name for cropping image (optional):", 
    "Enter name of ROI, if you wish to crop CT image which will be converted to ctcreate phantom format. \
    If no ROI entered, will use image bounds from original CT image volume", "") 

    self.scriptedEngine.addBeamParameterLineEdit(
    "Generate ctcreate phantom", "SliceThicknessX", "Slice thickness (mm) in X direction (optional):", 
    "Enter desired slice thickness in X direction for output ctcreate phantom. If not x,y,z slice \
    thickness not provided, will use same thickness from original CT image volume", "") 

    self.scriptedEngine.addBeamParameterLineEdit(
    "Generate ctcreate phantom", "SliceThicknessY", "Slice thickness (mm) in Y direction (optional):", 
    "Enter desired slice thickness in Y direction for output ctcreate phantom. If not x,y,z slice \
    thickness not provided, will use same thickness from original CT image volume", "") 

    self.scriptedEngine.addBeamParameterLineEdit(
    "Generate ctcreate phantom", "SliceThicknessZ", "Slice thickness (mm) in Z direction (optional):", 
    "Enter desired slice thickness in Z direction for output ctcreate phantom. If not x,y,z slice \
    thickness not provided, will use same thickness from original CT image volume", "") 

    ##########################################
    # Orthovoltage dose parameters tab
    ##########################################

    self.scriptedEngine.addBeamParameterLineEdit(
    "Orthovoltage dose", "SimulationTitle", "Simulation title:", 
    "Enter a title for the DOSXYZNrc simulation", "DOSXYZnrc simulation")

    self.scriptedEngine.addBeamParameterLineEdit(
    "Orthovoltage dose", "PhaseSpaceFilePath", "Phase space filepath:", 
    "Enter full path to phase space file", "")

    self.scriptedEngine.addBeamParameterLineEdit(
    "Orthovoltage dose", "ECut", "Global electron cutoff energy - ECUT (MeV):", 
    "Select global electron cutoff energy (MeV)", "")

    self.scriptedEngine.addBeamParameterLineEdit(
    "Orthovoltage dose", "PCut", "Global photon cutoff energy - PCUT (MeV):", 
    "Select global electron cutoff energy (MeV)", "0.01")

    self.scriptedEngine.addBeamParameterLineEdit(
    "Orthovoltage dose", "IncidentBeamSize", "Incident beam size (cm):", 
    "SBEAM SIZE is the side of a square field in cm. The default value for \
    BEAM SIZE is 100 cm. When phase-space particles are read from a data file \
    or reconstructed by a multiple-source model, DOSXYZnrcwill check their \
    positions and discard those that fall outside of the specified field. \
    Use with caution.", "100")


  #todo: verify that all entered parameters are valid
  def calculateDoseUsingEngine(self, beamNode, resultDoseVolumeNode):
    import qSlicerExternalBeamPlanningDoseEnginesPythonQt as engines
    orthovoltageEngine = engines.qSlicerMockDoseEngine()

    ##########################################
    # Get ctcreate parameters
    ##########################################

    ctcreateOutputPath = self.scriptedEngine.parameter(beamNode, "CtcreateOutputPath")
    seriesIUD = self.scriptedEngine.parameter(beamNode, "SeriesIUD")
    ROIName = self.scriptedEngine.parameter(beamNode, "ROIName")
    volumeName = self.scriptedEngine.parameter(beamNode, "VolumeName")
    sliceThicknessX = self.scriptedEngine.parameter(beamNode, "SliceThicknessX")
    sliceThicknessY = self.scriptedEngine.parameter(beamNode, "SliceThicknessY")
    sliceThicknessZ = self.scriptedEngine.parameter(beamNode, "SliceThicknessZ")

    parentPlan = beamNode.GetParentPlanNode()
    volumeNode = parentPlan.GetReferenceVolumeNode()

    if ROIName == "":
      ROI = None
    else:
      try:
        ROI = slicer.util.getNode(ROIName)
      except:
        print("Unable to get specified ROI, check for typo")  #todo: this should be output to the Warnings log
        ROI = None

    if sliceThicknessX == "" or sliceThicknessY == "" or sliceThicknessZ == "":
      thicknesses = None
    else:
      thicknesses = [float(sliceThicknessX), float(sliceThicknessY), float(sliceThicknessZ)]

    ##########################################
    # Call ctcreate
    ##########################################

    generateCtcreateInput(volumeNode,seriesIUD, ctcreateOutputPath, ROI, thicknesses)
    callCtcreate(ctcreateOutputPath)

    ##########################################
    # Get DOSXYZnrc parameters
    ##########################################


    ##########################################
    # Call DOSXYZnrc
    ##########################################


    #Call C++ orthovoltage engine to calculate orthovoltage dose
    return orthovoltageEngine.calculateDoseUsingEngine(beamNode, resultDoseVolumeNode)
