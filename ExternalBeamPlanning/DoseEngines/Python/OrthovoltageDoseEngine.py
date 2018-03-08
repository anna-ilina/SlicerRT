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
    "CT create", "PhaseSpaceFilePath", "Phase space filepath:", "Enter full path to phase space file",
    "Enter full path to phase space file") # 1st param is tab name

  def calculateDoseUsingEngine(self, beamNode, resultDoseVolumeNode):
    #import qSlicerExternalBeamPlanningDoseEnginesPythonQt as engines
    #orthovoltageEngine = engines.qSlicerMockDoseEngine()

    # Set parameter for C++ orthovoltage engine so that it is used with this beam node
    #orthovoltageEngine.setParameter( beamNode, "NoiseRange", self.scriptedEngine.doubleParameter(beamNode, "NoiseRange") )

    # Call C++ orthovoltage engine to calculate orthovoltage dose
    #return orthovoltageEngine.calculateDoseUsingEngine(beamNode, resultDoseVolumeNode)
