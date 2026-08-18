#!/bin/bash -b

mkdir -p tmp

Run() {
	camNum=24
	camNumMinus1=$((camNum - 1))

	for i in $(seq 0 $camNumMinus1); do
		i0=$i

		i1=$((i+1))
		if [ "$i1" -ge "$camNum" ]; then
			i1=$((i1 - $camNum))
		fi

		i2=$((i+2))
		if [ "$i2" -ge "$camNum" ]; then
			i2=$((i2 - $camNum))
		fi

		n1=$(printf "%04d" $i0 )
		n2=$(printf "%04d" $i1 )
		n3=$(printf "%04d" $i2 )
		python Run_Estimate3CamPose.py --cam1_id=$i0 --cam2_id=$i1 --cam3_id=$i2 --img1 Synthetic_OctPrism/$n1".png" --img2 Synthetic_OctPrism/$n2".png" --img3 Synthetic_OctPrism/$n3".png" --result_feature_points_list_csv tmp/featurePoints2d_$n1"_"$n2"_"$n3".csv" --result_cam_pose_csv tmp/camPose_$n1"_"$n2"_"$n3".csv" --result_cam_pose_ply tmp/camPose_$n1"_"$n2"_"$n3".ply" --result_points3d_csv tmp/points3d_$n1"_"$n2"_"$n3".csv" --result_points3d_ply tmp/points3d_$n1"_"$n2"_"$n3".ply" --lowes_ratio 0.7 --ransac_threshold 35 --f0 800 --reproj_err_converge 0.001 --j_threshold 1.0  &

	done
	wait
}

Run
