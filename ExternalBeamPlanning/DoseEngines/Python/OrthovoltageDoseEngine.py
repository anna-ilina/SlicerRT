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
    self.scriptedEngine.addBeamParameterLineEdit(
    "Generate ctcreate phantom", "CtcreateOutputPath", "Ctcreate output filepath:", 
    "Enter filepath to store ctcreate phantom and related files", "") 

    self.scriptedEngine.addBeamParameterLineEdit(
    "Generate ctcreate phantom", "SeriesIUD", "Series IUD:", 
    "Enter the series IUD of the CT phantom being used (to get DICOM slicenames for ctcreate)", "") 

    self.scriptedEngine.addBeamParameterLineEdit(
    "Generate ctcreate phantom", "ROIName", "ROI name for cropping image (optional):", 
    "Enter name of ROI, if you wish to crop CT image which will be converted to ctcreate phantom format", "") 

    #Todo: add ROI selection box 

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



  #todo: verify that all entered are valid
  def calculateDoseUsingEngine(self, beamNode, resultDoseVolumeNode):
    import qSlicerExternalBeamPlanningDoseEnginesPythonQt as engines
    orthovoltageEngine = engines.qSlicerMockDoseEngine()

    # get ctcreate parameters
    ctcreateOutputPath = self.scriptedEngine.parameter(beamNode, "CtcreateOutputPath")
    seriesIUD = self.scriptedEngine.parameter(beamNode, "SeriesIUD")
    ROIName = self.scriptedEngine.parameter(beamNode, "ROIName")
    #todo: allow user to input voxel thickness (optional)

    if ROIName == "":
      ROI = None
    else:
      try:
        ROI = slicer.mrmlScene.GetNodeByID(ROIName)
      except:
        print("Unable to get specified ROI, check for typo")  #todo: this should be output to the Warnings log
        ROI = None
  

    v = None  #todo: get volume node from parent plan (needed as input to determine thickness)

    # Call ctcreate
    generateCtcreateInput(v,seriesIUD, ctCreateOutputPath, ROI)

    #Call C++ orthovoltage engine to calculate orthovoltage dose
    return orthovoltageEngine.calculateDoseUsingEngine(beamNode, resultDoseVolumeNode)
