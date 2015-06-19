set(KIT qSlicer${MODULE_NAME}Module)

set(KIT_TEST_SRCS
  vtkSlicerSegmentComparisonModuleLogicTest1.cxx
  )

slicerMacroConfigureModuleCxxTestDriver(
  NAME ${KIT}
  SOURCES ${KIT_TEST_SRCS}
  TARGET_LIBRARIES vtkSlicerSegmentComparisonModuleLogic
  WITH_VTK_DEBUG_LEAKS_CHECK
  )

#-----------------------------------------------------------------------------
set(TEMP "${CMAKE_BINARY_DIR}/Testing/Temporary")

macro(TEST_WITH_DATA TestName TestExecutableName
      DataDirectoryPath InputContourReferenceFile InputContourCompareFile ReferenceVolumeFile
      TemporarySceneFile ApplySimpleTransformToInputCompare
      HausdorffMaximumMm HausdorffAverageMm Hausdorff95PercentMm
      DiceCoefficient TruePositivesPercent TrueNegativesPercent
      FalsePositivesPercent FalseNegativesPercent)
  add_test(
    NAME ${TestName}
    COMMAND ${Slicer_LAUNCH_COMMAND} $<TARGET_FILE:${KIT}CxxTests> ${TestExecutableName} ${ARGN}
    -DataDirectoryPath ${DataDirectoryPath}
    -InputContourReferenceFile ${InputContourReferenceFile}
    -InputContourCompareFile ${InputContourCompareFile}
    -ReferenceVolumeFile ${ReferenceVolumeFile}
    -TemporarySceneFile ${TemporarySceneFile}
    -ApplySimpleTransformToInputCompare ${ApplySimpleTransformToInputCompare}
    -HausdorffMaximumMm ${HausdorffMaximumMm}
    -HausdorffAverageMm ${HausdorffAverageMm}
    -Hausdorff95PercentMm ${Hausdorff95PercentMm}
    -DiceCoefficient ${DiceCoefficient}
    -TruePositivesPercent ${TruePositivesPercent}
    -TrueNegativesPercent ${TrueNegativesPercent}
    -FalsePositivesPercent ${FalsePositivesPercent}
    -FalseNegativesPercent ${FalseNegativesPercent}
  )
endmacro()

#-----------------------------------------------------------------------------
TEST_WITH_DATA(
  vtkSlicerSegmentComparisonModuleLogicTest_EclipseProstate_Base
  vtkSlicerSegmentComparisonModuleLogicTest1
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Testing/Data/
  EclipseProstate_Rectum.ctr
  EclipseProstate_Expanded_5_5_5_Rectum.ctr
  EclipseProstate_Dose.nrrd
  ${TEMP}/TestScene_SegmentComparison_EclipseProstate.mrml
  0
  7.1807
  5.22673
  6.3726
  0.520782
  0.182964
  99.48
  0.336723
  0.0
)
set_tests_properties(vtkSlicerSegmentComparisonModuleLogicTest_EclipseProstate_Base PROPERTIES FAIL_REGULAR_EXPRESSION "Error;ERROR;Warning;WARNING" )

#-----------------------------------------------------------------------------
TEST_WITH_DATA(
  vtkSlicerSegmentComparisonModuleLogicTest_EclipseProstate_SameInput
  vtkSlicerSegmentComparisonModuleLogicTest1
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Testing/Data/
  EclipseProstate_Rectum.ctr
  EclipseProstate_Rectum.ctr
  EclipseProstate_Dose.nrrd
  ${TEMP}/TestScene_SegmentComparison_EclipseProstate_SameInput.mrml
  0
  0.0
  0.0
  0.0
  1
  0.182964
  99.817
  0.0
  0.0
)
set_tests_properties(vtkSlicerSegmentComparisonModuleLogicTest_EclipseProstate_SameInput PROPERTIES FAIL_REGULAR_EXPRESSION "Error;ERROR;Warning;WARNING" )

#-----------------------------------------------------------------------------
TEST_WITH_DATA(
  vtkSlicerSegmentComparisonModuleLogicTest_EclipseProstate_Transformed
  vtkSlicerSegmentComparisonModuleLogicTest1
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../Testing/Data/
  EclipseProstate_Rectum.ctr
  EclipseProstate_Rectum.ctr
  EclipseProstate_Dose.nrrd
  ${TEMP}/TestScene_SegmentComparison_EclipseProstate_Transformed.mrml
  1
  5.00
  2.37532
  5
  0.693587
  0.126901
  99.761
  0.0560625
  0.0560625
)
set_tests_properties(vtkSlicerSegmentComparisonModuleLogicTest_EclipseProstate_Transformed PROPERTIES FAIL_REGULAR_EXPRESSION "Error;ERROR;Warning;WARNING" )