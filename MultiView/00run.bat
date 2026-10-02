@echo off
setlocal EnableDelayedExpansion

set "PY=python.exe"
set "CAM_NUM=24"
set "IMG_DIR=Synthetic_OctPrism"
set "OUT_DIR=tmp"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

REM ---------------------------------------------------------------
REM Step 1: Run_Estimate3CamPose for 24 triplets (cyclic wrap-around).
REM ---------------------------------------------------------------
for /L %%i in (0,1,23) do (
    set /a i0=%%i
    set /a i1=%%i+1
    set /a i2=%%i+2
    if !i1! geq %CAM_NUM% set /a i1-= %CAM_NUM%
    if !i2! geq %CAM_NUM% set /a i2-= %CAM_NUM%

    call :pad !i0!
    set "n1=!num!"
    call :pad !i1!
    set "n2=!num!"
    call :pad !i2!
    set "n3=!num!"

    echo [Step1] triplet !n1!_!n2!_!n3!  i0=!i0! i1=!i1! i2=!i2!
    "%PY%" Run_Estimate3CamPose.py ^
        --cam1_id=!i0! --cam2_id=!i1! --cam3_id=!i2! ^
        --img1 "%IMG_DIR%\!n1!.png" ^
        --img2 "%IMG_DIR%\!n2!.png" ^
        --img3 "%IMG_DIR%\!n3!.png" ^
        --result_feature_points_list_csv "%OUT_DIR%\featurePoints2d_!n1!_!n2!_!n3!.csv" ^
        --result_cam_pose_csv "%OUT_DIR%\camPose_!n1!_!n2!_!n3!.csv" ^
        --result_cam_pose_ply "%OUT_DIR%\camPose_!n1!_!n2!_!n3!.ply" ^
        --result_points3d_csv "%OUT_DIR%\points3d_!n1!_!n2!_!n3!.csv" ^
        --result_points3d_ply "%OUT_DIR%\points3d_!n1!_!n2!_!n3!.ply" ^
        --lowes_ratio 0.7 --ransac_threshold 35 ^
        --shared_intrinsic ^
        --reproj_err_converge 0.001 --j_threshold 1.0

    if errorlevel 1 (
        echo ERROR: triplet !n1!_!n2!_!n3! failed
        exit /b 1
    )

REM    --focal_length 2667 ^

)

REM ---------------------------------------------------------------
REM Step 2: Run_MergeMultiCam (chain of 22 + loop closure of 2 wrap triplets).
REM ---------------------------------------------------------------
REM NOTE: escape > as ^> in echo, otherwise cmd treats it as redirection and
REM overwrites camPose_merged.csv with this echo text. (Keep this bat ASCII-only:
REM UTF-8 non-ASCII comments get misparsed under the system code page.)
echo [Step2] merge 22 chain triplets ^(+2 wrap) -^> %OUT_DIR%\camPose_merged.csv
"%PY%" Run_MergeMultiCam.py ^
    --dir %OUT_DIR% --prefix camPose --start 0 --cam_num 22 ^
    --out_cam_pose_csv "%OUT_DIR%\camPose_merged.csv" ^
    --out_cam_pose_ply "%OUT_DIR%\camPose_merged.ply" ^
    --loop_closure_csv "%OUT_DIR%\camPose_0022_0023_0000.csv" "%OUT_DIR%\camPose_0023_0000_0001.csv" ^
    --bundle_adjustment ^
    --out_cam_pose_ba_csv "%OUT_DIR%\camPose_ba.csv" ^
    --out_cam_pose_ba_ply "%OUT_DIR%\camPose_ba.ply" ^
    --out_points3d_ba_csv "%OUT_DIR%\points3d_ba.csv" ^
    --out_points3d_ba_ply "%OUT_DIR%\points3d_ba.ply" ^
    --shared_intrinsic ^
    --ba_pose_lambda 30.0

if errorlevel 1 (
    echo ERROR: Run_MergeMultiCam failed
    exit /b 1
)

REM    --focal_length 2667 ^

echo Done.
exit /b 0

REM ---------------------------------------------------------------
:pad
set "num=000%1"
set "num=%num:~-4%"
goto :eof
